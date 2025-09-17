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

#include "PHY/defs_UE.h"
#include "PHY/phy_extern_ue.h"
#include "modulation_extern.h"
#include <math.h>
#include "PHY/sse_intrin.h"

void apply_7_5_kHz(PHY_VARS_UE *ue,int32_t*txdata,uint8_t slot)
{

  uint32_t *kHz7_5ptr;
  simde__m128i *txptr128, *kHz7_5ptr128;

  LTE_DL_FRAME_PARMS *frame_parms=&ue->frame_parms;

  switch (frame_parms->N_RB_UL) {

  case 6:
    kHz7_5ptr = (frame_parms->Ncp==0) ? (uint32_t*)s6n_kHz_7_5 : (uint32_t*)s6e_kHz_7_5;
    break;

  case 15:
    kHz7_5ptr = (frame_parms->Ncp==0) ? (uint32_t*)s15n_kHz_7_5 : (uint32_t*)s15e_kHz_7_5;
    break;

  case 25:
    kHz7_5ptr = (frame_parms->Ncp==0) ? (uint32_t*)s25n_kHz_7_5 : (uint32_t*)s25e_kHz_7_5;
    break;

  case 50:
    kHz7_5ptr = (frame_parms->Ncp==0) ? (uint32_t*)s50n_kHz_7_5 : (uint32_t*)s50e_kHz_7_5;
    break;

  case 75:
    kHz7_5ptr = (frame_parms->Ncp==0) ? (uint32_t*)s75n_kHz_7_5 : (uint32_t*)s75e_kHz_7_5;
    break;

  case 100:
    kHz7_5ptr = (frame_parms->Ncp==0) ? (uint32_t*)s100n_kHz_7_5 : (uint32_t*)s100e_kHz_7_5;
    break;

  default:
    kHz7_5ptr = (frame_parms->Ncp==0) ? (uint32_t*)s25n_kHz_7_5 : (uint32_t*)s25e_kHz_7_5;
    break;
  }

  uint32_t slot_offset = (uint32_t)slot * frame_parms->samples_per_tti/2;
  uint16_t len = frame_parms->samples_per_tti/2;

  txptr128 = (simde__m128i *)&txdata[slot_offset];
  kHz7_5ptr128 = (simde__m128i *)kHz7_5ptr;
  // apply 7.5 kHz

  for (uint32_t i = 0; i < (len >> 2); i++) {
    // Real part of complex multiplication (note: 7_5kHz signal is conjugated for this to work)
    txptr128[i] = oai_mm_cpx_mult_conj(kHz7_5ptr128[i], txptr128[i], 15);
  }

}

