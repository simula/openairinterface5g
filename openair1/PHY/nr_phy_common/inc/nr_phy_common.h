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

#ifndef __NR_PHY_COMMON__H__
#define __NR_PHY_COMMON__H__
#include "PHY/impl_defs_top.h"
#include "PHY/TOOLS/tools_defs.h"
#include "PHY/NR_REFSIG/nr_refsig_common.h"
#include "PHY/MODULATION/nr_modulation.h"

typedef struct {
  int size;
  int ports;
  int kprime;
  int lprime;
  int j[16];
  int koverline[16];
  int loverline[16];
} csi_mapping_parms_t;
void get_modulated_csi_symbols(int symbols_per_slot,
                               int slot,
                               int N_RB_DL,
                               int mod_length,
                               int16_t mod_csi[][(N_RB_DL << 4) >> 1],
                               int lprime,
                               int l0,
                               int l1,
                               int row,
                               int scramb_id);
void csi_rs_resource_mapping(int32_t **dataF,
                             int csi_rs_length,
                             int16_t mod_csi[][csi_rs_length >> 1],
                             int ofdm_symbol_size,
                             int dataF_offset,
                             int start_sc,
                             const csi_mapping_parms_t *mapping_parms,
                             int start_rb,
                             int nb_rbs,
                             double alpha,
                             int beta,
                             double rho,
                             int gs,
                             int freq_density);
csi_mapping_parms_t get_csi_mapping_parms(int row, int b, int l0, int l1);
int get_cdm_group_size(int cdm_type);
double get_csi_rho(int freq_density);
uint32_t get_csi_beta_amplitude(const int16_t amp, int power_control_offset_ss);
int get_csi_modulation_length(double rho, int freq_density, int kprime, int start_rb, int nb_rbs);
void nr_qpsk_llr(int32_t *rxdataF_comp, int16_t *llr, uint32_t nb_re);
void nr_16qam_llr(int32_t *rxdataF_comp, int32_t *ch_mag_in, int16_t *llr, uint32_t nb_re);
void nr_64qam_llr(int32_t *rxdataF_comp, int32_t *ch_mag, int32_t *ch_mag2, int16_t *llr, uint32_t nb_re);
void nr_256qam_llr(int32_t *rxdataF_comp, int32_t *ch_mag, int32_t *ch_mag2, int32_t *ch_mag3, int16_t *llr, uint32_t nb_re);
void freq2time(uint16_t ofdm_symbol_size, int16_t *freq_signal, int16_t *time_signal);
void nr_est_delay(int ofdm_symbol_size, const c16_t *ls_est, c16_t *ch_estimates_time, delay_t *delay);
unsigned int nr_get_tx_amp(int power_dBm, int power_max_dBm, int total_nb_rb, int nb_rb);
#endif
