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

#include "nr_modulation.h"
#include "PHY/NR_REFSIG/nr_mod_table.h"
#include "executables/softmodem-common.h"
#include <simde/x86/avx512.h>
// Lacking declaration in present simde external package, will be detected as compilation error when they will add it
#define simde_mm512_extracti64x2_epi64(a...) _mm512_extracti64x2_epi64(a)

// #define DEBUG_DLSCH_PRECODING_PRINT_WITH_TRIVIAL // TODO: For debug, to be removed if want to merge to develop
// #define DEBUG_LAYER_MAPPING
#define USE_NEON
// #define USE_GATHER
//  Table 6.3.1.5-1 Precoding Matrix W 1 layer 2 antenna ports 'n' = -1 and 'o' = -j
const char nr_W_1l_2p[6][2][1] = {
    {{'1'}, {'0'}}, // pmi 0
    {{'0'}, {'1'}},
    {{'1'}, {'1'}},
    {{'1'}, {'n'}},
    {{'1'}, {'j'}},
    {{'1'}, {'o'}} // pmi 5
};

// Table 6.3.1.5-3 Precoding Matrix W 1 layer 4 antenna ports 'n' = -1 and 'o' = -j
const char nr_W_1l_4p[28][4][1] = {
    {{'1'}, {'0'}, {'0'}, {'0'}}, // pmi 0
    {{'0'}, {'1'}, {'0'}, {'0'}},
    {{'0'}, {'0'}, {'1'}, {'0'}},
    {{'0'}, {'0'}, {'0'}, {'1'}},
    {{'1'}, {'0'}, {'1'}, {'0'}},
    {{'1'}, {'0'}, {'n'}, {'0'}},
    {{'1'}, {'0'}, {'j'}, {'0'}},
    {{'1'}, {'0'}, {'o'}, {'0'}}, // pmi 7
    {{'0'}, {'1'}, {'0'}, {'1'}}, // pmi 8
    {{'0'}, {'1'}, {'0'}, {'n'}},
    {{'0'}, {'1'}, {'0'}, {'j'}},
    {{'0'}, {'1'}, {'0'}, {'o'}},
    {{'1'}, {'1'}, {'1'}, {'1'}},
    {{'1'}, {'1'}, {'j'}, {'j'}},
    {{'1'}, {'1'}, {'n'}, {'n'}},
    {{'1'}, {'1'}, {'o'}, {'o'}},
    {{'1'}, {'j'}, {'1'}, {'j'}}, // pmi
    // 16
    {{'1'}, {'j'}, {'j'}, {'n'}},
    {{'1'}, {'j'}, {'n'}, {'o'}},
    {{'1'}, {'j'}, {'o'}, {'1'}},
    {{'1'}, {'n'}, {'1'}, {'n'}},
    {{'1'}, {'n'}, {'j'}, {'o'}},
    {{'1'}, {'n'}, {'n'}, {'1'}},
    {{'1'}, {'n'}, {'o'}, {'j'}}, // pmi 23
    {{'1'}, {'o'}, {'1'}, {'o'}}, // pmi 24
    {{'1'}, {'o'}, {'j'}, {'1'}},
    {{'1'}, {'o'}, {'n'}, {'j'}},
    {{'1'}, {'o'}, {'o'}, {'n'}} // pmi 27
};

// Table 6.3.1.5-4 Precoding Matrix W 2 antenna ports layers 2  'n' = -1 and 'o' = -j
const char nr_W_2l_2p[3][2][2] = {
    {{'1', '0'}, {'0', '1'}}, // pmi 0
    {{'1', '1'}, {'1', 'n'}},
    {{'1', '1'}, {'j', 'o'}} // pmi 2
};

// Table 6.3.1.5-5 Precoding Matrix W 2 layers 4 antenna ports 'n' = -1 and 'o' = -j
const char nr_W_2l_4p[22][4][2] = {
    {{'1', '0'}, {'0', '1'}, {'0', '0'}, {'0', '0'}}, // pmi 0
    {{'1', '0'}, {'0', '0'}, {'0', '1'}, {'0', '0'}}, {{'1', '0'}, {'0', '0'}, {'0', '0'}, {'0', '1'}},
    {{'0', '0'}, {'1', '0'}, {'0', '1'}, {'0', '0'}}, // pmi 3
    {{'0', '0'}, {'1', '0'}, {'0', '0'}, {'0', '1'}}, // pmi 4
    {{'0', '0'}, {'0', '0'}, {'1', '0'}, {'0', '1'}}, {{'1', '0'}, {'0', '1'}, {'1', '0'}, {'0', 'o'}},
    {{'1', '0'}, {'0', '1'}, {'1', '0'}, {'0', 'j'}}, {{'1', '0'}, {'0', '1'}, {'o', '0'}, {'0', '1'}}, // pmi 8
    {{'1', '0'}, {'0', '1'}, {'o', '0'}, {'0', 'n'}}, {{'1', '0'}, {'0', '1'}, {'n', '0'}, {'0', 'o'}},
    {{'1', '0'}, {'0', '1'}, {'n', '0'}, {'0', 'j'}}, // pmi 11
    {{'1', '0'}, {'0', '1'}, {'j', '0'}, {'0', '1'}}, // pmi 12
    {{'1', '0'}, {'0', '1'}, {'j', '0'}, {'0', 'n'}}, {{'1', '1'}, {'1', '1'}, {'1', 'n'}, {'1', 'n'}},
    {{'1', '1'}, {'1', '1'}, {'j', 'o'}, {'j', 'o'}}, // pmi 15
    {{'1', '1'}, {'j', 'j'}, {'1', 'n'}, {'j', 'o'}}, // pmi 16
    {{'1', '1'}, {'j', 'j'}, {'j', 'o'}, {'n', '1'}}, {{'1', '1'}, {'n', 'n'}, {'1', 'n'}, {'n', '1'}},
    {{'1', '1'}, {'n', 'n'}, {'j', 'o'}, {'o', 'j'}}, // pmi 19
    {{'1', '1'}, {'o', 'o'}, {'1', 'n'}, {'o', 'j'}}, {{'1', '1'}, {'o', 'o'}, {'j', 'o'}, {'1', 'n'}} // pmi 21
};

// Table 6.3.1.5-6 Precoding Matrix W 3 layers 4 antenna ports 'n' = -1 and 'o' = -j
const char nr_W_3l_4p[7][4][3] = {{{'1', '0', '0'}, {'0', '1', '0'}, {'0', '0', '1'}, {'0', '0', '0'}}, // pmi 0
                                  {{'1', '0', '0'}, {'0', '1', '0'}, {'1', '0', '0'}, {'0', '0', '1'}},
                                  {{'1', '0', '0'}, {'0', '1', '0'}, {'n', '0', '0'}, {'0', '0', '1'}},
                                  {{'1', '1', '1'}, {'1', 'n', '1'}, {'1', '1', 'n'}, {'1', 'n', 'n'}}, // pmi 3
                                  {{'1', '1', '1'}, {'1', 'n', '1'}, {'j', 'j', 'o'}, {'j', 'o', 'o'}}, // pmi 4
                                  {{'1', '1', '1'}, {'n', '1', 'n'}, {'1', '1', 'n'}, {'n', '1', '1'}},
                                  {{'1', '1', '1'}, {'n', '1', 'n'}, {'j', 'j', 'o'}, {'o', 'j', 'j'}}};

// Table 6.3.1.5-7 Precoding Matrix W 4 layers 4 antenna ports 'n' = -1 and 'o' = -j
const char nr_W_4l_4p[5][4][4] = {
    {{'1', '0', '0', '0'}, {'0', '1', '0', '0'}, {'0', '0', '1', '0'}, {'0', '0', '0', '1'}}, // pmi 0
    {{'1', '1', '0', '0'}, {'0', '0', '1', '1'}, {'1', 'n', '0', '0'}, {'0', '0', '1', 'n'}},
    {{'1', '1', '0', '0'}, {'0', '0', '1', '1'}, {'j', 'o', '0', '0'}, {'0', '0', 'j', 'o'}},
    {{'1', '1', '1', '1'}, {'1', 'n', '1', 'n'}, {'1', '1', 'n', 'n'}, {'1', 'n', 'n', '1'}}, // pmi 3
    {{'1', '1', '1', '1'}, {'1', 'n', '1', 'n'}, {'j', 'j', 'o', 'o'}, {'j', 'o', 'o', 'j'}} // pmi 4
};

void nr_modulation(const uint32_t *in, uint32_t length, uint16_t mod_order, int16_t *out)
{
  uint16_t mask = ((1 << mod_order) - 1);
  int32_t *nr_mod_table32;
  int32_t *out32 = (int32_t *)out;
  const uint8_t *in_bytes = (const uint8_t *)in;
  const uint64_t *in64 = (const uint64_t *)in;
  int64_t *out64 = (int64_t *)out;
  uint32_t i = 0;

#if defined(__SSE2__)
  simde__m128i *nr_mod_table128;
  simde__m128i *out128;
#endif

  LOG_D(PHY, "nr_modulation: length %d, mod_order %d\n", length, mod_order);

  switch (mod_order) {
#if defined(__SSE2__)
    case 2:
      nr_mod_table128 = (simde__m128i *)nr_qpsk_byte_mod_table;
      out128 = (simde__m128i *)out;
      for (i = 0; i < length / 8; i++)
        out128[i] = nr_mod_table128[in_bytes[i]];
      // the bits that are left out
      i = i * 8 / 2;
      nr_mod_table32 = (int32_t *)nr_qpsk_mod_table;
      while (i < length / 2) {
        const int idx = ((in_bytes[(i * 2) / 8] >> ((i * 2) & 0x7)) & mask);
        out32[i] = nr_mod_table32[idx];
        i++;
      }
      return;
#else
    case 2:
      nr_mod_table32 = (int32_t *)nr_qpsk_mod_table;
      for (i = 0; i < length / mod_order; i++) {
        const int idx = ((in[i * 2 / 32] >> ((i * 2) & 0x1f)) & mask);
        out32[i] = nr_mod_table32[idx];
      }
      return;
#endif

    case 4:
      out64 = (int64_t *)out;
      for (i = 0; i < length / 8; i++)
        out64[i] = nr_16qam_byte_mod_table[in_bytes[i]];
      // the bits that are left out
      i = i * 8 / 4;
      while (i < length / 4) {
        const int idx = ((in_bytes[(i * 4) / 8] >> ((i * 4) & 0x7)) & mask);
        out32[i] = nr_16qam_mod_table[idx];
        i++;
      }
      return;

    case 6:
      if (length > (3 * 64))
        for (i = 0; i < length - 3 * 64; i += 3 * 64) {
          uint64_t x = *in64++;
          uint64_t x1 = x & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x1 = (x >> 12) & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x1 = (x >> 24) & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x1 = (x >> 36) & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x1 = (x >> 48) & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          uint64_t x2 = (x >> 60);
          x = *in64++;
          x2 |= x << 4;
          x1 = x2 & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x1 = (x2 >> 12) & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x1 = (x2 >> 24) & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x1 = (x2 >> 36) & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x1 = (x2 >> 48) & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x2 = ((x >> 56) & 0xf0) | (x2 >> 60);
          x = *in64++;
          x2 |= x << 8;
          x1 = x2 & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x1 = (x2 >> 12) & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x1 = (x2 >> 24) & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x1 = (x2 >> 36) & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x1 = (x2 >> 48) & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x2 = ((x >> 52) & 0xff0) | (x2 >> 60);
          *out64++ = nr_64qam_mod_table[x2];
        }

      while (i + 24 <= length) {
        uint32_t xx = 0;
        memcpy(&xx, in_bytes + i / 8, 3);
        uint64_t x1 = xx & 0xfff;
        *out64++ = nr_64qam_mod_table[x1];
        x1 = (xx >> 12) & 0xfff;
        *out64++ = nr_64qam_mod_table[x1];
        i += 24;
      }
      if (i != length) {
        uint32_t xx = 0;
        memcpy(&xx, in_bytes + i / 8, 2);
        uint64_t x1 = xx & 0xfff;
        *out64++ = nr_64qam_mod_table[x1];
      }
      return;

    case 8:
      nr_mod_table32 = (int32_t *)nr_256qam_mod_table;
      for (i = 0; i < length / 8; i++)
        out32[i] = nr_mod_table32[in_bytes[i]];
      return;

    default:
      break;
  }
  AssertFatal(false, "Invalid or unsupported modulation order %d\n", mod_order);
}

void nr_layer_mapping(int nbCodes,
                      int encoded_len,
                      c16_t mod_symbs[nbCodes][encoded_len],
                      uint8_t n_layers,
                      int layerSz,
                      uint32_t n_symbs,
                      c16_t tx_layers[][layerSz])
{
  LOG_D(PHY, "Doing layer mapping for %d layers, %d symbols\n", n_layers, n_symbs);
  c16_t *mod = mod_symbs[0];
  switch (n_layers) {
    case 1:
      memcpy(tx_layers[0], mod, n_symbs * sizeof(**mod_symbs));
      break;

    case 2: {
      int i = 0;
      c16_t *tx0 = tx_layers[0];
      c16_t *tx1 = tx_layers[1];
#if defined(__AVX512BW__)
      simde__m512i perm2a = simde_mm512_set_epi32(30, 28, 26, 24, 22, 20, 18, 16, 14, 12, 10, 8, 6, 4, 2, 0);
      simde__m512i perm2b = simde_mm512_set_epi32(31, 29, 27, 25, 23, 21, 19, 17, 15, 13, 11, 9, 7, 5, 3, 1);
      for (; i < (n_symbs & ~31); i += 32) {
        simde__m512i a = *(simde__m512i *)(mod + i);
        simde__m512i b = *(simde__m512i *)(mod + i + 16);
        *(simde__m512i *)tx0 = simde_mm512_permutex2var_epi32(a, perm2a, b);
        *(simde__m512i *)tx1 = simde_mm512_permutex2var_epi32(a, perm2b, b);
        tx0 += 16;
        tx1 += 16;
      }
#endif
#ifdef __AVX2__
      simde__m256i perm2 = simde_mm256_set_epi32(7, 5, 3, 1, 6, 4, 2, 0);
      for (; i < (n_symbs & ~7); i += 8) {
        simde__m256i d = simde_mm256_permutevar8x32_epi32(*(simde__m256i *)(mod + i), perm2);
        *(simde__m128i *)tx0 = simde_mm256_extractf128_si256(d, 0);
        *(simde__m128i *)tx1 = simde_mm256_extractf128_si256(d, 1);
        tx0 += 4;
        tx1 += 4;
      }
#endif
#if defined(__aarch64__) && defined(USE_NEON)
      // SIMDe doesn't handle this properly, gcc up to 14.2 neither
      uint8_t const perm0[16] = {0, 1, 2, 3, 8, 9, 10, 11, 4, 5, 6, 7, 12, 13, 14, 15};
      uint8x16_t perm = vld1q_u8(perm0);
      uint8x16_t d;
      for (; i < (n_symbs & (~3)); i += 4) {
        d = vqtbl1q_u8(*(uint8x16_t *)(mod + i), perm);
        *(int64_t *)tx0 = vgetq_lane_u64((uint64x2_t)d, 0);
        *(int64_t *)tx1 = vgetq_lane_u64((uint64x2_t)d, 1);
        tx0 += 2;
        tx1 += 2;
      }
#endif
      for (; i < n_symbs; i += 2) {
        *tx0++ = mod[i];
        *tx1++ = mod[i + 1];
      }
    } break;
    case 3: {
      int i = 0;
      c16_t *tx0 = tx_layers[0];
      c16_t *tx1 = tx_layers[1];
      c16_t *tx2 = tx_layers[2];
#if defined(__AVX512BW__)
      simde__m512i perm3_0 = simde_mm512_set_epi32(13 + 16,
                                                   10 + 16,
                                                   7 + 16,
                                                   4 + 16,
                                                   1 + 16,
                                                   14 + 16,
                                                   11 + 16,
                                                   8 + 16,
                                                   5 + 16,
                                                   2 + 16,
                                                   15,
                                                   12,
                                                   9,
                                                   6,
                                                   3,
                                                   0);
      simde__m512i perm3_0b = simde_mm512_set_epi32(13 + 16, 10 + 16, 7 + 16, 4 + 16, 1 + 16, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
      simde__m512i perm3_1 = simde_mm512_set_epi32(14 + 16,
                                                   11 + 16,
                                                   8 + 16,
                                                   5 + 16,
                                                   2 + 16,
                                                   15 + 16,
                                                   12 + 16,
                                                   9 + 16,
                                                   6 + 16,
                                                   3 + 16,
                                                   0 + 16,
                                                   13,
                                                   10,
                                                   7,
                                                   4,
                                                   1);
      simde__m512i perm3_1b = simde_mm512_set_epi32(14 + 16, 11 + 16, 8 + 16, 5 + 16, 2 + 16, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
      simde__m512i perm3_2 = simde_mm512_set_epi32(15 + 16,
                                                   12 + 16,
                                                   9 + 16,
                                                   6 + 16,
                                                   3 + 16,
                                                   0 + 16,
                                                   13 + 16,
                                                   10 + 16,
                                                   7 + 16,
                                                   4 + 16,
                                                   1 + 16,
                                                   14,
                                                   11,
                                                   8,
                                                   5,
                                                   2);
      simde__m512i perm3_2b = simde_mm512_set_epi32(15 + 16, 12 + 16, 9 + 16, 6 + 16, 3 + 16, 0 + 16, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
      for (; i < (n_symbs & ~63); i += 48) {
        simde__m512i i0 = *(simde__m512i *)(mod + i);
        simde__m512i i1 = *(simde__m512i *)(mod + i + 16);
        simde__m512i i2 = *(simde__m512i *)(mod + i + 32);
        simde__m512i d0 = simde_mm512_permutex2var_epi32(i0, perm3_0, i1);
        *(simde__m512i *)tx0 = simde_mm512_permutex2var_epi32(d0, perm3_0b, i2); // 11000000
        tx0 += 16;
        d0 = simde_mm512_permutex2var_epi32(i0, perm3_1, i1);
        *(simde__m512i *)tx1 = simde_mm512_permutex2var_epi32(d0, perm3_1b, i2); // 11000000
        tx1 += 16;
        d0 = simde_mm512_permutex2var_epi32(i0, perm3_2, i1);
        *(simde__m512i *)tx2 = simde_mm512_permutex2var_epi32(d0, perm3_2b, i2); // 11000000
        tx2 += 16;
      }
#endif
#ifdef __AVX2__
      {
        simde__m256i perm3_0 = simde_mm256_set_epi32(5, 2, 7, 4, 1, 6, 3, 0);
        simde__m256i perm3_1 = simde_mm256_set_epi32(6, 3, 0, 5, 2, 7, 4, 1);
        simde__m256i perm3_2 = simde_mm256_set_epi32(7, 4, 1, 6, 3, 0, 5, 2);
        for (; i < (n_symbs & ~31); i += 24) {
          simde__m256i i0 = *(simde__m256i *)(mod + i);
          simde__m256i i1 = *(simde__m256i *)(mod + i + 8);
          simde__m256i i2 = *(simde__m256i *)(mod + i + 16);
          simde__m256i d0 = simde_mm256_permutevar8x32_epi32(i0, perm3_0);
          simde__m256i d1 = simde_mm256_permutevar8x32_epi32(i1, perm3_0);
          simde__m256i d2 = simde_mm256_permutevar8x32_epi32(i2, perm3_0);
          simde__m256i d3 = simde_mm256_blend_epi32(d0, d1, 0x38); // 00111000
          *(simde__m256i *)tx0 = simde_mm256_blend_epi32(d3, d2, 0xc0); // 11000000
          tx0 += 8;
          d0 = simde_mm256_permutevar8x32_epi32(i0, perm3_1);
          d1 = simde_mm256_permutevar8x32_epi32(i1, perm3_1);
          d2 = simde_mm256_permutevar8x32_epi32(i2, perm3_1);
          d3 = simde_mm256_blend_epi32(d0, d1, 0x18); // 00011000
          *(simde__m256i *)tx1 = simde_mm256_blend_epi32(d3, d2, 0xe0); // 11100000
          tx1 += 8;
          d0 = simde_mm256_permutevar8x32_epi32(i0, perm3_2);
          d1 = simde_mm256_permutevar8x32_epi32(i1, perm3_2);
          d2 = simde_mm256_permutevar8x32_epi32(i2, perm3_2);
          d3 = simde_mm256_blend_epi32(d0, d1, 0x1c); // 00011100
          *(simde__m256i *)tx2 = simde_mm256_blend_epi32(d3, d2, 0xe0); // 11100000
          tx2 += 8;
        }
      }
#endif
      for (; i < n_symbs; i += 3) {
        *tx0++ = mod[i];
        *tx1++ = mod[i + 1];
        *tx2++ = mod[i + 2];
      }

#ifdef DEBUG_LAYER_MAPPING
      printf("\nsymb %d/%u\n", i << 3, n_symbs);
      printf(" layer 0:\t");
      for (int j = 0; j < 8 * 6; j += 6) {
        printf("%d %d ", ((int16_t *)&mod[i << 3])[j], ((int16_t *)&mod[i << 3])[j + 1]);
      }
      printf("\n layer 1:\t");
      for (int j = 2; j < 8 * 6; j += 6) {
        printf("%d %d ", ((int16_t *)&mod[i << 3])[j], ((int16_t *)&mod[i << 3])[j + 1]);
      }
      printf("\n layer 2:\t");
      for (int j = 4; j < 8 * 6; j += 6) {
        printf("%d %d ", ((int16_t *)&mod[i << 3])[j], ((int16_t *)&mod[i << 3])[j + 1]);
      }
      printf("\n Mapping layer 0:\t");
      for (int j = 0; j < 16; j++) {
        printf("%d ", ((int16_t *)&tx_layers[0][n << 3])[j]);
      }
      printf("\n Mapping layer 1:\t");
      for (int j = 0; j < 16; j++) {
        printf("%d ", ((int16_t *)&tx_layers[1][n << 3])[j]);
      }
      printf("\n Mapping layer 2:\t");
      for (int j = 0; j < 16; j++) {
        printf("%d ", ((int16_t *)&tx_layers[2][n << 3])[j]);
      }
#endif
    } break;

    case 4: {
      int i = 0;
      c16_t *tx0 = tx_layers[0];
      c16_t *tx1 = tx_layers[1];
      c16_t *tx2 = tx_layers[2];
      c16_t *tx3 = tx_layers[3];
#if defined(__AVX512BW__)
      simde__m512i perm4 = simde_mm512_set_epi32(15, 11, 7, 3, 14, 10, 6, 2, 13, 9, 5, 1, 12, 8, 4, 0);
      for (; i < (n_symbs & ~15); i += 16) {
        simde__m512i e = simde_mm512_permutexvar_epi32(perm4, *(simde__m512i *)(mod + i));
        *(simde__m128i *)tx0 = simde_mm512_extracti64x2_epi64(e, 0);
        tx0 += 4;
        *(simde__m128i *)tx1 = simde_mm512_extracti64x2_epi64(e, 1);
        tx1 += 4;
        *(simde__m128i *)tx2 = simde_mm512_extracti64x2_epi64(e, 2);
        tx2 += 4;
        *(simde__m128i *)tx3 = simde_mm512_extracti64x2_epi64(e, 3);
        tx3 += 4;
      }
#endif
#ifdef __AVX2__
      {
        simde__m256i perm4 = simde_mm256_set_epi32(7, 3, 6, 2, 5, 1, 4, 0);
        for (; i < (n_symbs & ~7); i += 8) {
          simde__m256i e = simde_mm256_permutevar8x32_epi32(*(simde__m256i *)(mod + i), perm4);
          *(uint64_t *)tx0 = simde_mm256_extract_epi64(e, 0);
          tx0 += 2;
          *(uint64_t *)tx1 = simde_mm256_extract_epi64(e, 1);
          tx1 += 2;
          *(uint64_t *)tx2 = simde_mm256_extract_epi64(e, 2);
          tx2 += 2;
          *(uint64_t *)tx3 = simde_mm256_extract_epi64(e, 3);
          tx3 += 2;
        }
      }
#endif
#if defined(__aarch64__) && defined(USE_NEON)
      // SIMDe doesn't handle this properly, gcc up to 14.2 neither
      for (; i < (n_symbs & ~3); i += 4) {
        uint32x4_t d4 = *(uint32x4_t *)(mod + i);
        *(uint32_t *)tx0 = vgetq_lane_u32(d4, 0); 
	tx0++;
        *(uint32_t *)tx1 = vgetq_lane_u32(d4, 1); 
	tx1++;
        *(uint32_t *)tx2 = vgetq_lane_u32(d4, 0); 
	tx2++;
        *(uint32_t *)tx3 = vgetq_lane_u32(d4, 1); 
	tx3++;
      }
#endif
      for (; i < n_symbs; i += 4) {
        *tx0++ = mod[i];
        *tx1++ = mod[i + 1];
        *tx2++ = mod[i + 2];
        *tx3++ = mod[i + 3];
      }
    } break;

    case 5:
    case 6:
    case 7:
    case 8:
      /*
      // Layer 0,1
      for (int i = 0; i < n_symbs; i += 2) {
const int txIdx = i / 2;
tx_layer[0][txIdx] = mod_symbs[0][i];
tx_layer[1][txIdx] = mod_symbs[0][i + 1];
      }
      // layers 2,3,4
      else
for (int i = 0; i < n_symbs; i += 3) {
const int txIdx = i / 3;
tx_layer[2][txIdx] = mod_symbs[1][i + 2];
tx_layer[3][txIdx] = mod_symbs[1][i + 3];
tx_layer[4][txIdx] = mod_symbs[1][i + 4];
}
      break;

case 6:
      for (int q=0; q<2; q++)
for (int i = 0; i < n_symbs; i += 3) {
const int txIdx = i / 3;
tx_layer[0][txIdx] = mod_symbs[q][i + layer];
tx_layer[1][txIdx] = mod_symbs[q][i + layer];
tx_layer[2][txIdx] = mod_symbs[q][i + layer];
tx_layer[3][txIdx] = mod_symbs[q][i + layer];
tx_layer[4][txIdx] = mod_symbs[q][i + layer];
tx_layer[5][txIdx] = mod_symbs[q][i + layer];
}
      break;

case 7:
      if (layer < 3)
for (int i = 0; i < n_symbs; i += 3) {
const int txIdx = i / 3;
tx_layer[txIdx] = mod_symbs[1][i + layer];
}
      else
for (int i = 0; i < n_symbs; i += 4) {
const int txIdx = i / 4;
tx_layer[txIdx] = mod_symbs[0][i + layer];
}
      break;

case 8:
      for (int q=0; q<2; q++)
      for (int i = 0; i < n_symbs; i += 4) {
const int txIdx = i / 4;
tx_layer[txIdx] = mod_symbs[q][i + layer];
      }
      break;
*/
    default:
      AssertFatal(0, "Invalid number of layers %d\n", n_layers);
  }
}

void nr_ue_layer_mapping(const c16_t *mod_symbs, const int n_layers, const int n_symbs, c16_t tx_layers[][n_symbs])
{
  for (int l = 0; l < n_layers; l++) {
    for (int i = 0; i < n_symbs; i++) {
      tx_layers[l][i] = c16mulRealShift(mod_symbs[n_layers * i + l], AMP, 15);
    }
  }
}

void nr_dft(c16_t *z, c16_t *d, uint32_t Msc_PUSCH)
{
  simde__m128i dft_in128[3240], dft_out128[3240];
  c16_t *dft_in0 = (c16_t *)dft_in128, *dft_out0 = (c16_t *)dft_out128;

  uint32_t i, ip;

  simde__m128i norm128;

  if ((Msc_PUSCH % 1536) > 0) {
    for (i = 0, ip = 0; i < Msc_PUSCH; i++, ip += 4) {
      dft_in0[ip] = d[i];
    }
  }
  dft_size_idx_t dftsize = get_dft(Msc_PUSCH);
  switch (Msc_PUSCH) {
    case 12:
      dft(dftsize, (int16_t *)dft_in0, (int16_t *)dft_out0, 0);
      norm128 = simde_mm_set1_epi16(9459);
      for (i = 0; i < 12; i++) {
        ((simde__m128i *)dft_out0)[i] = simde_mm_slli_epi16(simde_mm_mulhi_epi16(((simde__m128i *)dft_out0)[i], norm128), 1);
      }

      break;
    default:
      dft(dftsize, (int16_t *)dft_in0, (int16_t *)dft_out0, 1);
      break;
  }

  if ((Msc_PUSCH % 1536) > 0) {
    for (i = 0, ip = 0; i < Msc_PUSCH; i++, ip += 4)
      z[i] = dft_out0[ip];
  }
}

void perform_symbol_rotation(NR_DL_FRAME_PARMS *fp, double f0, c16_t *symbol_rotation)
{
  const int nsymb = fp->symbols_per_slot * fp->slots_per_frame / 10;
  const double Tc = (1 / 480e3 / 4096);
  const double Nu = 2048 * 64 * (1 / (float)(1 << fp->numerology_index));
  const double Ncp0 = 16 * 64 + (144 * 64 * (1 / (float)(1 << fp->numerology_index)));
  const double Ncp1 = (144 * 64 * (1 / (float)(1 << fp->numerology_index)));

  LOG_D(PHY, "Doing symbol rotation calculation for TX/RX, f0 %f Hz, Nsymb %d\n", f0, nsymb);

  double tl = 0.0;
  double poff = 0.0;
  double exp_re = 0.0;
  double exp_im = 0.0;

  for (int l = 0; l < nsymb; l++) {
    double Ncp;
    if (l == 0 || l == (7 * (1 << fp->numerology_index))) {
      Ncp = Ncp0;
    } else {
      Ncp = Ncp1;
    }

    poff = 2 * M_PI * (tl + (Ncp * Tc)) * f0;
    exp_re = cos(poff);
    exp_im = sin(-poff);
    symbol_rotation[l].r = (int16_t)floor(exp_re * 32767);
    symbol_rotation[l].i = (int16_t)floor(exp_im * 32767);

    LOG_D(PHY,
          "Symbol rotation %d/%d => tl %f (%d,%d) (%f)\n",
          l,
          nsymb,
          tl,
          symbol_rotation[l].r,
          symbol_rotation[l].i,
          (poff / 2 / M_PI) - floor(poff / 2 / M_PI));

    tl += (Nu + Ncp) * Tc;
  }
}

void init_symbol_rotation(NR_DL_FRAME_PARMS *fp)
{
  double f[2] = {(double)fp->dl_CarrierFreq, (double)fp->ul_CarrierFreq};

  for (int ll = 0; ll < 2; ll++) {
    double f0 = f[ll];
    if (f0 == 0)
      continue;
    c16_t *rot = fp->symbol_rotation[ll];

    perform_symbol_rotation(fp, f0, rot);
  }
}

void init_timeshift_rotation(NR_DL_FRAME_PARMS *fp)
{
  const int sample_offset = fp->nb_prefix_samples / fp->ofdm_offset_divisor;
  for (int i = 0; i < fp->ofdm_symbol_size; i++) {
    double poff = -i * 2.0 * M_PI * sample_offset / fp->ofdm_symbol_size;
    double exp_re = cos(poff);
    double exp_im = sin(-poff);
    fp->timeshift_symbol_rotation[i].r = (int16_t)round(exp_re * 32767);
    fp->timeshift_symbol_rotation[i].i = (int16_t)round(exp_im * 32767);

    if (i < 10)
      LOG_D(PHY,
            "Timeshift symbol rotation %d => (%d,%d) %f\n",
            i,
            fp->timeshift_symbol_rotation[i].r,
            fp->timeshift_symbol_rotation[i].i,
            poff);
  }
}

c16_t nr_layer_precoder(int sz, c16_t datatx_F_precoding[][sz], const char *prec_matrix, uint8_t n_layers, int32_t re_offset)
{
  c16_t precodatatx_F = {0};

  for (int al = 0; al < n_layers; al++) {
    c16_t antenna = datatx_F_precoding[al][re_offset];
    switch (prec_matrix[al]) {
      case '0': // multiply by zero
        break;

      case '1': // multiply by 1
        precodatatx_F = c16add(precodatatx_F, antenna);
        break;

      case 'n': // multiply by -1
        precodatatx_F = c16sub(precodatatx_F, antenna);
        break;

      case 'j': //
        precodatatx_F.r -= antenna.i;
        precodatatx_F.i += antenna.r;
        break;

      case 'o': // -j
        precodatatx_F.r += antenna.i;
        precodatatx_F.i -= antenna.r;
        break;
    }
  }

  return precodatatx_F;
  // normalize
  /*  ((int16_t *)precodatatx_F)[0] = (int16_t)((((int16_t *)precodatatx_F)[0]*ONE_OVER_SQRT2_Q15)>>15);
      ((int16_t *)precodatatx_F)[1] = (int16_t)((((int16_t *)precodatatx_F)[1]*ONE_OVER_SQRT2_Q15)>>15);*/
}

c16_t nr_layer_precoder_cm(int n_layers,
                           int symSz,
                           c16_t datatx_F_precoding[n_layers][symSz],
                           int ap,
                           nfapi_nr_pm_pdu_t *pmi_pdu,
                           int offset)
{
  c16_t precodatatx_F = {0};
  for (int al = 0; al < n_layers; al++) {
    nfapi_nr_pm_weights_t *w = &pmi_pdu->weights[al][ap];
    c16_t prec_weight = {.r = w->precoder_weight_Re, .i = w->precoder_weight_Im};
    precodatatx_F = c16maddShift(datatx_F_precoding[al][offset], prec_weight, precodatatx_F, 15);
  }
  return precodatatx_F;
}

void nr_layer_precoder_simd(const int n_layers,
                            const int symSz,
                            const c16_t txdataF_res_mapped[n_layers][symSz],
                            const int ant,
                            const nfapi_nr_pm_pdu_t *pmi_pdu,
                            const int sc_offset,
                            const int re_cnt,
                            c16_t *txdataF_precoded)
{
  uint32_t sc = sc_offset;
  c16_t prec_weight = {0};
  // For x86, use 256 SIMD for every 8 RE and 128 SIMD for last 4 RE
  // For aarch64, use 128 SIMD for every 4 RE

  // 256 SIMD: Do 8 RE in one iteration, 3 iterations for 2 RB
#ifdef __AVX2__
  const uint32_t re_cnt_align8 = re_cnt & ~7;
  for (; sc < sc_offset + (re_cnt_align8); sc += sizeof(simde__m256i) / sizeof(prec_weight)) {
    // Matrix multiplication for 4 elements of the result (sizeof(simde__m256i) / sizeof(*prec_matrix) = 8)
    simde__m256i y = simde_mm256_set1_epi16(0); // Y = W[0]*X[0] + W[1]*X[1] + ... + W[nrOfLayers-1]*X[nrOfLayers-1]
    for (int nl = 0; nl < n_layers; nl++) {
      prec_weight.r = pmi_pdu->weights[nl][ant].precoder_weight_Re;
      prec_weight.i = pmi_pdu->weights[nl][ant].precoder_weight_Im;

      const simde__m256i x = simde_mm256_loadu_si256(&txdataF_res_mapped[nl][sc]);

      // Rearrange precoding matrix weight to match complex multiplication and broadcast it to match SIMD size
      const simde__m256i w_c = simde_mm256_set1_epi32(c16toI32(c16conj(prec_weight))); // broadcast conjugate of w
      const simde__m256i w_s = simde_mm256_set1_epi32(c16toI32(c16swap(prec_weight))); // broadcast swapped real and img of w

      // Multiplication and shift
      const simde__m256i reals =
          simde_mm256_srai_epi32(simde_mm256_madd_epi16(x, w_c), 15); // (int32_t) .r = (x.r * w.r - x.i * w.i) >> 15
      const simde__m256i imags =
          simde_mm256_slli_epi32(simde_mm256_madd_epi16(x, w_s),  1); // (int32_t) .i = (x.r * w.i + x.i * w.r) << 1, since higher 16 bit of each 32 bit is taken by blend_epi16

      // Re-arrange to match c16_t format
      const simde__m256i produ = simde_mm256_blend_epi16(reals, imags, 0xAA);

      // Accumulate the product
      y = simde_mm256_adds_epi16(y, produ);
    }
    // Store the result to txdataF
    simde_mm256_storeu_si256(&txdataF_precoded[sc], y);
  }
#endif

  // 128 SIMD: Do 4 RE in one iteration, 3 iterations for 1 RB
  const uint32_t re_cnt_align4 = re_cnt & ~3;
  for (; sc < sc_offset + re_cnt_align4; sc += sizeof(simde__m128i) / sizeof(prec_weight)) {
#ifdef DEBUG_DLSCH_PRECODING_PRINT_WITH_TRIVIAL // Get result with trivial solution, TODO: To be removed
    c16_t y_triv[4];
    for (int i = 0; i < 4; i++)
      y_triv[i] = nr_layer_precoder_cm(n_layers, symSz, txdataF_res_mapped, ant, pmi_pdu, sc + i);
    memcpy(&txdataF_precoded[sc], y_triv, sizeof(y_triv));
#endif

    // Matrix multiplication for 4 elements of the result (sizeof(simde__m128i) / sizeof(c16_t) = 4)
    simde__m128i y = simde_mm_set1_epi16(0); // Y = W[0]*X[0] + W[1]*X[1] + ... + W[nrOfLayers-1]*X[nrOfLayers-1]
    for (int nl = 0; nl < n_layers; nl++) {
      prec_weight.r = pmi_pdu->weights[nl][ant].precoder_weight_Re;
      prec_weight.i = pmi_pdu->weights[nl][ant].precoder_weight_Im;

      const simde__m128i x = simde_mm_loadu_si128(&txdataF_res_mapped[nl][sc]);

      // Rearrange precoding matrix weight to match complex multiplication and broadcast it to match SIMD size
      const simde__m128i w_c = simde_mm_set1_epi32(c16toI32(c16conj(prec_weight))); // broadcast conjugate of w
      const simde__m128i w_s = simde_mm_set1_epi32(c16toI32(c16swap(prec_weight))); // broadcast swapped real and img of w

      // Multiplication and shift
      const simde__m128i reals =
          simde_mm_srai_epi32(simde_mm_madd_epi16(x, w_c), 15); // (int32_t) .r = (x.r * w.r - x.i * w.i) >> 15
      const simde__m128i imags = simde_mm_slli_epi32(
          simde_mm_madd_epi16(x, w_s),
          1); // (int32_t) .i = (x.r * w.i + x.i * w.r) << 1, since higher 16 bit of each 32 bit is taken by blend_epi16

      /* Re-arrange to match c16_t format
         bit index: 0            | 16              | 32           | 48              | 64           | 80              | 96 | 112
         reals =   {R0.r[15..30] | R0.r[31] (0)*15 | R1.r[15..30] | R1.r[31] (0)*15 | R2.r[15..30] | R2.r[31] (0)*15 | R3.r[15..30]
         | R3.r[31] (0)*15} imags =   {0 R0.i[0..14]| R0.i[15..30]    | 0 R1.i[0..14]| R1.i[15..30]    | 0 R2.i[0..14]| R2.i[15..30]
         | 0 R3.i[0..14]| R3.i[15..30]   } 16b from  {reals        | imags           | reals        | imags | reals | imags | reals
         | imags          } produ =   {R0.r[15..30] | R0.i[15..30]    | R1.r[15..30] | R1.i[15..30] | R2.r[15..30] | R2.i[15..30] |
         R3.r[15..30] | R3.i[15..30]   }
      */
      const simde__m128i produ = simde_mm_blend_epi16(reals, imags, 0xAA);

      // Accumulate the product
      y = simde_mm_adds_epi16(y, produ);
    }
    // Store the result to txdataF
    simde_mm_storeu_si128(&txdataF_precoded[sc], y);

#ifdef DEBUG_DLSCH_PRECODING_PRINT_WITH_TRIVIAL // Print simd and trivial result, TODO: To be removed
    c16_t *y_simd = (c16_t *)&y;
    printf("debug_to_be_removed re_cnt=%d, sc=%u, y_simd=(%+4d,%+4d), (%+4d,%+4d), (%+4d,%+4d), (%+4d,%+4d)\n",
           re_cnt,
           sc,
           y_simd[0].r,
           y_simd[0].i,
           y_simd[1].r,
           y_simd[1].i,
           y_simd[2].r,
           y_simd[2].i,
           y_simd[3].r,
           y_simd[3].i);
    printf("debug_to_be_removed re_cnt=%d, sc=%u, y_triv=(%+4d,%+4d), (%+4d,%+4d), (%+4d,%+4d), (%+4d,%+4d)\n",
           re_cnt,
           sc,
           y_triv[0].r,
           y_triv[0].i,
           y_triv[1].r,
           y_triv[1].i,
           y_triv[2].r,
           y_triv[2].i,
           y_triv[3].r,
           y_triv[3].i);
#endif
  }
}
