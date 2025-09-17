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

/*! \file PHY/sse_intrin.h
 * \brief SSE includes and compatibility functions.
 *
 * This header collects all SSE compatibility functions. To use SSE inside a source file, include only sse_intrin.h.
 * The host CPU needs to have support for SSE2 at least. SSE3 and SSE4.1 functions are emulated if the CPU lacks support for them.
 * This will slow down the softmodem, but may be valuable if only offline signal processing is required.
 *
 * 
 * Has been changed in August 2022 to rely on SIMD Everywhere (SIMDE) from MIT
 * by bruno.mongazon-cazavet@nokia-bell-labs.com
 *
 * All AVX2 code is mapped to SIMDE which transparently relies on AVX2 HW (avx2-capable host) or SIMDE emulation
 * (non-avx2-capable host).
 * To force using SIMDE emulation on avx2-capable host use the --noavx2 flag. 
 * avx512 code is not mapped to SIMDE. It depends on --noavx512 flag.
 * If the --noavx512 is set the OAI AVX512 emulation using AVX2 is used.
 * If the --noavx512 is not set, AVX512 HW is used on avx512-capable host while OAI AVX512 emulation using AVX2
 * is used on non-avx512-capable host. 
 *
 * \author S. Held, Laurent THOMAS
 * \email sebastian.held@imst.de, laurent.thomas@open-cells.com	
 * \company IMST GmbH, Open Cells Project
 * \date 2019
 * \version 0.2
*/

#ifndef SSE_INTRIN_H
#define SSE_INTRIN_H

#include <simde/simde-common.h>
#include <simde/x86/avx2.h>
#include <simde/x86/fma.h>

#if defined(__AVX512BW__) || defined(__AVX512F__)
#include <immintrin.h>
// a solution should be found to use simde package for also AVX512, but it is C++ implementation, difficult to use in OAI
typedef struct {
  union {
    __m512i v;
    int16_t i16[32];
    int8_t i8[64];
  };
} oai512_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__arm__) || defined(__aarch64__)
/* ARM processors */
// note this fails on some x86 machines, with an error like:
// /usr/lib/gcc/x86_64-redhat-linux/8/include/gfniintrin.h:57:1: error: inlining failed in call to always_inline ‘_mm_gf2p8affine_epi64_epi8’: target specific option mismatch
#include <simde/x86/clmul.h>

#endif // x86_64 || i386

#include <stdbool.h>
#include "assertions.h"

/*
 * OAI specific SSE section
 */

typedef struct {
  union {
    simde__m128i v;
    int16_t i16[8];
    int8_t i8[16];
  };
} oai128_t;
typedef struct {
  union {
    simde__m256i v;
    int16_t i16[16];
    int8_t i8[32];
  };
} oai256_t;

__attribute__((always_inline)) static inline int64_t simde_mm_average_sse(simde__m128i *a, int length, int shift)
{
  // compute average level with shift (64-bit verstion)
  simde__m128i avg128 = simde_mm_setzero_si128();
  for (int i = 0; i < length >> 2; i++) {
    const simde__m128i in1 = a[i];
    avg128 = simde_mm_add_epi32(avg128, simde_mm_srai_epi32(simde_mm_madd_epi16(in1, in1), shift));
  }

  // Horizontally add pairs
  // 1st [A + C, B + D]
  simde__m128i sum_pairs = simde_mm_add_epi64(simde_mm_unpacklo_epi32(avg128, simde_mm_setzero_si128()), // [A, B] → [A, 0, B, 0]
                                              simde_mm_unpackhi_epi32(avg128, simde_mm_setzero_si128())  // [C, D] → [C, 0, D, 0]
  );

  // 2nd [A + B + C + D, ...]
  simde__m128i total_sum = simde_mm_add_epi64(sum_pairs, simde_mm_shuffle_epi32(sum_pairs, SIMDE_MM_SHUFFLE(1, 0, 3, 2)));

  // Extract horizontal sum as a scalar int64_t result
  return simde_mm_cvtsi128_si64(total_sum);
}

__attribute__((always_inline)) static inline int64_t simde_mm_average_avx2(simde__m256i *a, int length, int shift)
{
  simde__m256i avg256 = simde_mm256_setzero_si256();
  for (int i = 0; i < length >> 3; i++) {
    const simde__m256i in1 = simde_mm256_loadu_si256(&a[i]); // unaligned load
    avg256 = simde_mm256_add_epi32(avg256, simde_mm256_srai_epi32(simde_mm256_madd_epi16(in1, in1), shift));
  }

  // Split the 256-bit vector into two 128-bit halves and convert to 64-bit
  // [A + E, B + F, C + G, D + H]
  simde__m256i sum_pairs = simde_mm256_add_epi64(
      simde_mm256_cvtepi32_epi64(simde_mm256_castsi256_si128(avg256)),     // [A, B, C, D] → [A, 0, B, 0, C, 0, D, 0]
      simde_mm256_cvtepi32_epi64(simde_mm256_extracti128_si256(avg256, 1)) // [E, F, G, H] → [E, 0, F, 0, G, 0, H, 0]
  );

  // Horizontal sum within the 256-bit vector
  // [A + E + B + F, C + G + D + H]
  simde__m128i total_sum = simde_mm_add_epi64(simde_mm256_castsi256_si128(sum_pairs), simde_mm256_extracti128_si256(sum_pairs, 1));

  // [A + E + B + F + C + G + D + H, ...]
  total_sum = simde_mm_add_epi64(total_sum, simde_mm_shuffle_epi32(total_sum, SIMDE_MM_SHUFFLE(1, 0, 3, 2)));

  // Extract horizontal sum as a scalar int64_t result
  return simde_mm_cvtsi128_si64(total_sum);
}

__attribute__((always_inline)) static inline int32_t simde_mm_average(simde__m128i *a, int length, int shift, int16_t scale)
{
  int64_t avg = 0;

#if defined(__x86_64__) || defined(__i386__)
  if (__builtin_cpu_supports("avx2")) {
    avg += simde_mm_average_avx2((simde__m256i *)a, length, shift);

    // tail processing by SSE
    a += ((length & ~7) >> 2);
    length -= (length & ~7);
  }
#endif

  avg += simde_mm_average_sse(a, length, shift);

  return (uint32_t)(avg / scale);
}

/**
 * Perform element-wise conjugation on a 128-bit SIMD vector of 16-bit integers.
 *
 * The flips the sign of imaginary part of each complex element in the vector:
 * Input:  [r0,  i0, ..., r3,  i3]
 * Output: [r0, -i0, ..., r3, -i3]
 *
 * @param 128-bit SIMD vector of 4x complex 16-bit integers.
 * @return Complex conjugated 128-bit SIMD vector.
 */
__attribute__((always_inline)) static inline simde__m128i oai_mm_conj(simde__m128i a)
{
  const oai128_t neg_imag = {.i16 = {1, -1, 1, -1, 1, -1, 1, -1}};
  return simde_mm_sign_epi16(a, neg_imag.v);
}

/**
 * Perform element-wise IQ swap on a 128-bit SIMD vector of 16-bit integers.
 *
 * The swap imag and real part of each complex element in the vector:
 * Input:  [r0, i0, ..., r3, i3]
 * Output: [i0, r0, ..., i3, r3]
 *
 * @param 128-bit SIMD vector of 16-bit integers.
 * @return Swaped 128-bit SIMD vector.
 */
__attribute__((always_inline)) static inline
simde__m128i oai_mm_swap(simde__m128i a)
{
  // Shuffle mask to swap bytes for IQ swapping
  const oai128_t shuffle_mask_swap = {.i8 = {2, 3, 0, 1, 6, 7, 4, 5, 10, 11, 8, 9, 14, 15, 12, 13}};
  return simde_mm_shuffle_epi8(a, shuffle_mask_swap.v);
}

__attribute__((always_inline)) static inline
simde__m128i oai_mm_smadd(simde__m128i z1, simde__m128i z2, int shift)
{
  return simde_mm_srai_epi32(simde_mm_madd_epi16(z1, z2), shift);
}

__attribute__((always_inline)) static inline
simde__m128i oai_mm_pack(simde__m128i a, simde__m128i b)
{
  return simde_mm_packs_epi32(
    simde_mm_unpacklo_epi32(a, b), // real
    simde_mm_unpackhi_epi32(a, b)  // imag
  );
}

/**
 * Perform a COMPLEX MULTIPLICATION on a 128-bit SIMD vector of complex 16-bit integers.
 *
 * Input:  z1 = (a + bi) [ a0,  b0,  ...,  a3,  b3]
 * Input:  z2 = (c + di) [ c0,  d0,  ...,  c3,  d3]
 * Output: z3 = (e + fi) [ e0,  f0,  ...,  e3,  f3]
 *
 * conj(z1)              [ a0, -b0,  ...,  a3, -b3]
 * swap(z1)              [ b0,  a0,  ...,  b3,  a3] 
 * 
 * z3 = z1 * z2 = + (ac-bd) + (ad+bc)i
 *
 * @param 128-bit SIMD vector of four complex 16-bit integers.
 * @return a 128-bit SIMD vector.
 */
__attribute__((always_inline)) static inline
simde__m128i oai_mm_cpx_mult(simde__m128i z1, simde__m128i z2, int shift)
{
  simde__m128i re = oai_mm_smadd(oai_mm_conj(z1), z2, shift);
  simde__m128i im = oai_mm_smadd(oai_mm_swap(z1), z2, shift);
  return oai_mm_pack(re, im);
}

/**
 * Perform a CONJUGATE multiplication on a 128-bit SIMD vector of 16-bit integers.
 *
 * Input:  z1 = (a + bi) [ a0,  b0,  ...,  a3,  b3]
 * Input:  z2 = (c + di) [ c0,  d0,  ...,  c3,  d3]
 * Output: z3 = (e + fi) [ e0,  f0,  ...,  e3,  f3]
 * z3 = z1 * conj(z2) = + (ac+bd) + i(bc-ad)
 *
 * @param 128-bit SIMD vector of four complex 16-bit integers.
 * @return a 128-bit SIMD vector.
 */
__attribute__((always_inline)) static inline
simde__m128i oai_mm_cpx_mult_conj(simde__m128i a, simde__m128i b, int shift)
{
  simde__m128i re = oai_mm_smadd(a, b, shift);
  simde__m128i im = oai_mm_smadd(oai_mm_swap(oai_mm_conj(a)), b, shift);
  return oai_mm_pack(re, im);
}

/*
 * OAI specific AVX2 section
 */

/**
 * Perform element-wise conjugation on a 256-bit SIMD vector of 16-bit integers.
 *
 * The flips the sign of imaginary part of each complex element in the vector:
 * Input:  [r0,  i0, ..., r7,  i7]
 * Output: [r0, -i0, ..., r7, -i7]
 *
 * @param 256-bit SIMD vector of 8x complex 16-bit integers.
 * @return Complex conjugated 256-bit SIMD vector.
 */
__attribute__((always_inline)) static inline simde__m256i oai_mm256_conj(simde__m256i a)
{
  const oai256_t neg_imag = {.i16 = {1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1}};
  return simde_mm256_sign_epi16(a, neg_imag.v);
}

/**
 * Perform element-wise IQ swap on a 256-bit SIMD vector of 16-bit integers.
 *
 * This swaps the real and imaginary parts of each complex element in the vector:
 * Input:  [r0, i0, ..., r7, i7]
 * Output: [i0, r0, ..., i7, r7]
 *
 * @param 256-bit SIMD vector of 16-bit integers.
 * @return Swapped 256-bit SIMD vector.
 */
__attribute__((always_inline)) static inline simde__m256i oai_mm256_swap(simde__m256i a)
{
  // Shuffle mask to swap bytes for IQ swapping
  const oai256_t shuffle_mask_swap = {.i8 = {
                                          2,  3,  0,  1,  6,  7,  4,  5,  10, 11, 8,  9,  14, 15, 12, 13, // Low bytes
                                          18, 19, 16, 17, 22, 23, 20, 21, 26, 27, 24, 25, 30, 31, 28, 29 // High bytes
                                      }};
  return simde_mm256_shuffle_epi8(a, shuffle_mask_swap.v);
}

__attribute__((always_inline)) static inline
simde__m256i oai_mm256_smadd(simde__m256i z1, simde__m256i z2, int shift)
{
  return simde_mm256_srai_epi32(simde_mm256_madd_epi16(z1, z2), shift);
}

__attribute__((always_inline)) static inline
simde__m256i oai_mm256_pack(simde__m256i a, simde__m256i b)
{
  return simde_mm256_packs_epi32(
    simde_mm256_unpacklo_epi32(a, b), // real
    simde_mm256_unpackhi_epi32(a, b)  // imag
  );
}

/**
 * Perform a COMPLEX MULTIPLICATION on a 256-bit SIMD vector of complex 16-bit integers.
 *
 * Input:  z1 = (a + bi) [ a0,  b0,  ...,  a7,  b7 ]
 * Input:  z2 = (c + di) [ c0,  d0,  ...,  c7,  d7 ]
 * Output: z3 = (e + fi) [ e0,  f0,  ...,  e7,  f7]
 *
 * conj(z1)              [ a0, -b0,  ...,  a7, -b7]
 * swap(z1)              [ b0,  a0,  ...,  b7,  a7] 
 * 
 * z3 = z1 * z2 = + (ac-bd) + (ad+bc)i
 *
 * @param 256-bit SIMD vector of eight complex 16-bit integers.
 * @return a 256-bit SIMD vector.
 */
__attribute__((always_inline)) static inline
simde__m256i oai_mm256_cpx_mult(simde__m256i z1, simde__m256i z2, int shift)
{
  simde__m256i re = oai_mm256_smadd(oai_mm256_conj(z1), z2, shift);
  simde__m256i im = oai_mm256_smadd(oai_mm256_swap(z1), z2, shift);
  return oai_mm256_pack(re, im);
}

/**
 * Perform a CONJUGATE multiplication on a 256-bit SIMD vector of complex 16-bit integers.
 *
 * Input:  z1 = (a + bi) [ a0,  b0,  ...,  a3,  b3]
 * Input:  z2 = (c + di) [ c0,  d0,  ...,  c3,  d3]
 * Output: z3 = (e + fi) [ e0,  f0,  ...,  e3,  f3]
 * z3 =  z1 * conj(z2) =  (ac+bd) + i(bc-ad)
 *
 * @param 256-bit SIMD vector of eight complex 16-bit integers.
 * @return a 256-bit SIMD vector.
 */
__attribute__((always_inline)) static inline
simde__m256i oai_mm256_cpx_mult_conj(simde__m256i a, simde__m256i b, int shift)
{
  simde__m256i re = oai_mm256_smadd(a, b, shift);
  simde__m256i im = oai_mm256_smadd(oai_mm256_swap(oai_mm256_conj(a)), b, shift);
  return oai_mm256_pack(re, im);
}

#ifdef __cplusplus
}
#endif

#endif // SSE_INTRIN_H
