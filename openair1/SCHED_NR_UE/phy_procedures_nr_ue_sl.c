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

#define _GNU_SOURCE

#include "PHY/defs_nr_UE.h"
#include <openair1/PHY/TOOLS/phy_scope_interface.h>
#include "common/utils/LOG/log.h"
#include "common/utils/LOG/vcd_signal_dumper.h"
#include "UTIL/OPT/opt.h"
#include "intertask_interface.h"
#include "T.h"
#include "PHY/MODULATION/modulation_UE.h"
#include "PHY/NR_UE_ESTIMATION/nr_estimation.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"

void nr_fill_sl_indication(nr_sidelink_indication_t *sl_ind,
                           sl_nr_rx_indication_t *rx_ind,
                           sl_nr_sci_indication_t *sci_ind,
                           const UE_nr_rxtx_proc_t *proc,
                           PHY_VARS_NR_UE *ue,
                           void *phy_data)
{
  memset((void *)sl_ind, 0, sizeof(nr_sidelink_indication_t));

  sl_ind->gNB_index = proc->gNB_id;
  sl_ind->module_id = ue->Mod_id;
  sl_ind->cc_id = ue->CC_id;
  sl_ind->frame_rx = proc->frame_rx;
  sl_ind->slot_rx = proc->nr_slot_rx;
  sl_ind->frame_tx = proc->frame_tx;
  sl_ind->slot_tx = proc->nr_slot_tx;
  sl_ind->phy_data = phy_data;
  sl_ind->slot_type = SIDELINK_SLOT_TYPE_RX;

  if (rx_ind) {
    sl_ind->rx_ind = rx_ind; //  hang on rx_ind instance
    sl_ind->sci_ind = NULL;
  }
  if (sci_ind) {
    sl_ind->rx_ind = NULL;
    sl_ind->sci_ind = sci_ind;
  }
}

void nr_fill_sl_rx_indication(sl_nr_rx_indication_t *rx_ind,
                              uint8_t pdu_type,
                              PHY_VARS_NR_UE *ue,
                              uint16_t n_pdus,
                              const UE_nr_rxtx_proc_t *proc,
                              void *typeSpecific,
                              uint16_t rx_slss_id)
{
  if (n_pdus > 1) {
    LOG_E(NR_PHY, "In %s: multiple number of SL PDUs not supported yet...\n", __FUNCTION__);
  }

  sl_nr_ue_phy_params_t *sl_phy_params = &ue->SL_UE_PHY_PARAMS;

  switch (pdu_type) {
    case SL_NR_RX_PDU_TYPE_SLSCH:
      break;
    case FAPI_NR_RX_PDU_TYPE_SSB: {
      sl_nr_ssb_pdu_t *ssb_pdu = &rx_ind->rx_indication_body[n_pdus - 1].ssb_pdu;
      if (typeSpecific) {
        uint8_t *psbch_decoded_output = (uint8_t *)typeSpecific;
        memcpy(ssb_pdu->psbch_payload, psbch_decoded_output, 4); // 4 bytes of PSBCH payload bytes
        ssb_pdu->rsrp_dbm = sl_phy_params->psbch.rsrp_dBm_per_RE;
        ssb_pdu->rx_slss_id = rx_slss_id;
        ssb_pdu->decode_status = true;
        LOG_D(NR_PHY,
              "SL-IND: SSB to MAC. rsrp:%d, slssid:%d, payload:%x\n",
              ssb_pdu->rsrp_dbm,
              ssb_pdu->rx_slss_id,
              *((uint32_t *)(ssb_pdu->psbch_payload)));
      } else
        ssb_pdu->decode_status = false;
    } break;
    default:
      break;
  }

  rx_ind->rx_indication_body[n_pdus - 1].pdu_type = pdu_type;
  rx_ind->number_pdus = n_pdus;
}

static int nr_ue_psbch_procedures(PHY_VARS_NR_UE *ue,
                                  NR_DL_FRAME_PARMS *fp,
                                  const UE_nr_rxtx_proc_t *proc,
                                  int estimateSz,
                                  struct complex16 dl_ch_estimates[][estimateSz],
                                  nr_phy_data_t *phy_data,
                                  c16_t rxdataF[][fp->samples_per_slot_wCP])
{
  int ret = 0;
  DevAssert(ue);

  int frame_rx = proc->frame_rx;
  int nr_slot_rx = proc->nr_slot_rx;

  sl_nr_ue_phy_params_t *sl_phy_params = &ue->SL_UE_PHY_PARAMS;
  uint16_t rx_slss_id = sl_phy_params->sl_config.sl_sync_source.rx_slss_id;

  // VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_PSBCH_PROCEDURES, VCD_FUNCTION_IN);

  LOG_D(NR_PHY,
        "[UE  %d] Frame %d Slot %d, Trying PSBCH (SLSS ID %d)\n",
        ue->Mod_id,
        frame_rx,
        nr_slot_rx,
        sl_phy_params->sl_config.sl_sync_source.rx_slss_id);

  uint8_t decoded_pdu[4] = {0};
  ret = nr_rx_psbch(ue,
                    proc,
                    estimateSz,
                    dl_ch_estimates,
                    fp,
                    decoded_pdu,
                    rxdataF,
                    sl_phy_params->sl_config.sl_sync_source.rx_slss_id);

  nr_sidelink_indication_t sl_indication;
  sl_nr_rx_indication_t rx_ind = {0};
  uint16_t number_pdus = 1;

  uint8_t *result = NULL;
  if (ret) {
    sl_phy_params->psbch.rx_errors++;
    LOG_E(NR_PHY, "%d:%d PSBCH RX: NOK \n", proc->frame_rx, proc->nr_slot_rx);
  } else {
    result = decoded_pdu;
    sl_phy_params->psbch.rx_ok++;
    LOG_I(NR_PHY,
          "[UE%d] %d:%d PSBCH RX:OK. RSRP: %d dB/RE\n",
          ue->Mod_id,
          proc->frame_rx,
          proc->nr_slot_rx,
          sl_phy_params->psbch.rsrp_dB_per_RE);
  }

  nr_fill_sl_indication(&sl_indication, &rx_ind, NULL, proc, ue, phy_data);
  nr_fill_sl_rx_indication(&rx_ind, SL_NR_RX_PDU_TYPE_SSB, ue, number_pdus, proc, (void *)result, rx_slss_id);

  if (ue->if_inst && ue->if_inst->sl_indication)
    ue->if_inst->sl_indication(&sl_indication);

  // VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_PSBCH_PROCEDURES, VCD_FUNCTION_OUT);
  return ret;
}

int psbch_pscch_processing(PHY_VARS_NR_UE *ue, const UE_nr_rxtx_proc_t *proc, nr_phy_data_t *phy_data)
{
  int frame_rx = proc->frame_rx;
  int nr_slot_rx = proc->nr_slot_rx;
  sl_nr_ue_phy_params_t *sl_phy_params = &ue->SL_UE_PHY_PARAMS;
  NR_DL_FRAME_PARMS *fp = &sl_phy_params->sl_frame_params;
  int sampleShift = INT_MAX;

  // VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_UE_RX_SL, VCD_FUNCTION_IN);
  start_meas(&sl_phy_params->phy_proc_sl_rx);

  LOG_D(NR_PHY, " ****** Sidelink RX-Chain for Frame.Slot %d.%d ******  \n", frame_rx % 1024, nr_slot_rx);

  const uint32_t rxdataF_sz = fp->samples_per_slot_wCP;
  __attribute__((aligned(32))) c16_t rxdataF[fp->nb_antennas_rx][rxdataF_sz];

  if (phy_data->sl_rx_action == SL_NR_CONFIG_TYPE_RX_PSBCH) {
    const int estimateSz = fp->symbols_per_slot * fp->ofdm_symbol_size;

    // VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_SLOT_FEP_PSBCH, VCD_FUNCTION_IN);
    LOG_D(NR_PHY, " ----- PSBCH RX TTI: frame.slot %d.%d ------  \n", frame_rx % 1024, nr_slot_rx);

    __attribute__((aligned(32))) struct complex16 dl_ch_estimates[fp->nb_antennas_rx][estimateSz];
    __attribute__((aligned(32))) struct complex16 dl_ch_estimates_time[fp->nb_antennas_rx][fp->ofdm_symbol_size];

    // 0 for Normal Cyclic Prefix and 1 for EXT CyclicPrefix
    const int numsym = (fp->Ncp) ? SL_NR_NUM_SYMBOLS_SSB_EXT_CP : SL_NR_NUM_SYMBOLS_SSB_NORMAL_CP;

    for (int sym = 0; sym < numsym;) {
      nr_slot_fep(ue, fp, proc->nr_slot_rx, sym, rxdataF, link_type_sl, 0, ue->common_vars.rxdata);

      start_meas(&sl_phy_params->channel_estimation_stats);
      nr_pbch_channel_estimation(fp,
                                 &ue->SL_UE_PHY_PARAMS,
                                 estimateSz,
                                 dl_ch_estimates,
                                 dl_ch_estimates_time,
                                 proc,
                                 sym,
                                 sym,
                                 0,
                                 0,
                                 fp->ssb_start_subcarrier,
                                 rxdataF,
                                 true,
                                 sl_phy_params->sl_config.sl_sync_source.rx_slss_id);
      stop_meas(&sl_phy_params->channel_estimation_stats);
      if (sym == 12)
        UEscopeCopy(ue,
                    psbchDlChEstimateTime,
                    (void *)dl_ch_estimates_time,
                    sizeof(c16_t),
                    fp->nb_antennas_rx,
                    fp->ofdm_symbol_size,
                    0);

      // PSBCH present in symbols 0, 5-12 for normal cp
      sym = (sym == 0) ? 5 : sym + 1;
    }

    ue->adjust_rxgain = nr_sl_psbch_rsrp_measurements(sl_phy_params, fp, rxdataF, false);

    LOG_D(NR_PHY, " ------  Decode SL-MIB: frame.slot %d.%d ------  \n", frame_rx % 1024, nr_slot_rx);

    const int psbchSuccess = nr_ue_psbch_procedures(ue, fp, proc, estimateSz, dl_ch_estimates, phy_data, rxdataF);

    if (ue->no_timing_correction == 0 && psbchSuccess == 0) {
      LOG_D(NR_PHY, "start adjust sync slot = %d no timing %d\n", nr_slot_rx, ue->no_timing_correction);
      sampleShift =
          nr_adjust_synch_ue(fp, ue, proc->gNB_id, fp->ofdm_symbol_size, dl_ch_estimates_time, frame_rx, nr_slot_rx, 16384);
    }

    LOG_D(NR_PHY, "Doing N0 measurements in %s\n", __FUNCTION__);
    //    nr_ue_rrc_measurements(ue, proc, rxdataF);
    // VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_SLOT_FEP_PSBCH, VCD_FUNCTION_OUT);

    if (frame_rx % 64 == 0) {
      LOG_I(NR_PHY, "============================================\n");

      LOG_I(NR_PHY,
            "[UE%d] %d:%d PSBCH Stats: TX %d, RX ok %d, RX not ok %d\n",
            ue->Mod_id,
            frame_rx,
            nr_slot_rx,
            sl_phy_params->psbch.num_psbch_tx,
            sl_phy_params->psbch.rx_ok,
            sl_phy_params->psbch.rx_errors);

      LOG_I(NR_PHY, "============================================\n");
    }
  }

  UEscopeCopy(ue, commonRxdataF, rxdataF, sizeof(int32_t), fp->nb_antennas_rx, rxdataF_sz, 0);

  return sampleShift;
}

void phy_procedures_nrUE_SL_TX(PHY_VARS_NR_UE *ue, const UE_nr_rxtx_proc_t *proc, nr_phy_data_tx_t *phy_data)
{
  int slot_tx = proc->nr_slot_tx;
  int frame_tx = proc->frame_tx;
  int tx_action = 0;

  sl_nr_ue_phy_params_t *sl_phy_params = &ue->SL_UE_PHY_PARAMS;
  NR_DL_FRAME_PARMS *fp = &sl_phy_params->sl_frame_params;

  // VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_UE_TX_SL,VCD_FUNCTION_IN);

  const int samplesF_per_slot = NR_SYMBOLS_PER_SLOT * fp->ofdm_symbol_size;
  c16_t txdataF_buf[fp->nb_antennas_tx * samplesF_per_slot] __attribute__((aligned(32)));
  memset(txdataF_buf, 0, sizeof(txdataF_buf));
  c16_t *txdataF[fp->nb_antennas_tx]; /* workaround to be compatible with current txdataF usage in all tx procedures. */
  for (int i = 0; i < fp->nb_antennas_tx; ++i)
    txdataF[i] = &txdataF_buf[i * samplesF_per_slot];

  LOG_D(NR_PHY, "****** start Sidelink TX-Chain for AbsSubframe %d.%d ******\n", frame_tx, slot_tx);

  start_meas(&sl_phy_params->phy_proc_sl_tx);

  if (phy_data->sl_tx_action == SL_NR_CONFIG_TYPE_TX_PSBCH) {
    sl_nr_tx_config_psbch_pdu_t *psbch_vars = &phy_data->psbch_vars;
    nr_tx_psbch(ue, frame_tx, slot_tx, psbch_vars, txdataF);
    sl_phy_params->psbch.num_psbch_tx++;

    if (frame_tx % 64 == 0) {
      LOG_I(NR_PHY, "============================================\n");

      LOG_I(NR_PHY,
            "[UE%d] %d:%d PSBCH Stats: TX %d, RX ok %d, RX not ok %d\n",
            ue->Mod_id,
            frame_tx,
            slot_tx,
            sl_phy_params->psbch.num_psbch_tx,
            sl_phy_params->psbch.rx_ok,
            sl_phy_params->psbch.rx_errors);

      LOG_I(NR_PHY, "============================================\n");
    }
    tx_action = 1;
  }

  if (tx_action) {
    LOG_D(NR_PHY, "Sending Uplink data \n");
    nr_ue_pusch_common_procedures(ue, proc->nr_slot_tx, fp, fp->nb_antennas_tx, txdataF, link_type_sl);
  }

  LOG_D(NR_PHY, "****** end Sidelink TX-Chain for AbsSubframe %d.%d ******\n", frame_tx, slot_tx);

  // VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_UE_TX_SL, VCD_FUNCTION_OUT);
  stop_meas(&sl_phy_params->phy_proc_sl_tx);

}
