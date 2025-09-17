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
#include "PHY/impl_defs_top.h"

// Compute Energy of a complex signal vector, removing the DC component!
// input  : points to vector
// length : length of vector in complex samples

#define shift 4
//#define shift_DC 0
//#define SHRT_MIN -32768

//-----------------------------------------------------------------
// Average Power calculation with DC removing
//-----------------------------------------------------------------
int32_t signal_energy(int32_t *input,uint32_t length)
{
  uint32_t i;
  int32_t temp;
  simde__m128i in, in_clp, i16_min, coe1;
  simde__m128 num0, num1, num2, num3, recp1;

  //init
  num0 = simde_mm_setzero_ps();
  num1 = simde_mm_setzero_ps();
  i16_min = simde_mm_set1_epi16(SHRT_MIN);
  coe1 = simde_mm_set1_epi16(1);
  recp1 = simde_mm_rcp_ps(simde_mm_cvtepi32_ps(simde_mm_set1_epi32(length)));

  //Acc
  for (i = 0; i < (length >> 2); i++) {
    in = simde_mm_loadu_si128((simde__m128i *)input);
    in_clp = simde_mm_subs_epi16(in, simde_mm_cmpeq_epi16(in, i16_min));//if in=SHRT_MIN in+1, else in
    num0 = simde_mm_add_ps(num0, simde_mm_cvtepi32_ps(simde_mm_madd_epi16(in_clp, in_clp)));
    num1 = simde_mm_add_ps(num1, simde_mm_cvtepi32_ps(simde_mm_madd_epi16(in, coe1)));//DC
    input += 4;
  }
  //Ave
  num2 = simde_mm_dp_ps(num0, recp1, 0xFF);//AC power
  num3 = simde_mm_dp_ps(num1, recp1, 0xFF);//DC
  num3 = simde_mm_mul_ps(num3, num3);      //DC power
  //remove DC
  temp = simde_mm_cvtsi128_si32(simde_mm_cvttps_epi32(simde_mm_sub_ps(num2, num3)));

  return temp;
}

uint32_t signal_energy_nodc(const c16_t *input, uint32_t length)
{
  // init
  simde__m128 mm0 = simde_mm_setzero_ps();

  // Acc
  for (int32_t i = 0; i < (length >> 2); i++) {
    simde__m128i in = simde_mm_loadu_si128((simde__m128i *)input);
    mm0 = simde_mm_add_ps(mm0, simde_mm_cvtepi32_ps(simde_mm_madd_epi16(in, in)));
    input += 4;
  }

  // leftover
  float leftover_sum = 0;
  c16_t *leftover_input = (c16_t *)input;
  uint16_t lefover_count = length - ((length >> 2) << 2);
  for (int32_t i = 0; i < lefover_count; i++) {
    leftover_sum += leftover_input[i].r * leftover_input[i].r + leftover_input[i].i * leftover_input[i].i;
  }

  // Ave
  float sums[4];
  simde_mm_store_ps(sums, mm0);
  return (uint32_t)((sums[0] + sums[1] + sums[2] + sums[3] + leftover_sum) / (float)length);
}

double signal_energy_fp(double *s_re[2],double *s_im[2],uint32_t nb_antennas,uint32_t length,uint32_t offset)
{

  int32_t aa,i;
  double V=0.0;

  for (i=0; i<length; i++) {
    for (aa=0; aa<nb_antennas; aa++) {
     V= V + (s_re[aa][i+offset]*s_re[aa][i+offset]) + (s_im[aa][i+offset]*s_im[aa][i+offset]);
    }
  }

  return(V/length/nb_antennas);
}

double signal_energy_fp2(struct complexd *s,uint32_t length)
{

  int32_t i;
  double V=0.0;

  for (i=0; i<length; i++) {
		          //    printf("signal_energy_fp2 : %f,%f => %f\n",s[i].x,s[i].y,V);
		  //        //      V= V + (s[i].y*s[i].x) + (s[i].y*s[i].x);
    V= V + (s[i].r*s[i].r) + (s[i].i*s[i].i);
  }
  return(V/length);
}

int32_t signal_power(int32_t *input, uint32_t length)
{

  uint32_t i;
  int32_t temp;

  simde__m128i in, in_clp, i16_min;
  simde__m128  num0, num1;
  simde__m128  recp1;

  //init
  num0 = simde_mm_setzero_ps();
  i16_min = simde_mm_set1_epi16(SHRT_MIN);
  recp1 = simde_mm_rcp_ps(simde_mm_cvtepi32_ps(simde_mm_set1_epi32(length)));
  //Acc
  for (i = 0; i < (length >> 2); i++) {
    in = simde_mm_loadu_si128((simde__m128i *)input);
    in_clp = simde_mm_subs_epi16(in, simde_mm_cmpeq_epi16(in, i16_min));//if in=SHRT_MIN in+1, else in
    num0 = simde_mm_add_ps(num0, simde_mm_cvtepi32_ps(simde_mm_madd_epi16(in_clp, in_clp)));
    input += 4;
  }
  //Ave
  num1 = simde_mm_dp_ps(num0, recp1, 0xFF);
  temp = simde_mm_cvtsi128_si32(simde_mm_cvttps_epi32(num1));

  return temp;
}

int32_t interference_power(int32_t *input, uint32_t length)
{

  uint32_t i;
  int32_t temp;

  simde__m128i in, in_clp, i16_min;
  simde__m128i num0, num1, num2, num3;
  simde__m128  num4, num5, num6;
  simde__m128  recp1;

  //init
  i16_min = simde_mm_set1_epi16(SHRT_MIN);
  num5 = simde_mm_setzero_ps();
  recp1 = simde_mm_rcp_ps(simde_mm_cvtepi32_ps(simde_mm_set1_epi32(length>>2)));// 1/n, n= length/4
  //Acc
  for (i = 0; i < (length >> 2); i++) {
    in = simde_mm_loadu_si128((simde__m128i *)input);
    in_clp = simde_mm_subs_epi16(in, simde_mm_cmpeq_epi16(in, i16_min));           //if in=SHRT_MIN, in+1, else in
    num0 = simde_mm_cvtepi16_epi32(in_clp);                                   //lower 2 complex [0], [1]
    num1 = simde_mm_cvtepi16_epi32(simde_mm_shuffle_epi32(in_clp, 0x4E));          //upper 2 complex [2], [3]
    num2 = simde_mm_srai_epi32(simde_mm_add_epi32(num0, num1), 0x01);              //average A=complex( [0] + [2] ) / 2, B=complex( [1] + [3] ) / 2 
    num3 = simde_mm_sub_epi32(num2, simde_mm_shuffle_epi32(num2, 0x4E));           //complexA-complexB, B-A
    num4 = simde_mm_dp_ps(simde_mm_cvtepi32_ps(num3), simde_mm_cvtepi32_ps(num3), 0x3F);//C = num3 lower complex power, C, C, C
    num5 = simde_mm_add_ps(num5, num4);                                       //Acc Cn, Cn, Cn, Cn, 
    input += 4;
  }
  //Interference ve
  num6 = simde_mm_mul_ps(num5, recp1); //Cn / n
  temp = simde_mm_cvtsi128_si32(simde_mm_cvttps_epi32(num6));

  return temp;
}

