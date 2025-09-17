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

#include "tools_defs.h"
#include "PHY/sse_intrin.h"

// returns the complex dot product of x and y

/*! \brief Complex number dot_product
@param x input vector
@param y input vector
@param N size of vectors
@param output_shift normalization of int multiplications
*/

c32_t dot_product(const c16_t *x, const c16_t *y, const uint32_t N, const int output_shift)
{

  c32_t ret;

  const c16_t *end = x + N;

#if defined(__x86_64__) || defined(__i386__)
  if (__builtin_cpu_supports("avx2")) {
    
    simde__m256i cumul_re = simde_mm256_setzero_si256();
    simde__m256i cumul_im = simde_mm256_setzero_si256();

    simde__m256i *x256 = (simde__m256i *)x;
    simde__m256i *y256 = (simde__m256i *)y;

    for (int i = 0; i < N >> 3; i++ ) {
      const simde__m256i in1 = simde_mm256_loadu_si256(&x256[i]);
      const simde__m256i in2 = simde_mm256_loadu_si256(&y256[i]);

      const simde__m256i tmpRe = oai_mm256_smadd(in1, in2, output_shift);
      const simde__m256i tmpIm = oai_mm256_smadd(oai_mm256_swap(oai_mm256_conj(in1)), in2, output_shift);
      //const simde__m256i tmpIm = oai_mm256_smadd(in1, oai_mm256_conj(oai_mm256_swap(in2)), output_shift); // alternative way

      cumul_re = simde_mm256_add_epi32(cumul_re, tmpRe);
      cumul_im = simde_mm256_add_epi32(cumul_im, tmpIm);
    }

    // this gives Re Re Im Im Re Re Im Im
    const simde__m256i cumulTmp = simde_mm256_hadd_epi32(cumul_re, cumul_im);
    const simde__m256i cumul    = simde_mm256_hadd_epi32(cumulTmp, cumulTmp);

    ret.r = simde_mm256_extract_epi32(cumul, 0) + simde_mm256_extract_epi32(cumul, 4);
    ret.i = simde_mm256_extract_epi32(cumul, 1) + simde_mm256_extract_epi32(cumul, 5);
    
    // update pointers
    x += (N & ~7);
    y += (N & ~7);
    
  }
#endif
  
  // tail processing
  if (x!=end) {
    for ( ; x <end; x++,y++ ) {
      ret.r += ((x->r*y->r)>>output_shift) + ((x->i*y->i)>>output_shift);
      ret.i += ((x->r*y->i)>>output_shift) - ((x->i*y->r)>>output_shift);
    }
  }
  return ret;
}

#ifdef MAIN
//gcc -DMAIN openair1/PHY/TOOLS/cdot_prod.c -Iopenair1 -I. -Iopenair2/COMMON -march=native -lm
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>

void main(void)
{
  const int multiply_reduction=15;
  const int arraySize=16;
  const int signalAmplitude=3000;

  c32_t result={0};
  cd_t resDouble={0};
  c16_t x[arraySize] __attribute__((aligned(16)));
  c16_t y[arraySize] __attribute__((aligned(16)));
  int fd=open("/dev/urandom",0);
  read(fd,x,sizeof(x));
  read(fd,y,sizeof(y));
  close(fd);
  for (int i=0; i<arraySize; i++) {
    x[i].r%=signalAmplitude;
    x[i].i%=signalAmplitude;
    y[i].r%=signalAmplitude;
    y[i].i%=signalAmplitude;
    resDouble.r+=x[i].r*(double)y[i].r+x[i].i*(double)y[i].i;
    resDouble.i+=x[i].r*(double)y[i].i-x[i].i*(double)y[i].r;
  }
  resDouble.r/=pow(2,multiply_reduction);
  resDouble.i/=pow(2,multiply_reduction);

  result = dot_product(x,y,8*2,15);

  printf("result = %d (double: %f), %d (double %f)\n", result.r, resDouble.r,  result.i, resDouble.i);
}
#endif
