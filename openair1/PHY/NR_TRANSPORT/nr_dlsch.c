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

/*! \file nr_dlsch.c
 * \brief Top-level routines for transmission of the PDSCH 38211 v 15.2.0
 * \author Guy De Souza
 * \date 2018
 * \version 0.1
 * \company Eurecom
 * \email: desouza@eurecom.fr
 * \note
 * \warning
 */

#include "nr_dlsch.h"
#include "nr_dci.h"
#include "nr_sch_dmrs.h"
#include "PHY/MODULATION/nr_modulation.h"
#include "PHY/NR_REFSIG/dmrs_nr.h"
#include "PHY/NR_REFSIG/ptrs_nr.h"
#include "common/utils/LOG/vcd_signal_dumper.h"
#include "common/utils/nr/nr_common.h"
#include "executables/softmodem-common.h"
#include "SCHED_NR/sched_nr.h"

// #define DEBUG_DLSCH
// #define DEBUG_DLSCH_MAPPING
#include <simde/x86/avx512.h>
#define USE128BIT

static void nr_pdsch_codeword_scrambling(uint8_t *in, uint32_t size, uint8_t q, uint32_t Nid, uint32_t n_RNTI, uint32_t *out)
{
  nr_codeword_scrambling(in, size, q, Nid, n_RNTI, out);
}

static int do_ptrs_symbol(nfapi_nr_dl_tti_pdsch_pdu_rel15_t *rel15,
                          int start_sc,
                          int symbol_sz,
                          c16_t *txF,
                          c16_t *tx_layer,
                          int amp,
                          c16_t *mod_ptrs)
{
  int ptrs_idx = 0;
  int k = start_sc;
  c16_t *in = tx_layer;
  for (int i = 0; i < rel15->rbSize * NR_NB_SC_PER_RB; i++) {
    /* check for PTRS symbol and set flag for PTRS RE */
    bool is_ptrs_re =
        is_ptrs_subcarrier(k, rel15->rnti, rel15->PTRSFreqDensity, rel15->rbSize, rel15->PTRSReOffset, start_sc, symbol_sz);
    if (is_ptrs_re) {
      /* check if cuurent RE is PTRS RE*/
      uint16_t beta_ptrs = 1;
      txF[k] = c16mulRealShift(mod_ptrs[ptrs_idx], beta_ptrs * amp, 15);
#ifdef DEBUG_DLSCH_MAPPING
      printf("ptrs_idx %d\t \t k %d \t \t txdataF: %d %d, mod_ptrs: %d %d\n",
             ptrs_idx,
             k,
             txF[k].r,
             txF[k].i,
             mod_ptrs[ptrs_idx].r,
             mod_ptrs[ptrs_idx].i);
#endif
      ptrs_idx++;
    } else {
      txF[k] = c16mulRealShift(*in++, amp, 15);
#ifdef DEBUG_DLSCH_MAPPING
      printf("k %d \t txdataF: %d %d\n", k, txF[k].r, txF[k].i);
#endif
    }
    if (++k >= symbol_sz)
      k -= symbol_sz;
  }
  return in - tx_layer;
}

typedef union {
  uint64_t l;
  c16_t s[2];
} amp_t;

static inline int interleave_with_0_signal_first(c16_t *output, c16_t *mod_dmrs, const int16_t amp_dmrs, int sz)
{
#ifdef DEBUG_DLSCH_MAPPING
  printf("doing DMRS pattern for port 0 : d0 0 d1 0 ... dNm2 0 dNm1 0 (ul %d, rr %d)\n", upper_limit, remaining_re);
#endif
  // add filler to process all as SIMD
  c16_t *out = output;
  int i = 0;
  int end = sz / 2;
#if defined(__AVX512BW__)
  simde__m512i zeros512 = simde_mm512_setzero_si512(), amp_dmrs512 = simde_mm512_set1_epi16(amp_dmrs);
  simde__m512i perml = simde_mm512_set_epi32(23, 7, 22, 6, 21, 5, 20, 4, 19, 3, 18, 2, 17, 1, 16, 0);
  simde__m512i permh = simde_mm512_set_epi32(31, 15, 30, 14, 29, 13, 28, 12, 27, 11, 26, 10, 25, 9, 24, 8);
  for (; i < (end & ~15); i += 16) {
    simde__m512i d0 = simde_mm512_mulhrs_epi16(_mm512_loadu_si512((simde__m512i *)(mod_dmrs + i)), amp_dmrs512);
    simde_mm512_storeu_si512((simde__m512i *)out, simde_mm512_permutex2var_epi32(d0, perml, zeros512));
    out += 16;
    simde_mm512_storeu_si512((simde__m512i *)out, simde_mm512_permutex2var_epi32(d0, permh, zeros512));
    out += 16;
  }
#endif
#if defined(__AVX2__)
  simde__m256i zeros256 = simde_mm256_setzero_si256(), amp_dmrs256 = simde_mm256_set1_epi16(amp_dmrs);
  for (; i < (end & ~7); i += 8) {
    simde__m256i d0 = simde_mm256_mulhrs_epi16(simde_mm256_loadu_si256((simde__m256i *)(mod_dmrs + i)), amp_dmrs256);
    simde__m256i d2 = simde_mm256_unpacklo_epi32(d0, zeros256);
    simde__m256i d3 = simde_mm256_unpackhi_epi32(d0, zeros256);
    simde_mm256_storeu_si256((simde__m256i *)out, simde_mm256_permute2x128_si256(d2, d3, 32));
    out += 8;
    simde_mm256_storeu_si256((simde__m256i *)out, simde_mm256_permute2x128_si256(d2, d3, 49));
    out += 8;
  }
#endif
#if defined(USE128BIT)
  simde__m128i zeros = simde_mm_setzero_si128(), amp_dmrs128 = simde_mm_set1_epi16(amp_dmrs);
  for (; i < (end & ~3); i += 4) {
    simde__m128i d0 = simde_mm_mulhrs_epi16(simde_mm_loadu_si128((simde__m128i *)(mod_dmrs + i)), amp_dmrs128);
    simde__m128i d2 = simde_mm_unpacklo_epi32(d0, zeros);
    simde__m128i d3 = simde_mm_unpackhi_epi32(d0, zeros);
    simde_mm_storeu_si128((simde__m128i *)out, d2);
    out += 4;
    simde_mm_storeu_si128((simde__m128i *)out, d3);
    out += 4;
  }
#endif
  for (; i < end; i++) {
    *out++ = c16mulRealShift(mod_dmrs[i], amp_dmrs, 15);
    *out++ = (c16_t){};
  }
  return 0;
}

static inline int interleave_with_0_start_with_0(c16_t *output, c16_t *mod_dmrs, const int16_t amp_dmrs, int sz)
{
#ifdef DEBUG_DLSCH_MAPPING
  printf("doing DMRS pattern for port 2 : 0 d0 0 d1 ... 0 dNm2 0 dNm1\n");
#endif
  c16_t *out = output;
  int i = 0;
  int end = sz / 2;
#if defined(__AVX512BW__)
  simde__m512i zeros512 = simde_mm512_setzero_si512(), amp_dmrs512 = simde_mm512_set1_epi16(amp_dmrs);
  simde__m512i perml = simde_mm512_set_epi32(23, 7, 22, 6, 21, 5, 20, 4, 19, 3, 18, 2, 17, 1, 16, 0);
  simde__m512i permh = simde_mm512_set_epi32(31, 15, 30, 14, 29, 13, 28, 12, 27, 11, 26, 10, 25, 9, 24, 8);
  for (; i < (end & ~15); i += 16) {
    simde__m512i d0 = simde_mm512_mulhrs_epi16(_mm512_loadu_si512((simde__m512i *)(mod_dmrs + i)), amp_dmrs512);
    simde_mm512_storeu_si512((simde__m512i *)out, simde_mm512_permutex2var_epi32(zeros512, perml, d0));
    out += 16;
    simde_mm512_storeu_si512((simde__m512i *)out, simde_mm512_permutex2var_epi32(zeros512, permh, d0));
    out += 16;
  }
#endif
#if defined(__AVX2__)
  simde__m256i zeros256 = simde_mm256_setzero_si256(), amp_dmrs256 = simde_mm256_set1_epi16(amp_dmrs);
  for (; i < (end & ~7); i += 8) {
    simde__m256i d0 = simde_mm256_mulhrs_epi16(simde_mm256_loadu_si256((simde__m256i *)(mod_dmrs + i)), amp_dmrs256);
    simde__m256i d2 = simde_mm256_unpacklo_epi32(zeros256, d0);
    simde__m256i d3 = simde_mm256_unpackhi_epi32(zeros256, d0);
    simde_mm256_storeu_si256((simde__m256i *)out, simde_mm256_permute2x128_si256(d2, d3, 32));
    out += 8;
    simde_mm256_storeu_si256((simde__m256i *)out, simde_mm256_permute2x128_si256(d2, d3, 49));
    out += 8;
  }
#endif
#if defined(USE128BIT)
  simde__m128i zeros = simde_mm_setzero_si128(), amp_dmrs128 = simde_mm_set1_epi16(amp_dmrs);
  for (; i < (end & ~3); i += 4) {
    simde__m128i d0 = simde_mm_mulhrs_epi16(simde_mm_loadu_si128((simde__m128i *)(mod_dmrs + i)), amp_dmrs128);
    simde__m128i d2 = simde_mm_unpacklo_epi32(zeros, d0);
    simde__m128i d3 = simde_mm_unpackhi_epi32(zeros, d0);
    simde_mm_storeu_si128((simde__m128i *)out, d2);
    out += 4;
    simde_mm_storeu_si128((simde__m128i *)out, d3);
    out += 4;
  }
#endif
  for (; i < end; i++) {
    *out++ = (c16_t){};
    *out++ = c16mulRealShift(mod_dmrs[i], amp_dmrs, 15);
  }
  return 0;
}

static inline int interleave_signals(c16_t *output, c16_t *signal1, const int amp, c16_t *signal2, const int amp2, int sz)
{
#ifdef DEBUG_DLSCH_MAPPING
  printf("doing DMRS pattern for port 0 : d0 X0 d1 X1 ... dNm2 XNm2 dNm1 XNm1\n");
#endif
    // add filler to process all as SIMD
  c16_t *out = output;
  int i = 0;
  int end = sz / 2;
#if defined(__AVX512BW__)
  simde__m512i amp2512 = simde_mm512_set1_epi16(amp2), amp512 = simde_mm512_set1_epi16(amp);
  simde__m512i perml = simde_mm512_set_epi32(23, 7, 22, 6, 21, 5, 20, 4, 19, 3, 18, 2, 17, 1, 16, 0);
  simde__m512i permh = simde_mm512_set_epi32(31, 15, 30, 14, 29, 13, 28, 12, 27, 11, 26, 10, 25, 9, 24, 8);
  for (; i < (end & ~15); i += 16) {
    simde__m512i d0 = simde_mm512_mulhrs_epi16(_mm512_loadu_si512((simde__m512i *)(signal2 + i)), amp2512);
    simde__m512i d1 = simde_mm512_mulhrs_epi16(_mm512_loadu_si512((simde__m512i *)(signal1 + i)), amp512);
    simde_mm512_storeu_si512((simde__m512i *)out, simde_mm512_permutex2var_epi32(d0, perml, d1));
    out += 16;
    simde_mm512_storeu_si512((simde__m512i *)out, simde_mm512_permutex2var_epi32(d0, permh, d1));
    out += 16;
  }
#endif
#if defined(__AVX2__)
  simde__m256i amp2256 = simde_mm256_set1_epi16(amp2), amp256 = simde_mm256_set1_epi16(amp);
  for (; i < (end & ~7); i += 8) {
    simde__m256i d0 = simde_mm256_mulhrs_epi16(simde_mm256_loadu_si256((simde__m256i *)(signal2 + i)), amp2256);
    simde__m256i d1 = simde_mm256_mulhrs_epi16(simde_mm256_loadu_si256((simde__m256i *)(signal1 + i)), amp256);
    simde__m256i d2 = simde_mm256_unpacklo_epi32(d0, d1);
    simde__m256i d3 = simde_mm256_unpackhi_epi32(d0, d1);
    simde_mm256_storeu_si256((simde__m256i *)out, simde_mm256_permute2x128_si256(d2, d3, 32));
    out += 8;
    simde_mm256_storeu_si256((simde__m256i *)out, simde_mm256_permute2x128_si256(d2, d3, 49));
    out += 8;
  }
#endif
#if defined(USE128BIT)
  simde__m128i amp2128 = simde_mm_set1_epi16(amp2), amp128 = simde_mm_set1_epi16(amp);
  for (; i < (end & ~3); i += 4) {
    simde__m128i d0 = simde_mm_mulhrs_epi16(simde_mm_loadu_si128((simde__m128i *)(signal2 + i)), amp2128);
    simde__m128i d1 = simde_mm_mulhrs_epi16(simde_mm_loadu_si128((simde__m128i *)(signal1 + i)), amp128);
    simde__m128i d2 = simde_mm_unpacklo_epi32(d0, d1);
    simde__m128i d3 = simde_mm_unpackhi_epi32(d0, d1);
    simde_mm_storeu_si128((simde__m128i *)out, d2);
    out += 4;
    simde_mm_storeu_si128((simde__m128i *)out, d3);
    out += 4;
  }
#endif
  for (; i < end; i++) {
    *out++ = c16mulRealShift(signal2[i], amp2, 15);
    *out++ = c16mulRealShift(signal1[i], amp, 15);
  }
  return sz / 2;
}

static inline int dmrs_case00(c16_t *output,
                              c16_t *txl,
                              c16_t *mod_dmrs,
                              const int16_t amp_dmrs,
                              const int amp,
                              int sz,
                              int start_sc,
                              int remaining_re,
                              int dmrs_port,
                              const int dmrs_Type,
                              int symbol_sz,
                              int l_prime,
                              uint8_t numDmrsCdmGrpsNoData)
{
  // DMRS params for this dmrs port
  int Wt[2], Wf[2];
  get_Wt(Wt, dmrs_port, dmrs_Type);
  get_Wf(Wf, dmrs_port, dmrs_Type);
  const int8_t delta = get_delta(dmrs_port, dmrs_Type);
  int dmrs_idx = 0;
  int k = start_sc;
  c16_t *in = txl;
  uint8_t k_prime = 0;
  uint16_t n = 0;
  for (int i = 0; i < sz; i++) {
    if (k == ((start_sc + get_dmrs_freq_idx(n, k_prime, delta, dmrs_Type)) % (symbol_sz))) {
      output[k] = c16mulRealShift(mod_dmrs[dmrs_idx], Wt[l_prime] * Wf[k_prime] * amp_dmrs, 15);
      dmrs_idx++;
      k_prime = (k_prime + 1) & 1;
      n += (k_prime ? 0 : 1);
    }
    /* Map PTRS Symbol */
    /* Map DATA Symbol */
    else if (allowed_xlsch_re_in_dmrs_symbol(k, start_sc, symbol_sz, numDmrsCdmGrpsNoData, dmrs_Type)) {
      output[k] = c16mulRealShift(*in++, amp, 15);
    }
    /* mute RE */
    else {
      output[k] = (c16_t){0};
    }
    k = (k + 1) % symbol_sz;
  } // RE loop
  return in - txl;
}

static inline int no_ptrs_dmrs_case(c16_t *output, c16_t *txl, const int amp, const int sz)
{
  // Loop Over SCs:
  int i = 0;
#if defined(__AVX512BW__)
  simde__m512i amp512 = simde_mm512_set1_epi16(amp);
  for (; i < (sz & ~15); i += 16) {
    const simde__m512i txL = simde_mm512_loadu_si512((simde__m512i *)(txl + i));
    simde_mm512_storeu_si512((simde__m512i *)(output + i), simde_mm512_mulhrs_epi16(amp512, txL));
  }
#endif
#if defined(__AVX2__)
  simde__m256i amp256 = simde_mm256_set1_epi16(amp);
  for (; i < (sz & ~7); i += 8) {
    const simde__m256i txL = simde_mm256_loadu_si256((simde__m256i *)(txl + i));
    simde_mm256_storeu_si256((simde__m256i *)(output + i), _mm256_mulhrs_epi16(amp256, txL));
  }
#endif
#if defined(USE128BIT)
  simde__m128i amp128 = simde_mm_set1_epi16(amp);
  for (; i < (sz & ~3); i += 4) {
    const simde__m128i txL = simde_mm_loadu_si128((simde__m128i *)(txl + i));
    simde_mm_storeu_si128((simde__m128i *)(output + i), simde_mm_mulhrs_epi16(amp128, txL));
  }
#endif
  for (; i < sz; i++) {
    output[i] = c16mulRealShift(txl[i], amp, 15);
  }
  return sz;
}

static inline void neg_dmrs(c16_t *in, c16_t *out, int sz)
{
  for (int i = 0; i < sz; i++)
    *out++ = i % 2 ? (c16_t){-in[i].r, -in[i].i} : in[i];
}

static inline int do_onelayer(NR_DL_FRAME_PARMS *frame_parms,
                              int slot,
                              nfapi_nr_dl_tti_pdsch_pdu_rel15_t *rel15,
                              int layer,
                              c16_t *output,
                              c16_t *txl_start,
                              int start_sc,
                              int symbol_sz,
                              int l_symbol,
                              uint16_t dlPtrsSymPos,
                              int n_ptrs,
                              int amp,
                              int16_t amp_dmrs,
                              int l_prime,
                              nfapi_nr_dmrs_type_e dmrs_Type,
                              c16_t *dmrs_start)
{
  c16_t *txl = txl_start;
  const uint sz = rel15->rbSize * NR_NB_SC_PER_RB;
  int upper_limit = sz;
  int remaining_re = 0;
  if (start_sc + upper_limit > symbol_sz) {
    upper_limit = symbol_sz - start_sc;
    remaining_re = sz - upper_limit;
  }

  /* calculate if current symbol is PTRS symbols */
  int ptrs_symbol = 0;
  if (rel15->pduBitmap & 0x1) {
    ptrs_symbol = is_ptrs_symbol(l_symbol, dlPtrsSymPos);
  }

  if (ptrs_symbol) {
    /* PTRS QPSK Modulation for each OFDM symbol in a slot */
    LOG_D(PHY, "Doing ptrs modulation for symbol %d, n_ptrs %d\n", l_symbol, n_ptrs);
    c16_t mod_ptrs[max(n_ptrs, 1)]
        __attribute__((aligned(64))); // max only to please sanitizer, that kills if 0 even if it is not a error
    const uint32_t *gold =
        nr_gold_pdsch(frame_parms->N_RB_DL, frame_parms->symbols_per_slot, rel15->dlDmrsScramblingId, rel15->SCID, slot, l_symbol);
    nr_modulation(gold, n_ptrs * DMRS_MOD_ORDER, DMRS_MOD_ORDER, (int16_t *)mod_ptrs);
    txl += do_ptrs_symbol(rel15, start_sc, symbol_sz, output, txl, amp, mod_ptrs);

  } else if (rel15->dlDmrsSymbPos & (1 << l_symbol)) {
    /* Map DMRS Symbol */
    int dmrs_port = get_dmrs_port(layer, rel15->dmrsPorts);
    if (l_prime == 0 && dmrs_Type == NFAPI_NR_DMRS_TYPE1) {
      if (rel15->numDmrsCdmGrpsNoData == 2) {
        switch (dmrs_port & 3) {
          case 0:
            txl += interleave_with_0_signal_first(output + start_sc, dmrs_start, amp_dmrs, upper_limit);
            txl += interleave_with_0_signal_first(output, dmrs_start + upper_limit / 2, amp_dmrs, remaining_re);
            break;
          case 1: {
            c16_t dmrs[sz / 2];
            neg_dmrs(dmrs_start, dmrs, sz / 2);
            txl += interleave_with_0_signal_first(output + start_sc, dmrs, amp_dmrs, upper_limit);
            txl += interleave_with_0_signal_first(output, dmrs + upper_limit / 2, amp_dmrs, remaining_re);
          } break;
          case 2:
            txl += interleave_with_0_start_with_0(output + start_sc, dmrs_start, amp_dmrs, upper_limit);
            txl += interleave_with_0_start_with_0(output, dmrs_start + upper_limit / 2, amp_dmrs, remaining_re);
            break;
          case 3: {
            c16_t dmrs[sz / 2];
            neg_dmrs(dmrs_start, dmrs, sz / 2);
            txl += interleave_with_0_start_with_0(output + start_sc, dmrs, amp_dmrs, upper_limit);
            txl += interleave_with_0_start_with_0(output, dmrs + upper_limit / 2, amp_dmrs, remaining_re);
          } break;
        }
      } else if (rel15->numDmrsCdmGrpsNoData == 1) {
        switch (dmrs_port & 3) {
          case 0:
            txl += interleave_signals(output + start_sc, txl, amp, dmrs_start, amp_dmrs, upper_limit);
            txl += interleave_signals(output, txl, amp, dmrs_start + upper_limit / 2, amp_dmrs, remaining_re);
            break;
          case 1: {
            c16_t dmrs[sz / 2];
            neg_dmrs(dmrs_start, dmrs, sz / 2);
            txl += interleave_signals(output + start_sc, txl, amp, dmrs, amp_dmrs, upper_limit);
            txl += interleave_signals(output, txl, amp, dmrs + upper_limit / 2, amp_dmrs, remaining_re);
          } break;
          case 2:
            txl += interleave_signals(output + start_sc, dmrs_start, amp_dmrs, txl, amp, upper_limit);
            txl += interleave_signals(output, dmrs_start + upper_limit / 2, amp_dmrs, txl, amp, remaining_re);
            break;
          case 3: {
            c16_t dmrs[sz / 2];
            neg_dmrs(dmrs_start, dmrs, sz / 2);
            txl += interleave_signals(output + start_sc, dmrs, amp_dmrs, txl, amp, upper_limit);
            txl += interleave_signals(output, dmrs + upper_limit / 2, amp_dmrs, txl, amp, remaining_re);
          } break;
        }
      } else
        AssertFatal(false, "rel15->numDmrsCdmGrpsNoData is %d\n", rel15->numDmrsCdmGrpsNoData);
    } else {
      txl += dmrs_case00(output,
                         txl,
                         dmrs_start,
                         amp_dmrs,
                         amp,
                         sz,
                         start_sc,
                         remaining_re,
                         dmrs_port,
                         dmrs_Type,
                         symbol_sz,
                         l_prime,
                         rel15->numDmrsCdmGrpsNoData);
    } // generic DMRS case
  } else { // no PTRS or DMRS in this symbol
    txl += no_ptrs_dmrs_case(output + start_sc, txl, amp, upper_limit);
    txl += no_ptrs_dmrs_case(output, txl, amp, remaining_re);
  } // no DMRS/PTRS in symbol
  return txl - txl_start;
}

static inline void do_txdataF(c16_t **txdataF,
                              int symbol_sz,
                              c16_t txdataF_precoding[][symbol_sz],
                              PHY_VARS_gNB *gNB,
                              nfapi_nr_dl_tti_pdsch_pdu_rel15_t *rel15,
                              int ant,
                              int start_sc,
                              int txdataF_offset_per_symbol)
{
  NR_DL_FRAME_PARMS *frame_parms = &gNB->frame_parms;
  int rb = 0;
  uint16_t subCarrier = start_sc;
  nfapi_nr_tx_precoding_and_beamforming_t *pb = &rel15->precodingAndBeamforming;
  while (rb < rel15->rbSize) {
    // get pmi info
    const int pmi = (pb->prg_size > 0) ? (pb->prgs_list[(int)rb / pb->prg_size].pm_idx) : 0;
    const int pmi2 = (rb < (rel15->rbSize - 1) && pb->prg_size > 0) ? (pb->prgs_list[(int)(rb + 1) / pb->prg_size].pm_idx) : -1;

    // If pmi of next RB and pmi of current RB are the same, we do 2 RB in a row
    // if pmi differs, or current rb is the end (rel15->rbSize - 1), than we do 1 RB in a row
    const int rb_step = pmi == pmi2 ? 2 : 1;
    const int re_cnt = NR_NB_SC_PER_RB * rb_step;

    if (pmi == 0) { // unitary Precoding
      if (subCarrier + re_cnt <= symbol_sz) { // RB does not cross DC
        if (ant < rel15->nrOfLayers)
          memcpy(&txdataF[ant][txdataF_offset_per_symbol + subCarrier],
                 &txdataF_precoding[ant][subCarrier],
                 re_cnt * sizeof(**txdataF));
        else
          memset(&txdataF[ant][txdataF_offset_per_symbol + subCarrier], 0, re_cnt * sizeof(**txdataF));
      } else { // RB does cross DC
        const int neg_length = symbol_sz - subCarrier;
        const int pos_length = re_cnt - neg_length;
        if (ant < rel15->nrOfLayers) {
          memcpy(&txdataF[ant][txdataF_offset_per_symbol + subCarrier],
                 &txdataF_precoding[ant][subCarrier],
                 neg_length * sizeof(**txdataF));
          memcpy(&txdataF[ant][txdataF_offset_per_symbol], &txdataF_precoding[ant], pos_length * sizeof(**txdataF));
        } else {
          memset(&txdataF[ant][txdataF_offset_per_symbol + subCarrier], 0, neg_length * sizeof(**txdataF));
          memset(&txdataF[ant][txdataF_offset_per_symbol], 0, pos_length * sizeof(**txdataF));
        }
      }
      subCarrier += re_cnt;
      if (subCarrier >= symbol_sz) {
        subCarrier -= symbol_sz;
      }
    } else { // non-unitary Precoding
      AssertFatal(frame_parms->nb_antennas_tx > 1, "No precoding can be done with a single antenna port\n");
      // get the precoding matrix weights:
      nfapi_nr_pm_pdu_t *pmi_pdu = &gNB->gNB_config.pmi_list.pmi_pdu[pmi - 1]; // pmi 0 is identity matrix
      AssertFatal(pmi == pmi_pdu->pm_idx, "PMI %d doesn't match to the one in precoding matrix %d\n", pmi, pmi_pdu->pm_idx);
      AssertFatal(ant < pmi_pdu->num_ant_ports,
                  "Antenna port index %d exceeds precoding matrix AP size %d\n",
                  ant,
                  pmi_pdu->num_ant_ports);
      AssertFatal(rel15->nrOfLayers == pmi_pdu->numLayers,
                  "Number of layers %d doesn't match to the one in precoding matrix %d\n",
                  rel15->nrOfLayers,
                  pmi_pdu->numLayers);
      if ((subCarrier + re_cnt) < symbol_sz) { // within ofdm_symbol_size, use SIMDe
        nr_layer_precoder_simd(rel15->nrOfLayers,
                               symbol_sz,
                               txdataF_precoding,
                               ant,
                               pmi_pdu,
                               subCarrier,
                               re_cnt,
                               &txdataF[ant][txdataF_offset_per_symbol]);
        subCarrier += re_cnt;
      } else { // crossing ofdm_symbol_size, use simple arithmetic operations
        for (int i = 0; i < re_cnt; i++) {
          txdataF[ant][txdataF_offset_per_symbol + subCarrier] =
              nr_layer_precoder_cm(rel15->nrOfLayers, symbol_sz, txdataF_precoding, ant, pmi_pdu, subCarrier);
#ifdef DEBUG_DLSCH_MAPPING
          printf("antenna %d\t l %d \t subCarrier %d \t txdataF: %d %d\n",
                 ant,
                 l_symbol,
                 subCarrier,
                 txdataF[ant][l_symbol * symbol_sz + subCarrier + txdataF_offset].r,
                 txdataF[ant][l_symbol * symbol_sz + subCarrier + txdataF_offset].i);
#endif
          if (++subCarrier >= symbol_sz) {
            subCarrier -= symbol_sz;
          }
        }
      } // else{ // crossing ofdm_symbol_size, use simple arithmetic operations
    } // else { // non-unitary Precoding

    rb += rb_step;
  } // RB loop: while(rb < rel15->rbSize)
}
static int do_one_dlsch(unsigned char *input_ptr, PHY_VARS_gNB *gNB, NR_gNB_DLSCH_t *dlsch, int slot)
{
  const int16_t amp = gNB->TX_AMP;
  NR_DL_FRAME_PARMS *frame_parms = &gNB->frame_parms;

  time_stats_t *dlsch_scrambling_stats = &gNB->dlsch_scrambling_stats;
  time_stats_t *dlsch_modulation_stats = &gNB->dlsch_modulation_stats;
  NR_DL_gNB_HARQ_t *harq = &dlsch->harq_process;
  nfapi_nr_dl_tti_pdsch_pdu_rel15_t *rel15 = &harq->pdsch_pdu.pdsch_pdu_rel15;
  const int layerSz = frame_parms->N_RB_DL * NR_SYMBOLS_PER_SLOT * NR_NB_SC_PER_RB;
  const int symbol_sz=frame_parms->ofdm_symbol_size;
  const int dmrs_Type = rel15->dmrsConfigType;
  const int nb_re_dmrs = rel15->numDmrsCdmGrpsNoData * (rel15->dmrsConfigType == NFAPI_NR_DMRS_TYPE1 ? 6 : 4);
  const int16_t amp_dmrs = min((double)amp * sqrt(rel15->numDmrsCdmGrpsNoData), INT16_MAX); // 3GPP TS 38.214 Section 4.1: Table 4.1-1
  LOG_D(PHY,
        "pdsch: BWPStart %d, BWPSize %d, rbStart %d, rbsize %d\n",
        rel15->BWPStart,
        rel15->BWPSize,
        rel15->rbStart,
        rel15->rbSize);
  const int n_dmrs = (rel15->BWPStart + rel15->rbStart + rel15->rbSize) * nb_re_dmrs;

  const int dmrs_symbol_map = rel15->dlDmrsSymbPos; // single DMRS: 010000100 Double DMRS 110001100
  const int xOverhead = 0;
  const int nb_re =
      (12 * rel15->NrOfSymbols - nb_re_dmrs * get_num_dmrs(rel15->dlDmrsSymbPos) - xOverhead) * rel15->rbSize * rel15->nrOfLayers;
  const int Qm = rel15->qamModOrder[0];
  const int encoded_length = nb_re * Qm;

  /* PTRS */
  uint16_t dlPtrsSymPos = 0;
  int n_ptrs = 0;
  uint32_t ptrsSymbPerSlot = 0;
  if (rel15->pduBitmap & 0x1) {
    set_ptrs_symb_idx(&dlPtrsSymPos,
                      rel15->NrOfSymbols,
                      rel15->StartSymbolIndex,
                      1 << rel15->PTRSTimeDensity,
                      rel15->dlDmrsSymbPos);
    n_ptrs = (rel15->rbSize + rel15->PTRSFreqDensity - 1) / rel15->PTRSFreqDensity;
    ptrsSymbPerSlot = get_ptrs_symbols_in_slot(dlPtrsSymPos, rel15->StartSymbolIndex, rel15->NrOfSymbols);
  }
  harq->unav_res = ptrsSymbPerSlot * n_ptrs;

#ifdef DEBUG_DLSCH
  printf("PDSCH encoding:\nPayload:\n");
  for (int i = 0; i < (harq->B >> 3); i += 16) {
    for (int j = 0; j < 16; j++)
      printf("0x%02x\t", harq->pdu[i + j]);
    printf("\n");
  }
  printf("\nEncoded payload:\n");
  for (int i = 0; i < encoded_length; i += 8) {
    for (int j = 0; j < 8; j++)
      printf("%d", (input_ptr[i >> 3] >> j) & 1);
    printf("\t");
  }
  printf("\n");
#endif

  if (IS_SOFTMODEM_DLSIM)
    memcpy(harq->f, input_ptr, (encoded_length + 7) >> 3);

  c16_t mod_symbs[rel15->NrOfCodewords][encoded_length] __attribute__((aligned(64)));
  for (int codeWord = 0; codeWord < rel15->NrOfCodewords; codeWord++) {
    /// scrambling
    start_meas(dlsch_scrambling_stats);
    uint32_t scrambled_output[(encoded_length >> 5) + 4]; // modulator acces by 4 bytes in some cases
    memset(scrambled_output, 0, sizeof(scrambled_output));
    nr_pdsch_codeword_scrambling(input_ptr, encoded_length, codeWord, rel15->dataScramblingId, rel15->rnti, scrambled_output);

#ifdef DEBUG_DLSCH
    printf("PDSCH scrambling:\n");
    for (int i = 0; i < encoded_length >> 8; i++) {
      for (int j = 0; j < 8; j++)
        printf("0x%08x\t", scrambled_output[(i << 3) + j]);
      printf("\n");
    }
#endif

    stop_meas(dlsch_scrambling_stats);
    /// Modulation
    start_meas(dlsch_modulation_stats);
    nr_modulation(scrambled_output, encoded_length, Qm, (int16_t *)mod_symbs[codeWord]);
    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_gNB_PDSCH_MODULATION, 0);
    stop_meas(dlsch_modulation_stats);
#ifdef DEBUG_DLSCH
    printf("PDSCH Modulation: Qm %d(%d)\n", Qm, nb_re);
    for (int i = 0; i < nb_re; i += 8) {
      for (int j = 0; j < 8; j++) {
        printf("%d %d\t", mod_symbs[codeWord][i + j].r, mod_symbs[codeWord][i + j].i);
      }
      printf("\n");
    }
#endif
  }

  /// Resource mapping
  // Non interleaved VRB to PRB mapping
  uint16_t start_sc = frame_parms->first_carrier_offset + (rel15->rbStart + rel15->BWPStart) * NR_NB_SC_PER_RB;
  if (start_sc >= symbol_sz)
    start_sc -= symbol_sz;

  const uint32_t txdataF_offset = slot * frame_parms->samples_per_slot_wCP;
#ifdef DEBUG_DLSCH_MAPPING
  printf("PDSCH resource mapping started (start SC %d\tstart symbol %d\tN_PRB %d\tnb_re %d,nb_layers %d)\n",
         start_sc,
         rel15->StartSymbolIndex,
         rel15->rbSize,
         nb_re,
         rel15->nrOfLayers);
#endif

  AssertFatal(n_dmrs, "n_dmrs can't be 0\n");
  // make a large enough tail to process all re with SIMD regardless a garbadge filler
  c16_t mod_dmrs[(n_dmrs+63)&~63] __attribute__((aligned(64)));
  unsigned int re_beginning_of_symbol = 0;

  int layerSz2 = (layerSz + 63) & ~63;
  c16_t tx_layers[rel15->nrOfLayers][layerSz2] __attribute__((aligned(64)));
  memset(tx_layers, 0, sizeof(tx_layers));
  nr_layer_mapping(rel15->NrOfCodewords, encoded_length, mod_symbs, rel15->nrOfLayers, layerSz2, nb_re, tx_layers);

  /// Layer Precoding and Antenna port mapping
  // tx_layers 1-8 are mapped on antenna ports 1000-1007
  // The precoding info is supported by nfapi such as num_prgs, prg_size, prgs_list and pm_idx
  // The same precoding matrix is applied on prg_size RBs, Thus
  //        pmi = prgs_list[rbidx/prg_size].pm_idx, rbidx =0,...,rbSize-1
  // The Precoding matrix:
  // The Codebook Type I
  start_meas(&gNB->dlsch_precoding_stats);
  nfapi_nr_tx_precoding_and_beamforming_t *pb = &rel15->precodingAndBeamforming;
  // beam number in multi-beam scenario (concurrent beams)
  int bitmap = SL_to_bitmap(rel15->StartSymbolIndex, rel15->NrOfSymbols);
  int beam_nb = beam_index_allocation(gNB->enable_analog_das,
                                      pb->prgs_list[0].dig_bf_interface_list[0].beam_idx,
                                      &gNB->gNB_config.analog_beamforming_ve,
                                      &gNB->common_vars,
                                      slot,
                                      frame_parms->symbols_per_slot,
                                      bitmap);

  c16_t **txdataF = gNB->common_vars.txdataF[beam_nb];

  // Loop Over OFDM symbols:
  for (int l_symbol = rel15->StartSymbolIndex; l_symbol < rel15->StartSymbolIndex + rel15->NrOfSymbols; l_symbol++) {
    int l_prime = 0; // single symbol layer 0
    int l_overline = get_l0(rel15->dlDmrsSymbPos);

#ifdef DEBUG_DLSCH_MAPPING
    printf("PDSCH resource mapping symbol %d\n", l_symbol);
#endif
    /// DMRS QPSK modulation
    if ((dmrs_symbol_map & (1 << l_symbol))) { // DMRS time occasion
      // The reference point for is subcarrier -1 of the lowest-numbered resource block in CORESET 0 if the corresponding
      // PDCCH is associated with CORESET -1 and Type0-PDCCH common search space and is addressed to SI-RNTI
      // 2GPP TS 38.211 V15.8.0 Section 7.4.1.1.2 Mapping to physical resources
      if (l_symbol == (l_overline + 1)) // take into account the double DMRS symbols
        l_prime = 1;
      else if (l_symbol > (l_overline + 1)) { // new DMRS pair
        l_overline = l_symbol;
        l_prime = 0;
      }
#ifdef DEBUG_DLSCH_MAPPING
      printf("dlDmrsScramblingId %d, SCID %d slot %d l_symbol %d\n", rel15->dlDmrsScramblingId, rel15->SCID, slot, l_symbol);
#endif
      const uint32_t *gold = nr_gold_pdsch(frame_parms->N_RB_DL,
                                           frame_parms->symbols_per_slot,
                                           rel15->dlDmrsScramblingId,
                                           rel15->SCID,
                                           slot,
                                           l_symbol);
      // Qm = 1 as DMRS is QPSK modulated
      nr_modulation(gold, n_dmrs * DMRS_MOD_ORDER, DMRS_MOD_ORDER, (int16_t *)mod_dmrs);

#ifdef DEBUG_DLSCH_MAPPING
      printf("DMRS modulation (symbol %d, %d symbols, type %d):\n", l_symbol, n_dmrs, dmrs_Type);
      for (int i = 0; i < n_dmrs / 2; i += 8) {
        for (int j = 0; j < 8; j++) {
          printf("%d %d\t", mod_dmrs[i + j].r, mod_dmrs[i + j].i);
        }
        printf("\n");
      }
#endif
    }
    uint32_t dmrs_idx = rel15->rbStart;
    if (rel15->refPoint == 0)
      dmrs_idx += rel15->BWPStart;
    dmrs_idx *= dmrs_Type == NFAPI_NR_DMRS_TYPE1 ? 6 : 4;
    c16_t txdataF_precoding[rel15->nrOfLayers][symbol_sz] __attribute__((aligned(64)));
    int layer_sz = 0;
    for (int layer = 0; layer < rel15->nrOfLayers; layer++) {
      layer_sz = do_onelayer(frame_parms,
                             slot,
                             rel15,
                             layer,
                             txdataF_precoding[layer],
                             tx_layers[layer] + re_beginning_of_symbol,
                             start_sc,
                             symbol_sz,
                             l_symbol,
                             dlPtrsSymPos,
                             n_ptrs,
                             amp,
                             amp_dmrs,
                             l_prime,
                             dmrs_Type,
                             mod_dmrs + dmrs_idx);
    } // layer loop
    re_beginning_of_symbol += layer_sz;
    stop_meas(&gNB->dlsch_resource_mapping_stats);

    for (int ant = 0; ant < frame_parms->nb_antennas_tx; ant++) {
      const size_t txdataF_offset_per_symbol = l_symbol * symbol_sz + txdataF_offset;
      do_txdataF(txdataF, symbol_sz, txdataF_precoding, gNB, rel15, ant, start_sc, txdataF_offset_per_symbol);
    }
  }

  stop_meas(&gNB->dlsch_precoding_stats);
  /* output and its parts for each dlsch should be aligned on 64 bytes (or 8 * 64 bits)
   * should remain a multiple of 8 * 64 with enough offset to fit each dlsch
   */
  uint32_t size_output_tb = rel15->rbSize * NR_SYMBOLS_PER_SLOT * NR_NB_SC_PER_RB * Qm * rel15->nrOfLayers;
  return ((size_output_tb + 511) >> 9) << 6;
}

void nr_generate_pdsch(processingData_L1tx_t *msgTx, int frame, int slot)
{
  PHY_VARS_gNB *gNB = msgTx->gNB;
  NR_DL_FRAME_PARMS *frame_parms = &gNB->frame_parms;
  time_stats_t *dlsch_encoding_stats = &gNB->dlsch_encoding_stats;
  time_stats_t *tinput = &gNB->tinput;
  time_stats_t *tprep = &gNB->tprep;
  time_stats_t *tparity = &gNB->tparity;
  time_stats_t *toutput = &gNB->toutput;
  time_stats_t *dlsch_rate_matching_stats = &gNB->dlsch_rate_matching_stats;
  time_stats_t *dlsch_interleaving_stats = &gNB->dlsch_interleaving_stats;
  time_stats_t *dlsch_segmentation_stats = &gNB->dlsch_segmentation_stats;

  size_t size_output = 0;

  for (int dlsch_id = 0; dlsch_id < msgTx->num_pdsch_slot; dlsch_id++) {
    NR_gNB_DLSCH_t *dlsch = msgTx->dlsch[dlsch_id];
    NR_DL_gNB_HARQ_t *harq = &dlsch->harq_process;
    nfapi_nr_dl_tti_pdsch_pdu_rel15_t *rel15 = &harq->pdsch_pdu.pdsch_pdu_rel15;

    LOG_D(PHY,
          "pdsch: BWPStart %d, BWPSize %d, rbStart %d, rbsize %d\n",
          rel15->BWPStart,
          rel15->BWPSize,
          rel15->rbStart,
          rel15->rbSize);

    const int Qm = rel15->qamModOrder[0];

    /* PTRS */
    uint16_t dlPtrsSymPos = 0;
    int n_ptrs = 0;
    uint32_t ptrsSymbPerSlot = 0;
    if (rel15->pduBitmap & 0x1) {
      set_ptrs_symb_idx(&dlPtrsSymPos,
                        rel15->NrOfSymbols,
                        rel15->StartSymbolIndex,
                        1 << rel15->PTRSTimeDensity,
                        rel15->dlDmrsSymbPos);
      n_ptrs = (rel15->rbSize + rel15->PTRSFreqDensity - 1) / rel15->PTRSFreqDensity;
      ptrsSymbPerSlot = get_ptrs_symbols_in_slot(dlPtrsSymPos, rel15->StartSymbolIndex, rel15->NrOfSymbols);
    }
    harq->unav_res = ptrsSymbPerSlot * n_ptrs;

    /// CRC, coding, interleaving and rate matching
    AssertFatal(harq->pdu != NULL, "%4d.%2d no HARQ PDU for PDSCH generation\n", msgTx->frame, msgTx->slot);

    /* output and its parts for each dlsch should be aligned on 64 bytes (or 8 * 64 bits)
     * => size_output is a sum of parts sizes rounded up to a multiple of 8 * 64
     */
    size_t size_output_tb = rel15->rbSize * NR_SYMBOLS_PER_SLOT * NR_NB_SC_PER_RB * Qm * rel15->nrOfLayers;
    size_output += ceil_mod(size_output_tb, 8 * 64);
  }

  unsigned char output[size_output >> 3] __attribute__((aligned(64)));
  bzero(output, sizeof(output));

  start_meas(dlsch_encoding_stats);
  if (nr_dlsch_encoding(gNB,
                        msgTx,
                        frame,
                        slot,
                        frame_parms,
                        output,
                        tinput,
                        tprep,
                        tparity,
                        toutput,
                        dlsch_rate_matching_stats,
                        dlsch_interleaving_stats,
                        dlsch_segmentation_stats)
      == -1) {
    return;
  }
  stop_meas(dlsch_encoding_stats);

  unsigned char *output_ptr = output;
  for (int dlsch_id = 0; dlsch_id < msgTx->num_pdsch_slot; dlsch_id++) {
    output_ptr += do_one_dlsch(output_ptr, gNB, msgTx->dlsch[dlsch_id], slot);
  }
}

void dump_pdsch_stats(FILE *fd, PHY_VARS_gNB *gNB)
{
  for (int i = 0; i < MAX_MOBILES_PER_GNB; i++) {
    NR_gNB_PHY_STATS_t *stats = &gNB->phy_stats[i];
    if (stats->active && stats->frame != stats->dlsch_stats.dump_frame) {
      stats->dlsch_stats.dump_frame = stats->frame;
      fprintf(fd,
              "DLSCH RNTI %x: current_Qm %d, current_RI %d, total_bytes TX %d\n",
              stats->rnti,
              stats->dlsch_stats.current_Qm,
              stats->dlsch_stats.current_RI,
              stats->dlsch_stats.total_bytes_tx);
    }
  }
}
