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

/*! \file ru_procedures.c
 * \brief Implementation of RU procedures
 * \author R. Knopp, F. Kaltenberger, N. Nikaein, X. Foukas
 * \date 2011
 * \version 0.1
 * \company Eurecom
 * \email: knopp@eurecom.fr,florian.kaltenberger@eurecom.fr,navid.nikaein@eurecom.fr, x.foukas@sms.ed.ac.uk
 * \note
 * \warning
 */

#include "PHY/defs_gNB.h"
#include "sched_nr.h"
#include "PHY/MODULATION/modulation_common.h"
#include "PHY/MODULATION/nr_modulation.h"

#include "common/utils/LOG/log.h"
#include "common/utils/system.h"
#include "common/utils/LOG/vcd_signal_dumper.h"

#include "T.h"

#include "assertions.h"

#include <time.h>

// RU OFDM Modulator gNodeB
// OFDM modulation core routine, generates a first_symbol to first_symbol+num_symbols on a particular slot and TX antenna port
void nr_feptx0(RU_t *ru, int tti_tx, int first_symbol, int num_symbols, int aa)
{
  NR_DL_FRAME_PARMS *fp = ru->nr_frame_parms;

  unsigned int slot_offset,slot_offsetF;
  int slot = tti_tx;

  //VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_RU_FEPTX_OFDM+(first_symbol!=0?1:0) , 1 );

  if (aa == 0 && first_symbol == 0)
    start_meas(&ru->ofdm_mod_stats);
  slot_offset  = fp->get_samples_slot_timestamp(slot, fp, 0);
  slot_offsetF = first_symbol * fp->ofdm_symbol_size;

  int abs_first_symbol = slot * fp->symbols_per_slot;

  for (int idx_sym = abs_first_symbol; idx_sym < abs_first_symbol + first_symbol; idx_sym++)
    slot_offset += (idx_sym % (0x7 << fp->numerology_index)) ? fp->nb_prefix_samples : fp->nb_prefix_samples0;

  slot_offset += fp->ofdm_symbol_size * first_symbol;

  LOG_D(PHY,
        "SFN/SF:RU:TX:%d/%d aa %d Generating slot %d (first_symbol %d num_symbols %d) slot_offset %d, slot_offsetF %d\n",
        ru->proc.frame_tx,
        ru->proc.tti_tx,
        aa,
        slot,
        first_symbol,
        num_symbols,
        slot_offset,
        slot_offsetF);
  
  if (fp->Ncp == 1) {
    PHY_ofdm_mod(&ru->common.txdataF_BF[aa][slot_offsetF],
                 (int*)&ru->common.txdata[aa][slot_offset],
                 fp->ofdm_symbol_size,
                 num_symbols,
                 fp->nb_prefix_samples,
                 CYCLIC_PREFIX);
  } else {
    if (fp->numerology_index != 0) {
      
      if (!(slot%(fp->slots_per_subframe/2))&&(first_symbol==0)) { // case where first symbol in slot has longer prefix
        PHY_ofdm_mod(&ru->common.txdataF_BF[aa][slot_offsetF],
                     (int*)&ru->common.txdata[aa][slot_offset],
                     fp->ofdm_symbol_size,
                     1,
                     fp->nb_prefix_samples0,
                     CYCLIC_PREFIX);

        PHY_ofdm_mod(&ru->common.txdataF_BF[aa][slot_offsetF+fp->ofdm_symbol_size],
                     (int*)&ru->common.txdata[aa][slot_offset+fp->nb_prefix_samples0+fp->ofdm_symbol_size],
                     fp->ofdm_symbol_size,
                     num_symbols-1,
                     fp->nb_prefix_samples,
                     CYCLIC_PREFIX);
      }
      else { // all symbols in slot have shorter prefix
        PHY_ofdm_mod(&ru->common.txdataF_BF[aa][slot_offsetF],
                     (int*)&ru->common.txdata[aa][slot_offset],
                     fp->ofdm_symbol_size,
                     num_symbols,
                     fp->nb_prefix_samples,
                     CYCLIC_PREFIX);
      }
    } // numerology_index!=0
    else { //numerology_index == 0
      for (int idx_sym = abs_first_symbol; idx_sym < abs_first_symbol+num_symbols; idx_sym++) {
        if (idx_sym % 0x7) {
          PHY_ofdm_mod(&ru->common.txdataF_BF[aa][slot_offsetF],
                       (int*)&ru->common.txdata[aa][slot_offset],
                       fp->ofdm_symbol_size,
                       1,
                       fp->nb_prefix_samples,
                       CYCLIC_PREFIX);
          slot_offset += fp->nb_prefix_samples+fp->ofdm_symbol_size;
          slot_offsetF += fp->ofdm_symbol_size;
        }
        else {
          PHY_ofdm_mod(&ru->common.txdataF_BF[aa][slot_offsetF],
                       (int*)&ru->common.txdata[aa][slot_offset],
                       fp->ofdm_symbol_size,
                       1,
                       fp->nb_prefix_samples0,
                       CYCLIC_PREFIX);
          slot_offset += fp->nb_prefix_samples0+fp->ofdm_symbol_size;
          slot_offsetF += fp->ofdm_symbol_size;
        }
      } // for(idx_symbol..
    } //  numerology 0
  }

  if (aa == 0 && first_symbol == 0)
    stop_meas(&ru->ofdm_mod_stats);
        
  //VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_RU_FEPTX_OFDM+(first_symbol!=0?1:0), 0);
}

// RU FEP TX OFDM modulation, single-thread
void nr_feptx_ofdm(RU_t *ru,int frame_tx,int tti_tx) {
     
  nfapi_nr_config_request_scf_t *cfg = &ru->gNB_list[0]->gNB_config;
  NR_DL_FRAME_PARMS *fp=ru->nr_frame_parms;
  int cyclic_prefix_type = NFAPI_CP_NORMAL;

  unsigned int aa=0;
  int slot_sizeF = (fp->ofdm_symbol_size)*
                   ((cyclic_prefix_type == 1) ? 12 : 14);
  int slot = tti_tx;
  int *txdata = &ru->common.txdata[aa][fp->get_samples_slot_timestamp(slot,fp,0)];

  if (nr_slot_select(cfg,frame_tx,slot) == NR_UPLINK_SLOT) return;

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_RU_FEPTX_OFDM , 1 );


    //    LOG_D(HW,"Frame %d: Generating slot %d\n",frame,next_slot);

  nr_feptx0(ru,slot,0,NR_NUMBER_OF_SYMBOLS_PER_SLOT,aa);

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_RU_FEPTX_OFDM , 0 );

  LOG_D(PHY,"feptx_ofdm (TXPATH): frame %d, slot %d: txp (time %p) %d dB, txp (freq) %d dB\n",
	frame_tx,slot,txdata,dB_fixed(signal_energy((int32_t*)txdata,fp->get_samples_per_slot(
  slot,fp))),dB_fixed(signal_energy_nodc((c16_t*)ru->common.txdataF_BF[aa],2*slot_sizeF)));

}

void nr_feptx_prec(RU_t *ru, int frame_tx, int slot_tx)
{
  PHY_VARS_gNB **gNB_list = ru->gNB_list;
  AssertFatal(ru->num_gNB == 1, "Cannot handle more than 1 gNB\n");
  PHY_VARS_gNB *gNB = gNB_list[0];
  nfapi_nr_config_request_scf_t *cfg = &ru->gNB_list[0]->gNB_config;
  NR_DL_FRAME_PARMS *fp = ru->nr_frame_parms;
  int txdataF_offset = slot_tx * fp->samples_per_slot_wCP;
  start_meas(&ru->precoding_stats);

  if (nr_slot_select(cfg,frame_tx,slot_tx) == NR_UPLINK_SLOT)
    return;

  if (gNB->common_vars.analog_bf) {
    for (int i = 0; i < ru->num_beams_period; i++) {
      memcpy((void*) &ru->common.beam_id[i][slot_tx * fp->symbols_per_slot],
             (void*) &gNB->common_vars.beam_id[i][slot_tx * fp->symbols_per_slot],
             (fp->symbols_per_slot) * sizeof(int));
    }
  }

  // If there is no digital beamforming we just need to copy the data to RU
  if (ru->config.dbt_config.num_dig_beams == 0 || ru->gNB_list[0]->common_vars.analog_bf) {
    for (int b = 0; b < ru->num_beams_period; b++) {
      for (int i = 0; i < ru->nb_tx; ++i) {
        int tx_idx = i + b * ru->nb_tx;
        memcpy((void*)ru->common.txdataF_BF[tx_idx],
               (void*)&gNB->common_vars.txdataF[b][i][txdataF_offset],
               fp->samples_per_slot_wCP * sizeof(int32_t));
      }
    }
  }  else {
    AssertFatal(false, "This needs to be fixed by using appropriate beams from config\n");
  }
  stop_meas(&ru->precoding_stats);
}

// core routine for FEP TX, called from threads in RU TX thread-pool 
void nr_feptx(void *arg)
{
  feptx_cmd_t *feptx = (feptx_cmd_t *)arg;

  RU_t *ru = feptx->ru;
  int slot = feptx->slot;
  int aa = feptx->aid;
  int bb = feptx->beam;
  int startSymbol = feptx->startSymbol;
  NR_DL_FRAME_PARMS *fp = ru->nr_frame_parms;
  int numSymbols = feptx->numSymbols;
  int numSamples = feptx->numSymbols * fp->ofdm_symbol_size;
  int txdataF_offset = (slot * fp->samples_per_slot_wCP) + startSymbol * fp->ofdm_symbol_size;
  int txdataF_BF_offset = startSymbol * fp->ofdm_symbol_size;

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_RU_FEPTX_PREC+feptx->aid , 1);
  int tx_idx = aa + bb * ru->nb_tx;

  if (tx_idx == 0)
    start_meas(&ru->precoding_stats);

  if (ru->gNB_list[0]->common_vars.analog_bf) {
    memcpy(&ru->common.beam_id[bb][slot * fp->symbols_per_slot],
           &ru->gNB_list[0]->common_vars.beam_id[bb][slot * fp->symbols_per_slot],
           (fp->symbols_per_slot) * sizeof(int));
  }

  // If there is no digital beamforming we just need to copy the data to RU
  if (ru->config.dbt_config.num_dig_beams == 0 || ru->gNB_list[0]->common_vars.analog_bf)
     memcpy((void*)&ru->common.txdataF_BF[tx_idx][txdataF_BF_offset],
            (void*)&ru->gNB_list[0]->common_vars.txdataF[bb][aa][txdataF_offset],
            numSamples * sizeof(int32_t));
  else {
    AssertFatal(false, "This needs to be fixed by using appropriate beams from config\n");
  }

  if (tx_idx == 0)
    stop_meas(&ru->precoding_stats);
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_RU_FEPTX_PREC+feptx->aid , 0);

      ////////////FEPTX////////////
  nr_feptx0(ru, slot, startSymbol, numSymbols, tx_idx);

  // Task completed in //
  completed_task_ans(feptx->ans);
}

// RU FEP TX using thread-pool
void nr_feptx_tp(RU_t *ru, int frame_tx, int slot)
{
  nfapi_nr_config_request_scf_t *cfg = &ru->gNB_list[0]->gNB_config;
  if (nr_slot_select(cfg, frame_tx, slot) == NR_UPLINK_SLOT)
    return;
  if (ru->idx == 0)
    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME( VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_RU_FEPTX_OFDM, 1);
  start_meas(&ru->ofdm_total_stats);

  int nt = ru->nb_tx * ru->num_beams_period;
  size_t const sz = nt + (ru->half_slot_parallelization > 0) * nt;
  feptx_cmd_t arr[sz];
  task_ans_t ans;
  init_task_ans(&ans, sz);

  int nbfeptx = 0;
  for (int beam = 0; beam < ru->num_beams_period; beam++) {
    for (int aid = 0; aid < ru->nb_tx; aid++) {
      feptx_cmd_t *feptx_cmd = &arr[nbfeptx];
      feptx_cmd->ans = &ans;
      feptx_cmd->beam = beam;
      feptx_cmd->aid = aid;
      feptx_cmd->ru = ru;
      feptx_cmd->slot = slot;
      feptx_cmd->startSymbol = 0;
      feptx_cmd->numSymbols =
          (ru->half_slot_parallelization > 0) ? ru->nr_frame_parms->symbols_per_slot >> 1 : ru->nr_frame_parms->symbols_per_slot;

      task_t t = {.func = nr_feptx, .args = feptx_cmd};
      pushTpool(ru->threadPool, t);
      nbfeptx++;
      if (ru->half_slot_parallelization > 0) {
        feptx_cmd_t *feptx_cmd = &arr[nbfeptx];
        feptx_cmd->ans = &ans;
        feptx_cmd->beam = beam;
        feptx_cmd->aid = aid;
        feptx_cmd->ru = ru;
        feptx_cmd->slot = slot;
        feptx_cmd->startSymbol = ru->nr_frame_parms->symbols_per_slot >> 1;
        feptx_cmd->numSymbols = ru->nr_frame_parms->symbols_per_slot >> 1;

        task_t t = {.func = nr_feptx, .args = feptx_cmd};
        pushTpool(ru->threadPool, t);
        nbfeptx++;
      }
    }
  }
  join_task_ans(&ans);

  stop_meas(&ru->ofdm_total_stats);
  if (ru->idx == 0)
    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME( VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_RU_FEPTX_OFDM, 0);
}

// core RX FEP routine, called by threads in RU thread-pool
void nr_fep(void* arg)
{
  feprx_cmd_t *feprx_cmd = (feprx_cmd_t *)arg;
  RU_t *ru = feprx_cmd->ru;
  int aid = feprx_cmd->aid;
  int beam = feprx_cmd->beam;
  int slot = feprx_cmd->slot;
  int startSymbol = feprx_cmd->startSymbol;
  int endSymbol = feprx_cmd->endSymbol;
  NR_DL_FRAME_PARMS *fp = ru->nr_frame_parms;
  
  LOG_D(PHY,"aid %d, frame %d slot %d, startSymbol %d, endSymbol %d\n", aid, ru->proc.frame_rx, slot, startSymbol, endSymbol);

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_RU_FEPRX+aid, 1);

  int idx = aid + beam * ru->nb_rx;
  int offset = (slot % RU_RX_SLOT_DEPTH) * fp->symbols_per_slot * fp->ofdm_symbol_size;
  for (int l = startSymbol; l <= endSymbol; l++) 
      nr_slot_fep_ul(fp,
                     ru->common.rxdata[idx],
                     &ru->common.rxdataF[idx][offset],
                     l,
                     slot,
                     ru->N_TA_offset);
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_RU_FEPRX+aid, 0);

  // Task completed in //
  completed_task_ans(feprx_cmd->ans);
}

// RU RX FEP using thread-pool
void nr_fep_tp(RU_t *ru, int slot) {

  int nbfeprx=0;
  if (ru->idx == 0) VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME( VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_RU_FEPRX, 1 );
  start_meas(&ru->ofdm_demod_stats);

  int nt = ru->nb_rx * ru->num_beams_period;
  size_t const sz = nt + (ru->half_slot_parallelization > 0) * nt;
  feprx_cmd_t arr[sz];
  task_ans_t ans;
  init_task_ans(&ans, sz);

  for (int beam = 0; beam < ru->num_beams_period; beam++) {
    for (int aid = 0; aid < ru->nb_rx; aid++) {
      feprx_cmd_t *feprx_cmd = &arr[nbfeprx];
      feprx_cmd->ans = &ans;
      feprx_cmd->beam = beam;
      feprx_cmd->aid = aid;
      feprx_cmd->ru = ru;
      feprx_cmd->slot = ru->proc.tti_rx;
      feprx_cmd->startSymbol = 0;
      feprx_cmd->endSymbol = (ru->half_slot_parallelization > 0) ? (ru->nr_frame_parms->symbols_per_slot >> 1) - 1
                                                                 : (ru->nr_frame_parms->symbols_per_slot - 1);

      task_t t = {.func = nr_fep, .args = feprx_cmd};
      pushTpool(ru->threadPool, t);
      nbfeprx++;
      if (ru->half_slot_parallelization > 0) {
        feprx_cmd_t *feprx_cmd = &arr[nbfeprx];
        feprx_cmd->ans = &ans;
        feprx_cmd->beam = beam;
        feprx_cmd->aid = aid;
        feprx_cmd->ru = ru;
        feprx_cmd->slot = ru->proc.tti_rx;
        feprx_cmd->startSymbol = ru->nr_frame_parms->symbols_per_slot >> 1;
        feprx_cmd->endSymbol = ru->nr_frame_parms->symbols_per_slot - 1;

        task_t t = {.func = nr_fep, .args = feprx_cmd};
        pushTpool(ru->threadPool, t);

        nbfeprx++;
      }
    }
  }
  join_task_ans(&ans);

  stop_meas(&ru->ofdm_demod_stats);
  if (ru->idx == 0) VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME( VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_RU_FEPRX, 0 );
}

