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

/*! \file nr_ue_measurements.c
 * \brief UE measurements routines
 * \author  R. Knopp, G. Casati, K. Saaifan
 * \date 2020
 * \version 0.1
 * \company Eurecom, Fraunhofer IIS
 * \email: knopp@eurecom.fr, guido.casati@iis.fraunhofer.de, khodr.saaifan@iis.fraunhofer.de
 * \note
 * \warning
 */

#include "executables/softmodem-common.h"
#include "executables/nr-softmodem-common.h"
#include "PHY/defs_nr_UE.h"
#include "PHY/INIT/nr_phy_init.h"
#include "PHY/phy_extern_nr_ue.h"
#include "common/utils/LOG/log.h"
#include "PHY/sse_intrin.h"

//#define k1 1000
#define k1 ((long long int) 1000)
#define k2 ((long long int) (1024-k1))

//#define DEBUG_MEAS_RRC
//#define DEBUG_MEAS_UE
//#define DEBUG_RANK_EST

uint32_t get_nr_rx_total_gain_dB (module_id_t Mod_id,uint8_t CC_id)
{

  PHY_VARS_NR_UE *ue = PHY_vars_UE_g[Mod_id][CC_id];

  if (ue)
    return ue->rx_total_gain_dB;

  return 0xFFFFFFFF;
}


float_t get_nr_RSRP(module_id_t Mod_id,uint8_t CC_id,uint8_t gNB_index)
{

  AssertFatal(PHY_vars_UE_g!=NULL,"PHY_vars_UE_g is null\n");
  AssertFatal(PHY_vars_UE_g[Mod_id]!=NULL,"PHY_vars_UE_g[%d] is null\n",Mod_id);
  AssertFatal(PHY_vars_UE_g[Mod_id][CC_id]!=NULL,"PHY_vars_UE_g[%d][%d] is null\n",Mod_id,CC_id);

  PHY_VARS_NR_UE *ue = PHY_vars_UE_g[Mod_id][CC_id];

  if (ue)
    return (10*log10(ue->measurements.rsrp[gNB_index])-
	    get_nr_rx_total_gain_dB(Mod_id,0) -
	    10*log10(20*12));
  return -140.0;
}

void nr_ue_measurements(PHY_VARS_NR_UE *ue,
                        const UE_nr_rxtx_proc_t *proc,
                        NR_UE_DLSCH_t *dlsch,
                        uint32_t pdsch_est_size,
                        int32_t dl_ch_estimates[][pdsch_est_size])
{
  int slot = proc->nr_slot_rx;
  int aarx, aatx, gNB_id = 0;
  NR_DL_FRAME_PARMS *frame_parms = &ue->frame_parms;
  int ch_offset = frame_parms->ofdm_symbol_size*2;
  int N_RB_DL = dlsch->dlsch_config.number_rbs;

  ue->measurements.nb_antennas_rx = frame_parms->nb_antennas_rx;

  allocCast3D(rx_spatial_power,
              int,
              ue->measurements.rx_spatial_power,
              NUMBER_OF_CONNECTED_gNB_MAX,
              cmax(frame_parms->nb_antenna_ports_gNB, 1),
              cmax(frame_parms->nb_antennas_rx, 1),
              false);
  allocCast3D(rx_spatial_power_dB,
              unsigned short,
              ue->measurements.rx_spatial_power_dB,
              NUMBER_OF_CONNECTED_gNB_MAX,
              cmax(frame_parms->nb_antenna_ports_gNB, 1),
              cmax(frame_parms->nb_antennas_rx, 1),
              false);

  // signal measurements
  for (gNB_id = 0; gNB_id < ue->n_connected_gNB; gNB_id++){

    ue->measurements.rx_power_tot[gNB_id] = 0;

    for (aarx = 0; aarx < frame_parms->nb_antennas_rx; aarx++){

      ue->measurements.rx_power[gNB_id][aarx] = 0;

      for (aatx = 0; aatx < frame_parms->nb_antenna_ports_gNB; aatx++){
        const int z=signal_energy_nodc((c16_t*)&dl_ch_estimates[gNB_id][ch_offset], N_RB_DL * NR_NB_SC_PER_RB);
        rx_spatial_power[gNB_id][aatx][aarx] = z;

        if (rx_spatial_power[gNB_id][aatx][aarx] < 0)
          rx_spatial_power[gNB_id][aatx][aarx] = 0;

        rx_spatial_power_dB[gNB_id][aatx][aarx] = (unsigned short)dB_fixed(rx_spatial_power[gNB_id][aatx][aarx]);
        ue->measurements.rx_power[gNB_id][aarx] += rx_spatial_power[gNB_id][aatx][aarx];
      }

      ue->measurements.rx_power_dB[gNB_id][aarx] = (unsigned short) dB_fixed(ue->measurements.rx_power[gNB_id][aarx]);
      ue->measurements.rx_power_tot[gNB_id] += ue->measurements.rx_power[gNB_id][aarx];

    }

    ue->measurements.rx_power_tot_dB[gNB_id] = (unsigned short) dB_fixed(ue->measurements.rx_power_tot[gNB_id]);

  }

  // filter to remove jitter
  if (ue->init_averaging == 0) {

    for (gNB_id = 0; gNB_id < ue->n_connected_gNB; gNB_id++)
      ue->measurements.rx_power_avg[gNB_id] = (int)(((k1*((long long int)(ue->measurements.rx_power_avg[gNB_id]))) + (k2*((long long int)(ue->measurements.rx_power_tot[gNB_id])))) >> 10);

    ue->measurements.n0_power_avg = (int)(((k1*((long long int) (ue->measurements.n0_power_avg))) + (k2*((long long int) (ue->measurements.n0_power_tot))))>>10);

    LOG_D(PHY, "Noise Power Computation: k1 %lld k2 %lld n0 avg %u n0 tot %u\n", k1, k2, ue->measurements.n0_power_avg, ue->measurements.n0_power_tot);

  } else {

    for (gNB_id = 0; gNB_id < ue->n_connected_gNB; gNB_id++)
      ue->measurements.rx_power_avg[gNB_id] = ue->measurements.rx_power_tot[gNB_id];

    ue->measurements.n0_power_avg = ue->measurements.n0_power_tot;
    ue->init_averaging = 0;

  }

  for (gNB_id = 0; gNB_id < ue->n_connected_gNB; gNB_id++) {

    ue->measurements.rx_power_avg_dB[gNB_id] = dB_fixed( ue->measurements.rx_power_avg[gNB_id]);
    ue->measurements.wideband_cqi_tot[gNB_id] = ue->measurements.rx_power_tot_dB[gNB_id] - ue->measurements.n0_power_tot_dB;
    ue->measurements.wideband_cqi_avg[gNB_id] = ue->measurements.rx_power_avg_dB[gNB_id] - dB_fixed(ue->measurements.n0_power_avg);
    ue->measurements.rx_rssi_dBm[gNB_id] = ue->measurements.rx_power_avg_dB[gNB_id] + 30 - SQ15_SQUARED_NORM_FACTOR_DB - ((int)openair0_cfg[0].rx_gain[0] - (int)openair0_cfg[0].rx_gain_offset[0]) - dB_fixed(ue->frame_parms.ofdm_symbol_size);

    LOG_D(PHY, "[gNB %d] Slot %d, RSSI %d dB (%d dBm/RE), WBandCQI %d dB, rxPwrAvg %d, n0PwrAvg %d\n",
      gNB_id,
      slot,
      ue->measurements.rx_power_avg_dB[gNB_id],
      ue->measurements.rx_rssi_dBm[gNB_id],
      ue->measurements.wideband_cqi_avg[gNB_id],
      ue->measurements.rx_power_avg[gNB_id],
      ue->measurements.n0_power_tot);
  }
}

// This function calculates:
// - SS reference signal received digital power in dB/RE
int nr_ue_calculate_ssb_rsrp(const NR_DL_FRAME_PARMS *fp,
                             const UE_nr_rxtx_proc_t *proc,
                             const c16_t rxdataF[][fp->samples_per_slot_wCP],
                             int symbol_offset,
                             int ssb_start_subcarrier)
{
  int k_start = 56;
  int k_end   = 183;
  unsigned int ssb_offset = fp->first_carrier_offset + ssb_start_subcarrier;

  uint8_t l_sss = (symbol_offset + 2) % fp->symbols_per_slot;

  uint32_t rsrp = 0;

  LOG_D(PHY, "In %s: l_sss %d ssb_offset %d\n", __FUNCTION__, l_sss, ssb_offset);
  int nb_re = 0;

  for (int aarx = 0; aarx < fp->nb_antennas_rx; aarx++) {
    int16_t *rxF_sss = (int16_t *)&rxdataF[aarx][l_sss * fp->ofdm_symbol_size];

    for(int k = k_start; k < k_end; k++){
      int re = (ssb_offset + k) % fp->ofdm_symbol_size;

#ifdef DEBUG_MEAS_UE
      LOG_I(PHY, "In %s rxF_sss[%d] %d %d\n", __FUNCTION__, re, rxF_sss[re * 2], rxF_sss[re * 2 + 1]);
#endif

      rsrp += (((int32_t)rxF_sss[re*2]*rxF_sss[re*2]) + ((int32_t)rxF_sss[re*2 + 1]*rxF_sss[re*2 + 1]));
      nb_re++;

    }
  }

  rsrp /= nb_re;

  LOG_D(PHY, "In %s: RSRP/nb_re: %d nb_re :%d\n", __FUNCTION__, rsrp, nb_re);

  int rsrp_db_per_re = 10 * log10(rsrp);

  return rsrp_db_per_re;
}

// This function implements:
// - SS reference signal received power (SS-RSRP) as per clause 5.1.1 of 3GPP TS 38.215 version 16.3.0 Release 16
// - no Layer 3 filtering implemented (no filterCoefficient provided from RRC)
// Todo:
// - Layer 3 filtering according to clause 5.5.3.2 of 3GPP TS 38.331 version 16.2.0 Release 16
// Measurement units:
// - RSRP:    W (dBW)
// - RX Gain  dB
void nr_ue_ssb_rsrp_measurements(PHY_VARS_NR_UE *ue,
                                 int ssb_index,
                                 const UE_nr_rxtx_proc_t *proc,
                                 c16_t rxdataF[][ue->frame_parms.samples_per_slot_wCP])
{
  NR_DL_FRAME_PARMS *fp = &ue->frame_parms;

  int symbol_offset = nr_get_ssb_start_symbol(fp, ssb_index);

  if (fp->half_frame_bit)
    symbol_offset += (fp->slots_per_frame >> 1) * fp->symbols_per_slot;

  int rsrp_db_per_re = nr_ue_calculate_ssb_rsrp(fp, proc, rxdataF, symbol_offset, fp->ssb_start_subcarrier);

  openair0_config_t *cfg0 = &openair0_cfg[0];

  ue->measurements.ssb_rsrp_dBm[ssb_index] = rsrp_db_per_re + 30 - SQ15_SQUARED_NORM_FACTOR_DB
                                             - ((int)cfg0->rx_gain[0] - (int)cfg0->rx_gain_offset[0])
                                             - dB_fixed(fp->ofdm_symbol_size);

  LOG_D(PHY,
        "[UE %d] ssb %d SS-RSRP: %d dBm/RE (%d dB/RE)\n",
        ue->Mod_id,
        ssb_index,
        ue->measurements.ssb_rsrp_dBm[ssb_index],
        rsrp_db_per_re);
}

// This function computes the received noise power
// Measurement units:
// - psd_awgn (AWGN power spectral density):     dBm/Hz
void nr_ue_rrc_measurements(PHY_VARS_NR_UE *ue,
                            const UE_nr_rxtx_proc_t *proc,
                            c16_t rxdataF[][ue->frame_parms.samples_per_slot_wCP])
{
  uint8_t k;
  int slot = proc->nr_slot_rx;
  int aarx;
  int16_t *rxF_sss;
  const uint8_t k_left = 48;
  const uint8_t k_right = 183;
  const uint8_t k_length = 8;
  uint8_t l_sss = (ue->symbol_offset + 2) % ue->frame_parms.symbols_per_slot;
  unsigned int ssb_offset = ue->frame_parms.first_carrier_offset + ue->frame_parms.ssb_start_subcarrier;
  double rx_gain = openair0_cfg[0].rx_gain[0];
  double rx_gain_offset = openair0_cfg[0].rx_gain_offset[0];

  ue->measurements.n0_power_tot = 0;

  LOG_D(PHY, "In %s doing measurements for ssb_offset %d l_sss %d \n", __FUNCTION__, ssb_offset, l_sss);

  for (aarx = 0; aarx<ue->frame_parms.nb_antennas_rx; aarx++) {

    ue->measurements.n0_power[aarx] = 0;
    rxF_sss = (int16_t *)&rxdataF[aarx][l_sss*ue->frame_parms.ofdm_symbol_size];

    //-ve spectrum from SSS
    for(k = k_left; k < k_left + k_length; k++){

      int re = (ssb_offset + k) % ue->frame_parms.ofdm_symbol_size;

      #ifdef DEBUG_MEAS_RRC
      LOG_I(PHY, "In %s -rxF_sss %d %d\n", __FUNCTION__, rxF_sss[re*2], rxF_sss[re*2 + 1]);
      #endif

      ue->measurements.n0_power[aarx] += (((int32_t)rxF_sss[re*2]*rxF_sss[re*2]) + ((int32_t)rxF_sss[re*2 + 1]*rxF_sss[re*2 + 1]));

    }

    //+ve spectrum from SSS
    for(k = k_right; k < k_right + k_length; k++){

      int re = (ssb_offset + k) % ue->frame_parms.ofdm_symbol_size;

      #ifdef DEBUG_MEAS_RRC
      LOG_I(PHY, "In %s +rxF_sss %d %d\n", __FUNCTION__, rxF_sss[re*2], rxF_sss[re*2 + 1]);
      #endif

      ue->measurements.n0_power[aarx] += (((int32_t)rxF_sss[re*2]*rxF_sss[re*2]) + ((int32_t)rxF_sss[re*2 + 1]*rxF_sss[re*2 + 1]));

    }

    ue->measurements.n0_power[aarx] /= 2*k_length;
    ue->measurements.n0_power_dB[aarx] = (unsigned short) dB_fixed(ue->measurements.n0_power[aarx]);
    ue->measurements.n0_power_tot += ue->measurements.n0_power[aarx];

  }

  ue->measurements.n0_power_tot_dB = (unsigned short) dB_fixed(ue->measurements.n0_power_tot);

  #ifdef DEBUG_MEAS_RRC
  const int psd_awgn = -174;
  const int scs = 15000 * (1 << ue->frame_parms.numerology_index);
  const int nf_usrp = ue->measurements.n0_power_tot_dB + 3 + 30 - ((int)rx_gain - (int)rx_gain_offset) - SQ15_SQUARED_NORM_FACTOR_DB - (psd_awgn + dB_fixed(scs) + dB_fixed(ue->frame_parms.ofdm_symbol_size));
  LOG_D(PHY, "In [%s][slot:%d] NF USRP %d dB\n", __FUNCTION__, slot, nf_usrp);
  #endif

  LOG_D(PHY,
        "In [%s][slot:%d] Noise Level %d (digital level %d dB, noise power spectral density %f dBm/RE)\n",
        __FUNCTION__,
        slot,
        ue->measurements.n0_power_tot,
        ue->measurements.n0_power_tot_dB,
        ue->measurements.n0_power_tot_dB + 30 - SQ15_SQUARED_NORM_FACTOR_DB - dB_fixed(ue->frame_parms.ofdm_symbol_size)
            - ((int)rx_gain - (int)rx_gain_offset));
}

// This function implements:
// - PSBCH RSRP calculations according to 38.215 section 5.1.22 Release 16
// - PSBCH DMRS used for calculations
// - TBD: SSS REs for calculation.
// Measurement units:
// - RSRP:    W (dBW)
// returns RXgain to be adjusted based on target rx power (50db) - received digital power in db/RE
int nr_sl_psbch_rsrp_measurements(sl_nr_ue_phy_params_t *sl_phy_params,
                                  NR_DL_FRAME_PARMS *fp,
                                  c16_t rxdataF[][fp->samples_per_slot_wCP],
                                  bool use_SSS)
{
  SL_NR_UE_PSBCH_t *psbch_rx = &sl_phy_params->psbch;
  uint8_t numsym = (fp->Ncp) ? SL_NR_NUM_SYMBOLS_SSB_EXT_CP : SL_NR_NUM_SYMBOLS_SSB_NORMAL_CP;
  uint32_t re_offset = fp->first_carrier_offset + fp->ssb_start_subcarrier;
  uint32_t rsrp = 0, num_re = 0;

  LOG_D(PHY, "PSBCH RSRP MEAS: numsym:%d, re_offset:%d\n", numsym, re_offset);

  for (int aarx = 0; aarx < fp->nb_antennas_rx; aarx++) {
    // Calculate PSBCH RSRP based from DMRS REs
    for (uint8_t symbol = 0; symbol < numsym;) {
      struct complex16 *rxF = &rxdataF[aarx][symbol * fp->ofdm_symbol_size];

      for (int re = 0; re < SL_NR_NUM_PSBCH_RE_IN_ONE_SYMBOL; re++) {
        if (re % 4 == 0) { // DMRS RE
          uint16_t offset = (re_offset + re) % fp->ofdm_symbol_size;

          rsrp += c16amp2(rxF[offset]);
          num_re++;
        }
      }
      symbol = (symbol == 0) ? 5 : symbol + 1;
    }
  }

  if (use_SSS) {
    // TBD...
    // UE can decide between using only PSBCH DMRS or PSBCH DMRS and SSS for PSBCH RSRP computation.
    // If needed this can be implemented. Reference Spec 38.215
  }

  psbch_rx->rsrp_dB_per_RE = 10 * log10(rsrp / num_re);
  psbch_rx->rsrp_dBm_per_RE = psbch_rx->rsrp_dB_per_RE + 30 - SQ15_SQUARED_NORM_FACTOR_DB
                              - ((int)openair0_cfg[0].rx_gain[0] - (int)openair0_cfg[0].rx_gain_offset[0])
                              - dB_fixed(fp->ofdm_symbol_size);

  int adjust_rxgain = TARGET_RX_POWER - psbch_rx->rsrp_dB_per_RE;

  LOG_D(PHY,
        "PSBCH RSRP (DMRS REs): numREs:%d RSRP :%d dB/RE ,RSRP:%d dBm/RE, adjust_rxgain:%d dB\n",
        num_re,
        psbch_rx->rsrp_dB_per_RE,
        psbch_rx->rsrp_dBm_per_RE,
        adjust_rxgain);

  return adjust_rxgain;
}
