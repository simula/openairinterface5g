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

void init_byte2m128i(void);

typedef struct {
  int size;
  int ports;
  int kprime;
  int lprime;
  int j[16];
  int koverline[16];
  int loverline[16];
} csi_mapping_parms_t;
csi_mapping_parms_t get_csi_mapping_parms(int row, int b, int l0, int l1);
int get_cdm_group_size(int cdm_type);
void nr_qpsk_llr(int32_t *rxdataF_comp, int16_t *llr, uint32_t nb_re);
void nr_16qam_llr(int32_t *rxdataF_comp, c16_t *ch_mag_in, int16_t *llr, uint32_t nb_re);
void nr_64qam_llr(int32_t *rxdataF_comp, c16_t *ch_mag, c16_t *ch_mag2, int16_t *llr, uint32_t nb_re);
void nr_256qam_llr(int32_t *rxdataF_comp, c16_t *ch_mag, c16_t *ch_mag2, c16_t *ch_mag3, int16_t *llr, uint32_t nb_re);
void freq2time(uint16_t ofdm_symbol_size, int16_t *freq_signal, int16_t *time_signal);
void nr_est_delay(int ofdm_symbol_size, const c16_t *ls_est, c16_t *ch_estimates_time, delay_t *delay);
unsigned int nr_get_tx_amp(int power_dBm, int power_max_dBm, int total_nb_rb, int nb_rb);
void nr_fo_compensation(double fo_Hz, int samples_per_ms, int sample_offset, const c16_t *rxdata_in, c16_t *rxdata_out, int size);
void nr_channel_level(const int symbol,
                      const int size_est,
                      const c16_t ch_estimates_ext[][size_est],
                      const int nb_rx,
                      const int Nl,
                      int32_t avg[nb_rx * Nl],
                      const uint32_t len);
void nr_generate_csi_rs(const NR_DL_FRAME_PARMS *frame_parms,
                        const csi_mapping_parms_t *phy_csi_parms,
                        const int16_t amp,
                        const int slot,
                        const uint8_t freq_density,
                        const uint16_t start_rb,
                        const uint16_t nr_of_rbs,
                        const uint8_t symb_l0,
                        const uint8_t symb_l1,
                        const uint8_t row,
                        const uint16_t scramb_id,
                        const uint8_t power_control_offset_ss,
                        const uint8_t cdm_type,
                        c16_t **dataF);

#endif
