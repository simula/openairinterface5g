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

/*!\file ldpc_encode_parity_check.c
 * \brief Parity check function used by ldpc encoders
 * \author Florian Kaltenberger, Raymond Knopp, Kien le Trung (Eurecom)
 * \email openair_tech@eurecom.fr
 * \date 27-03-2018
 * \version 1.0
 * \note
 * \warning
 */

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "assertions.h"
#include "common/utils/LOG/log.h"

#define USE_PERMUTEX
#ifdef __aarch64__
#define USE_ALIGNR
#endif
#ifdef __AVX512F__
#if defined(__AVX512VBMI__) && defined(USE_PERMUTEX)
// For AVX512 machines, use an AVX512 version of the encoder for Zc=384 only for now. This is used almost exclusively for
// high-throughput cases. The version with "permutex2var" instruction uses less memory (i.e. 1/64th of the memory to store the
// input), but uses more reads instead of creating 64 shifts of the input with memcpy
#include "ldpc384_simd512_permutex_byte.c"
#else
#include "ldpc384_simd512_byte.c"
#endif
#else
#include "ldpc384_byte.c"
#endif
#ifdef USE_ALIGNR
#include "ldpc384_alignr_byte_128.c"
#else
#include "ldpc384_byte_128.c"
#endif
#include "ldpc352_byte.c"
#include "ldpc352_byte_128.c"
#include "ldpc320_byte.c"
#include "ldpc320_byte_128.c"
#include "ldpc288_byte.c"
#include "ldpc288_byte_128.c"
#include "ldpc256_byte.c"
#include "ldpc256_byte_128.c"
#include "ldpc240_byte.c"
#include "ldpc224_byte.c"
#include "ldpc224_byte_128.c"
#include "ldpc208_byte.c"
#include "ldpc192_byte.c"
#include "ldpc192_byte_128.c"
#include "ldpc176_byte.c"
#include "ldpc_BG2_Zc384_byte.c"
#include "ldpc_BG2_Zc384_byte_128.c"
#include "ldpc_BG2_Zc352_byte.c"
#include "ldpc_BG2_Zc352_byte_128.c"
#include "ldpc_BG2_Zc320_byte.c"
#include "ldpc_BG2_Zc320_byte_128.c"
#include "ldpc_BG2_Zc288_byte.c"
#include "ldpc_BG2_Zc288_byte_128.c"
#include "ldpc_BG2_Zc256_byte.c"
#include "ldpc_BG2_Zc256_byte_128.c"
#include "ldpc_BG2_Zc240_byte.c"
#include "ldpc_BG2_Zc224_byte.c"
#include "ldpc_BG2_Zc224_byte_128.c"
#include "ldpc_BG2_Zc208_byte.c"
#include "ldpc_BG2_Zc192_byte.c"
#include "ldpc_BG2_Zc192_byte_128.c"
#include "ldpc_BG2_Zc176_byte.c"
#include "ldpc_BG2_Zc160_byte_128.c"
#include "ldpc_BG2_Zc160_byte.c"
#include "ldpc_BG2_Zc144_byte.c"
#include "ldpc_BG2_Zc128_byte.c"
#include "ldpc_BG2_Zc128_byte_128.c"
#include "ldpc_BG2_Zc120_byte.c"
#include "ldpc_BG2_Zc112_byte.c"
#include "ldpc_BG2_Zc104_byte.c"
#include "ldpc_BG2_Zc96_byte.c"
#include "ldpc_BG2_Zc96_byte_128.c"
#include "ldpc_BG2_Zc88_byte.c"
#include "ldpc_BG2_Zc80_byte.c"
#include "ldpc_BG2_Zc72_byte.c"

static void encode_parity_check_part_optim(uint8_t *cc, uint8_t *d, short BG,short Zc,short Kb, int simd_size, int ncols)
{
  unsigned char c[2 * 22 * Zc * simd_size] __attribute__((aligned(64))); //double size matrix of c
  for (int i1 = 0; i1 < ncols; i1++)   {
    memcpy(&c[2 * i1 * Zc], &cc[i1 * Zc], Zc * sizeof(unsigned char));
    memcpy(&c[(2 * i1 + 1) * Zc], &cc[i1 * Zc], Zc * sizeof(unsigned char));
  }
#if (defined(USE_PERMUTEX) && defined(__AVX512VBMI__)) || defined(USE_ALIGNR)
  if (Zc < 384 || BG == 2)
#endif
  {
    for (int i1 = 1; i1 < simd_size; i1++) {
      memcpy(&c[(2 * ncols * Zc * i1)], &c[i1], (2 * ncols * Zc * sizeof(unsigned char)) - i1);
    }
  }
  if (BG == 1) {
    switch (Zc) {
      case 176:
        ldpc176_byte(c, d);
        break;
      case 192:
        ldpc192_byte(c, d);
        break;
      case 208:
        ldpc208_byte(c, d);
        break;
      case 224:
        ldpc224_byte(c, d);
        break;
      case 240:
        ldpc240_byte(c, d);
        break;
      case 256:
        ldpc256_byte(c, d);
        break;
      case 288:
        ldpc288_byte(c, d);
        break;
      case 320:
        ldpc320_byte(c, d);
        break;
      case 352:
        ldpc352_byte(c, d);
        break;
      case 384:
        ldpc384_byte(c, d);
        break;
      default:
        AssertFatal(false, "BG %d Zc %d is not supported yet\n", BG, Zc);
    }
  } else if (BG == 2) {
    switch (Zc) {
      case 72:
        ldpc_BG2_Zc72_byte(c, d);
        break;
      case 80:
        ldpc_BG2_Zc80_byte(c, d);
        break;
      case 88:
        ldpc_BG2_Zc88_byte(c, d);
        break;
      case 96:
        ldpc_BG2_Zc96_byte(c, d);
        break;
      case 104:
        ldpc_BG2_Zc104_byte(c, d);
        break;
      case 112:
        ldpc_BG2_Zc112_byte(c, d);
        break;
      case 120:
        ldpc_BG2_Zc120_byte(c, d);
        break;
      case 128:
        ldpc_BG2_Zc128_byte(c, d);
        break;
      case 144:
        ldpc_BG2_Zc144_byte(c, d);
        break;
      case 160:
        ldpc_BG2_Zc160_byte(c, d);
        break;
      case 176:
        ldpc_BG2_Zc176_byte(c, d);
        break;
      case 192:
        ldpc_BG2_Zc192_byte(c, d);
        break;
      case 208:
        ldpc_BG2_Zc208_byte(c, d);
        break;
      case 224:
        ldpc_BG2_Zc224_byte(c, d);
        break;
      case 240:
        ldpc_BG2_Zc240_byte(c, d);
        break;
      case 256:
        ldpc_BG2_Zc256_byte(c, d);
        break;
      case 288:
        ldpc_BG2_Zc288_byte(c, d);
        break;
      case 320:
        ldpc_BG2_Zc320_byte(c, d);
        break;
      case 352:
        ldpc_BG2_Zc352_byte(c, d);
        break;
      case 384:
        ldpc_BG2_Zc384_byte(c, d);
        break;
      default:
        AssertFatal(false , "BG %d Zc %d is not supported yet\n", BG, Zc);
    }
  } else
    AssertFatal(false, "BG %d is not supported\n", BG);
}



