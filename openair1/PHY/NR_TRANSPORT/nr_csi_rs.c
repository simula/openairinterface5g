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

#include "PHY/MODULATION/nr_modulation.h"
#include "PHY/NR_REFSIG/nr_refsig.h"

//#define NR_CSIRS_DEBUG

void nr_generate_csi_rs(const NR_DL_FRAME_PARMS *frame_parms,
                        int32_t **dataF,
                        const int16_t amp,
                        const nfapi_nr_dl_tti_csi_rs_pdu_rel15_t *csi_params,
                        const int slot,
                        const csi_mapping_parms_t *phy_csi_parms)
{
#ifdef NR_CSIRS_DEBUG
  LOG_I(NR_PHY, "csi_params->subcarrier_spacing = %i\n", csi_params->subcarrier_spacing);
  LOG_I(NR_PHY, "csi_params->cyclic_prefix = %i\n", csi_params->cyclic_prefix);
  LOG_I(NR_PHY, "csi_params->start_rb = %i\n", csi_params->start_rb);
  LOG_I(NR_PHY, "csi_params->nr_of_rbs = %i\n", csi_params->nr_of_rbs);
  LOG_I(NR_PHY, "csi_params->csi_type = %i (0:TRS, 1:CSI-RS NZP, 2:CSI-RS ZP)\n", csi_params->csi_type);
  LOG_I(NR_PHY, "csi_params->row = %i\n", csi_params->row);
  LOG_I(NR_PHY, "csi_params->freq_domain = %i\n", csi_params->freq_domain);
  LOG_I(NR_PHY, "csi_params->symb_l0 = %i\n", csi_params->symb_l0);
  LOG_I(NR_PHY, "csi_params->symb_l1 = %i\n", csi_params->symb_l1);
  LOG_I(NR_PHY, "csi_params->cdm_type = %i\n", csi_params->cdm_type);
  LOG_I(NR_PHY, "csi_params->freq_density = %i (0: dot5 (even RB), 1: dot5 (odd RB), 2: one, 3: three)\n", csi_params->freq_density);
  LOG_I(NR_PHY, "csi_params->scramb_id = %i\n", csi_params->scramb_id);
  LOG_I(NR_PHY, "csi_params->power_control_offset = %i\n", csi_params->power_control_offset);
  LOG_I(NR_PHY, "csi_params->power_control_offset_ss = %i\n", csi_params->power_control_offset_ss);
#endif

  // setting the frequency density from its index
  double rho = get_csi_rho(csi_params->freq_density);
  int csi_length = get_csi_modulation_length(rho,
                                             csi_params->freq_density,
                                             phy_csi_parms->kprime,
                                             csi_params->start_rb,
                                             csi_params->nr_of_rbs);

#ifdef NR_CSIRS_DEBUG
  printf(" start rb %d, nr of rbs %d, csi length %d\n", csi_params->start_rb, csi_params->nr_of_rbs, csi_length);
#endif

  //*8(max allocation per RB)*2(QPSK))
  int16_t mod_csi[frame_parms->symbols_per_slot][(frame_parms->N_RB_DL << 4) >> 1] __attribute__((aligned(16)));
  get_modulated_csi_symbols(frame_parms->symbols_per_slot,
                            slot,
                            frame_parms->N_RB_DL,
                            csi_length,
                            mod_csi,
                            phy_csi_parms->lprime,
                            csi_params->symb_l0,
                            csi_params->symb_l1,
                            csi_params->row,
                            csi_params->scramb_id);

  uint32_t beta = get_csi_beta_amplitude(amp, csi_params->power_control_offset_ss);
  double alpha = 0;
  if (phy_csi_parms->ports == 1)
    alpha = rho;
  else
    alpha = 2 * rho;

#ifdef NR_CSIRS_DEBUG
  printf(" rho %f, alpha %f\n", rho, alpha);
#endif

  // CDM group size from CDM type index
  int gs = get_cdm_group_size(csi_params->cdm_type);

  int dataF_offset = slot * frame_parms->samples_per_slot_wCP;

  csi_rs_resource_mapping(dataF,
                          frame_parms->N_RB_DL << 4,
                          mod_csi,
                          frame_parms->ofdm_symbol_size,
                          dataF_offset,
                          frame_parms->first_carrier_offset,
                          phy_csi_parms,
                          csi_params->start_rb,
                          csi_params->nr_of_rbs,
                          alpha,
                          beta,
                          rho,
                          gs,
                          csi_params->freq_density);
}
