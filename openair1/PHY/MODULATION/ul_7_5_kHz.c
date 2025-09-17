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

#include "PHY/defs_eNB.h"
#include <math.h>
#include "PHY/sse_intrin.h"
#include "modulation_extern.h"

void remove_7_5_kHz(RU_t *ru,uint8_t slot)
{

  c16_t **rxdata=(c16_t **)ru->common.rxdata;
  c16_t **rxdata_7_5kHz=(c16_t **)ru->common.rxdata_7_5kHz;
  int16_t *kHz7_5;

  LTE_DL_FRAME_PARMS *frame_parms=ru->frame_parms;

  switch (frame_parms->N_RB_UL) {

  case 6:
    kHz7_5 = (frame_parms->Ncp==0) ? s6n_kHz_7_5 : s6e_kHz_7_5;
    break;

  case 15:
    kHz7_5 = (frame_parms->Ncp==0) ? s15n_kHz_7_5 : s15e_kHz_7_5;
    break;

  case 25:
    kHz7_5 = (frame_parms->Ncp==0) ? s25n_kHz_7_5 : s25e_kHz_7_5;
    break;

  case 50:
    kHz7_5 = (frame_parms->Ncp==0) ? s50n_kHz_7_5 : s50e_kHz_7_5;
    break;

  case 75:
    kHz7_5 = (frame_parms->Ncp==0) ? s75n_kHz_7_5 : s75e_kHz_7_5;
    break;

  case 100:
    kHz7_5 = (frame_parms->Ncp==0) ? s100n_kHz_7_5 : s100e_kHz_7_5;
    break;

  default:
    kHz7_5 = (frame_parms->Ncp==0) ? s25n_kHz_7_5 : s25e_kHz_7_5;
    break;
  }

  uint32_t slot_offset = ((uint32_t)slot * frame_parms->samples_per_tti/2)-ru->N_TA_offset;
  uint32_t slot_offset2 = (uint32_t)(slot&1) * frame_parms->samples_per_tti/2;

  uint16_t len = frame_parms->samples_per_tti/2;

  for (uint8_t aa=0; aa<ru->nb_rx; aa++) {
    c16_t *rxptr = rxdata[aa]+slot_offset;
    c16_t *rxptr_7_5kHz = rxdata_7_5kHz[aa]+slot_offset2;
    // apply 7.5 kHz
    mult_complex_vectors((c16_t*)kHz7_5,rxptr,rxptr_7_5kHz,len, 15);
    // undo 7.5 kHz offset for symbol 3 in case RU is slave (for OTA synchronization)
    if (ru->is_slave == 1 && slot == 2){
      int offset=3*frame_parms->ofdm_symbol_size+2*frame_parms->nb_prefix_samples+frame_parms->nb_prefix_samples0;
      memcpy(rxdata_7_5kHz[aa]+offset,
             rxdata[aa]+offset,
             (frame_parms->ofdm_symbol_size + frame_parms->nb_prefix_samples)*sizeof(c16_t));
    }
  }
}
