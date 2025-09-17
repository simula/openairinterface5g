/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

#include "nr_refsig.h"
#include "openair1/PHY/LTE_TRANSPORT/transport_proto.h" // for lte_gold_generic()

#define REFRESH_RATE (1000 * 100)

typedef struct {
  int key;
  int length;
  int usage;
} gold_cache_t;

typedef struct {
  uint32_t *table;
  uint32_t tblSz;
  int calls;
  int iterate;
} gold_cache_table_t;
static const int roundedHeaderSz = (((sizeof(gold_cache_t) + 63) / 64) * 64) / sizeof(uint32_t);
static const int grain = 64 / sizeof(uint32_t);

// Allocate, also reorder to have the most frequent first, so the cache search is optimized
static void refresh_table(gold_cache_table_t *t, int sizeIncrease)
{
  uint32_t *old = t->table;
  uint oldSz = t->tblSz;
  if (t->tblSz == 0)
    t->tblSz = PAGE_SIZE / sizeof(*t->table);
  if (sizeIncrease)
    t->tblSz += max(sizeIncrease, PAGE_SIZE / sizeof(*t->table));
  int ret = posix_memalign((void **)&t->table, 64, t->tblSz * sizeof(*t->table));
  AssertFatal(ret == 0, "No more memory");
  LOG_D(PHY,
        "re-organize gold sequence table to %lu pages of memory calls since last reorder: %d, search rate: %f\n",
        t->tblSz * sizeof(*t->table) / PAGE_SIZE,
        t->calls,
        t->calls ? t->iterate / (float)t->calls : 0.0);
  int maxUsage;
  uint32_t *currentTmp = t->table;
  do {
    maxUsage = 0;
    gold_cache_t *entryToCopy = NULL;
    for (uint32_t *searchmax = old; searchmax < old + oldSz; searchmax += roundedHeaderSz) {
      gold_cache_t *tbl = (gold_cache_t *)searchmax;
      if (!tbl->length)
        break;
      if (tbl->usage > maxUsage) {
        maxUsage = tbl->usage;
        entryToCopy = tbl;
      }
      searchmax += tbl->length;
    }
    if (maxUsage) {
      memcpy(currentTmp, entryToCopy, (roundedHeaderSz + entryToCopy->length) * sizeof(*t->table));
      currentTmp += roundedHeaderSz + entryToCopy->length;
      entryToCopy->usage = 0;
    }
  } while (maxUsage);
  const uint usedSz = currentTmp - t->table;
  memset(t->table + usedSz, 0, (t->tblSz - usedSz) * sizeof(*t->table));
  free(old);
  t->calls = 0;
  t->iterate = 0;
  return;
}

static pthread_key_t gold_table_key;
static pthread_once_t gold_key_once = PTHREAD_ONCE_INIT;

static void delete_table(void *ptr)
{
  gold_cache_table_t *table = (gold_cache_table_t *)ptr;
  if (table->table)
    free(table->table);
  free(ptr);
}

static void make_table_key()
{
  (void)pthread_key_create(&gold_table_key, delete_table);
}

uint32_t *gold_cache(uint32_t key, int length)
{
  (void)pthread_once(&gold_key_once, make_table_key);
  gold_cache_table_t *tableCache;
  if ((tableCache = pthread_getspecific(gold_table_key)) == NULL) {
    tableCache = calloc(1, sizeof(gold_cache_table_t));
    (void)pthread_setspecific(gold_table_key, tableCache);
  }

  // align for AVX512
  length = ((length + grain - 1) / grain) * grain;
  tableCache->calls++;

  // periodic refresh
  if (tableCache->calls > REFRESH_RATE)
    refresh_table(tableCache, 0);

  uint32_t *ptr = tableCache->table;
  // check if already cached
  for (; ptr < tableCache->table + tableCache->tblSz; ptr += roundedHeaderSz) {
    gold_cache_t *tbl = (gold_cache_t *)ptr;
    tableCache->iterate++;
    if (tbl->length >= length && tbl->key == key) {
      tbl->usage++;
      return ptr + roundedHeaderSz;
    }
    if (tbl->key == key) {
      // We use a longer sequence, same key
      // let's delete the shorter and force reorganize
      tbl->usage = 0;
      tableCache->calls += REFRESH_RATE;
    }
    if (!tbl->length)
      break;
    ptr += tbl->length;
  }

  // not enough space in the table
  if (!ptr || ptr > tableCache->table + tableCache->tblSz - (2 * roundedHeaderSz + length))
    refresh_table(tableCache, 2 * roundedHeaderSz + length);

  // We will add a new entry
  uint32_t *firstFree;
  int size = 0;
  for (firstFree = tableCache->table; firstFree < tableCache->table + tableCache->tblSz; firstFree += roundedHeaderSz) {
    gold_cache_t *tbl = (gold_cache_t *)firstFree;
    if (!tbl->length)
      break;
    firstFree += tbl->length;
    size++;
  }
  if (!tableCache->calls)
    LOG_D(PHY, "Number of entries (after reorganization) in gold cache: %d\n", size);

  gold_cache_t *new = (gold_cache_t *)firstFree;
  *new = (gold_cache_t){.key = key, .length = length, .usage = 1};
  unsigned int x1 = 0, x2 = key;
  uint32_t *sequence = firstFree + roundedHeaderSz;
  *sequence++ = lte_gold_generic(&x1, &x2, 1);
  for (int n = 1; n < length; n++)
    *sequence++ = lte_gold_generic(&x1, &x2, 0);
  LOG_D(PHY, "created a gold sequence, start %d; len %d\n", key, length);
  return firstFree + roundedHeaderSz;
}

uint32_t *nr_gold_pbch(int Lmax, int Nid, int n_hf, int l)
{
  int i_ssb = l & (Lmax - 1);
  int i_ssb2 = i_ssb + (n_hf << 2);
  uint32_t x2 = (1 << 11) * (i_ssb2 + 1) * ((Nid >> 2) + 1) + (1 << 6) * (i_ssb2 + 1) + (Nid & 3);
  return gold_cache(x2, NR_PBCH_DMRS_LENGTH_DWORD);
}

uint32_t *nr_gold_pdcch(int N_RB_DL, int symbols_per_slot, unsigned short nid, int ns, int l)
{
  int pdcch_dmrs_init_length = (((N_RB_DL << 1) * 3) >> 5) + 1;
  uint64_t x2tmp0 = (((uint64_t)symbols_per_slot * ns + l + 1) * ((nid << 1) + 1));
  x2tmp0 <<= 17;
  x2tmp0 += (nid << 1);
  uint32_t x2 = x2tmp0 % (1U << 31); // cinit
  LOG_D(PHY, "PDCCH DMRS slot %d, symb %d, Nid %d, x2 %x\n", ns, l, nid, x2);
  return gold_cache(x2, pdcch_dmrs_init_length);
}

uint32_t *nr_gold_pdsch(int N_RB_DL, int symbols_per_slot, int nid, int nscid, int slot, int symbol)
{
  int pdsch_dmrs_init_length = ((N_RB_DL * 24) >> 5) + 1;
  uint64_t x2tmp0 = (((uint64_t)symbols_per_slot * slot + symbol + 1) * (((uint64_t)nid << 1) + 1)) << 17;
  uint32_t x2 = (x2tmp0 + (nid << 1) + nscid) % (1U << 31); // cinit
  LOG_D(PHY, "UE DMRS slot %d, symb %d, nscid %d, x2 %x\n", slot, symbol, nscid, x2);
  return gold_cache(x2, pdsch_dmrs_init_length);
}

uint32_t *nr_gold_pusch(int N_RB_UL, int symbols_per_slot, int Nid, int nscid, int slot, int symbol)
{
  return nr_gold_pdsch(N_RB_UL, symbols_per_slot, Nid, nscid, slot, symbol);
}

uint32_t *nr_gold_csi_rs(int N_RB_DL, int symbols_per_slot, int slot, int symb, uint32_t Nid)
{
  int csi_dmrs_init_length = ((N_RB_DL << 4) >> 5) + 1;
  uint64_t temp_x2 = (1ULL << 10) * ((uint64_t)symbols_per_slot * slot + symb + 1) * ((Nid << 1) + 1) + Nid;
  uint32_t x2 = temp_x2 % (1U << 31);
  return gold_cache(x2, csi_dmrs_init_length);
}

uint32_t *nr_gold_prs(int Nid, int slotNum, int symNum)
{
  LOG_D(PHY, "Initialised NR-PRS sequence for PCI %d\n", Nid);
  // initial x2 for prs as ts138.211
  uint32_t pow22 = 1 << 22;
  uint32_t pow10 = 1 << 10;
  uint32_t c_init1 = pow22 * ceil(Nid / 1024);
  uint32_t c_init2 = pow10 * (slotNum + symNum + 1) * (2 * (Nid % 1024) + 1);
  uint32_t c_init3 = Nid % 1024;
  uint32_t x2 = c_init1 + c_init2 + c_init3;
  return gold_cache(x2, NR_MAX_PRS_INIT_LENGTH_DWORD);
}
