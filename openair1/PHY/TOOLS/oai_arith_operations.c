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

#include <simde/x86/avx2.h>

#define SHUFFLE_MASK SIMDE_MM_SHUFFLE(3, 1, 2, 0) // 0xD8

// Helper function for 128-bit vectors
static inline simde__m128i
oai_mm_presort_complex(simde__m128i x) {
  return simde_mm_shuffle_epi32(simde_mm_shufflehi_epi16(simde_mm_shufflelo_epi16(x, SHUFFLE_MASK), SHUFFLE_MASK), SHUFFLE_MASK);
}

// Helper function for 256-bit vectors
static inline simde__m256i
oai_mm256_presort_complex(simde__m256i x) {
  return simde_mm256_shuffle_epi32(simde_mm256_shufflehi_epi16(simde_mm256_shufflelo_epi16(x, SHUFFLE_MASK), SHUFFLE_MASK), SHUFFLE_MASK);
}

void oai_mm_separate_real_imag_parts(simde__m128i *out_re, simde__m128i *out_im, simde__m128i in0, simde__m128i in1)
{
  // presort real and imag part
  simde__m128i xmm0 = oai_mm_presort_complex(in0);
  simde__m128i xmm1 = oai_mm_presort_complex(in1);
  
  if (out_re)
    *out_re = simde_mm_unpacklo_epi64(xmm0, xmm1);

  if (out_im)
    *out_im = simde_mm_unpackhi_epi64(xmm0, xmm1);

}

void oai_mm256_separate_real_imag_parts(simde__m256i *out_re, simde__m256i *out_im, simde__m256i in0, simde__m256i in1)
{
  
  // presort
  simde__m256i xmm0 = oai_mm256_presort_complex(in0);
  simde__m256i xmm1 = oai_mm256_presort_complex(in1);
 
  if (out_re)
    *out_re = simde_mm256_permute4x64_epi64(simde_mm256_unpacklo_epi64(xmm0, xmm1), SHUFFLE_MASK);

  if (out_im)
    *out_im = simde_mm256_permute4x64_epi64(simde_mm256_unpackhi_epi64(xmm0, xmm1), SHUFFLE_MASK);

}

// Function to separate the real and imaginary parts from the combined 256-bit vector
//__attribute__((always_inline)) static inline
void oai_mm256_separate_vectors(simde__m256i combined, simde__m128i *re, simde__m128i *im) {

  // presort
  simde__m256i xmm0 = oai_mm256_presort_complex(combined);
    
  // Unpack the low and high parts of the combined vector to extract real and imaginary parts
  simde__m128i lo = simde_mm256_extractf128_si256(xmm0, 0);  // Extract the lower 128 bits (real part)
  simde__m128i hi = simde_mm256_extractf128_si256(xmm0, 1);  // Extract the higher 128 bits (imaginary part)
    
  // Now we need to separate the real and imaginary parts within each 128-bit part.
  simde__m128i xmmre = simde_mm_unpacklo_epi16(lo, hi); // Real part of the low half
  simde__m128i xmmim = simde_mm_unpackhi_epi16(lo, hi); // Imaginary part of the high half

  *re = oai_mm_presort_complex(xmmre);
  *im = oai_mm_presort_complex(xmmim);

}

// Function to perform interleaving and combining re and im to a complex vector
//__attribute__((always_inline)) static inline
void oai_mm256_combine_vectors(simde__m128i re, simde__m128i im, simde__m256i *combined) {
  *combined = simde_mm256_set_m128i(
    simde_mm_unpackhi_epi16(re, im),
    simde_mm_unpacklo_epi16(re, im)
  );
}

#undef SHUFFLE_MASK
