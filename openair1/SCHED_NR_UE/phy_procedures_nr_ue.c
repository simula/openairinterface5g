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

/*! \file phy_procedures_nr_ue.c
 * \brief Implementation of UE procedures from 36.213 LTE specifications
 * \author R. Knopp, F. Kaltenberger, N. Nikaein, A. Mico Pereperez, G. Casati
 * \date 2018
 * \version 0.1
 * \company Eurecom
 * \email: knopp@eurecom.fr,florian.kaltenberger@eurecom.fr, navid.nikaein@eurecom.fr, guido.casati@iis.fraunhofer.de
 * \note
 * \warning
 */

#define _GNU_SOURCE

#include "nr/nr_common.h"
#include "assertions.h"
#include "defs.h"
#include "PHY/defs_nr_UE.h"
#include "PHY/NR_REFSIG/dmrs_nr.h"
#include "PHY/phy_extern_nr_ue.h"
#include "PHY/MODULATION/modulation_UE.h"
#include "PHY/INIT/nr_phy_init.h"
#include "PHY/NR_REFSIG/ptrs_nr.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_ue.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "PHY/NR_UE_TRANSPORT/srs_modulation_nr.h"
#include "SCHED_NR_UE/phy_sch_processing_time.h"
#include "PHY/NR_UE_ESTIMATION/nr_estimation.h"
#ifdef EMOS
#include "SCHED/phy_procedures_emos.h"
#endif
#include "executables/softmodem-common.h"
#include "executables/nr-uesoftmodem.h"
#include "SCHED_NR_UE/pucch_uci_ue_nr.h"
#include <openair1/PHY/TOOLS/phy_scope_interface.h>
#include "nfapi/open-nFAPI/nfapi/public_inc/nfapi_nr_interface.h"

//#define DEBUG_PHY_PROC
//#define NR_PDCCH_SCHED_DEBUG
//#define NR_PUCCH_SCHED
//#define NR_PUCCH_SCHED_DEBUG
//#define NR_PDSCH_DEBUG

#ifndef PUCCH
#define PUCCH
#endif

#include "common/utils/LOG/log.h"

#ifdef EMOS
fifo_dump_emos_UE emos_dump_UE;
#endif

#include "common/utils/LOG/vcd_signal_dumper.h"
#include "UTIL/OPT/opt.h"
#include "intertask_interface.h"
#include "T.h"
#include "instrumentation.h"

static const unsigned int gain_table[31] = {100,  112,  126,  141,  158,  178,  200,  224,  251, 282,  316,
                                            359,  398,  447,  501,  562,  631,  708,  794,  891, 1000, 1122,
                                            1258, 1412, 1585, 1778, 1995, 2239, 2512, 2818, 3162};

static void nr_ue_prach_procedures(PHY_VARS_NR_UE *ue, const UE_nr_rxtx_proc_t *proc, c16_t **txData);

void nr_fill_dl_indication(nr_downlink_indication_t *dl_ind,
                           fapi_nr_dci_indication_t *dci_ind,
                           fapi_nr_rx_indication_t *rx_ind,
                           const UE_nr_rxtx_proc_t *proc,
                           PHY_VARS_NR_UE *ue,
                           void *phy_data)
{
  memset((void*)dl_ind, 0, sizeof(nr_downlink_indication_t));

  dl_ind->gNB_index = proc->gNB_id;
  dl_ind->module_id = ue->Mod_id;
  dl_ind->cc_id     = ue->CC_id;
  dl_ind->frame     = proc->frame_rx;
  dl_ind->slot      = proc->nr_slot_rx;
  dl_ind->phy_data  = phy_data;

  if (dci_ind) {

    dl_ind->rx_ind = NULL; //no data, only dci for now
    dl_ind->dci_ind = dci_ind;

  } else if (rx_ind) {

    dl_ind->rx_ind = rx_ind; //  hang on rx_ind instance
    dl_ind->dci_ind = NULL;

  }
}

static uint32_t get_ssb_arfcn(NR_DL_FRAME_PARMS *frame_parms)
{
  uint32_t band_size_hz = frame_parms->N_RB_DL * 12 * frame_parms->subcarrier_spacing;
  int ssb_center_sc = frame_parms->ssb_start_subcarrier + 120; // ssb is 20 PRBs -> 240 sub-carriers
  uint64_t ssb_freq = frame_parms->dl_CarrierFreq - (band_size_hz / 2) + frame_parms->subcarrier_spacing * ssb_center_sc;
  return to_nrarfcn(frame_parms->nr_band, ssb_freq, frame_parms->numerology_index, band_size_hz);
}
void nr_fill_rx_indication(fapi_nr_rx_indication_t *rx_ind,
                           uint8_t pdu_type,
                           PHY_VARS_NR_UE *ue,
                           NR_UE_DLSCH_t *dlsch0,
                           NR_UE_DLSCH_t *dlsch1,
                           uint16_t n_pdus,
                           const UE_nr_rxtx_proc_t *proc,
                           void *typeSpecific,
                           uint8_t *b)
{
  if (n_pdus > 1){
    LOG_E(PHY, "In %s: multiple number of DL PDUs not supported yet...\n", __FUNCTION__);
  }
  fapi_nr_rx_indication_body_t *rx = rx_ind->rx_indication_body + n_pdus - 1;
  switch (pdu_type){
    case FAPI_NR_RX_PDU_TYPE_SIB:
    case FAPI_NR_RX_PDU_TYPE_RAR:
    case FAPI_NR_RX_PDU_TYPE_DLSCH:
      if(dlsch0) {
        NR_DL_UE_HARQ_t *dl_harq0 = &ue->dl_harq_processes[0][dlsch0->dlsch_config.harq_process_nbr];
        rx->pdsch_pdu.harq_pid = dlsch0->dlsch_config.harq_process_nbr;
        rx->pdsch_pdu.ack_nack = dl_harq0->decodeResult;
        rx->pdsch_pdu.pdu = b;
        rx->pdsch_pdu.pdu_length = dlsch0->dlsch_config.TBS / 8;
        if (dl_harq0->decodeResult) {
          int t = WS_C_RNTI;
          if (pdu_type == FAPI_NR_RX_PDU_TYPE_RAR)
            t = WS_RA_RNTI;
          if (pdu_type == FAPI_NR_RX_PDU_TYPE_SIB)
            t = WS_SI_RNTI;
          ws_trace_t tmp = {.nr = true,
                            .direction = DIRECTION_DOWNLINK,
                            .pdu_buffer = b,
                            .pdu_buffer_size = rx->pdsch_pdu.pdu_length,
                            .ueid = 0,
                            .rntiType = t,
                            .rnti = dlsch0->rnti,
                            .sysFrame = proc->frame_rx,
                            .subframe = proc->nr_slot_rx,
                            .harq_pid = dlsch0->dlsch_config.harq_process_nbr};
          trace_pdu(&tmp);
        }
      }
      if(dlsch1) {
        AssertFatal(1==0,"Second codeword currently not supported\n");
      }
      break;
    case FAPI_NR_RX_PDU_TYPE_SSB: {
      if (typeSpecific) {
        NR_DL_FRAME_PARMS *frame_parms = &ue->frame_parms;
        fapiPbch_t *pbch = (fapiPbch_t *)typeSpecific;
        memcpy(rx->ssb_pdu.pdu, pbch->decoded_output, sizeof(pbch->decoded_output));
        rx->ssb_pdu.additional_bits = pbch->xtra_byte;
        rx->ssb_pdu.ssb_index = (frame_parms->ssb_index) & 0x7;
        rx->ssb_pdu.ssb_length = frame_parms->Lmax;
        rx->ssb_pdu.cell_id = frame_parms->Nid_cell;
        rx->ssb_pdu.ssb_start_subcarrier = frame_parms->ssb_start_subcarrier;
        rx->ssb_pdu.rsrp_dBm = ue->measurements.ssb_rsrp_dBm[frame_parms->ssb_index];
        rx->ssb_pdu.sinr_dB = ue->measurements.ssb_sinr_dB[frame_parms->ssb_index];
        rx->ssb_pdu.arfcn = get_ssb_arfcn(frame_parms);
        rx->ssb_pdu.radiolink_monitoring = RLM_in_sync; // TODO to be removed from here
        rx->ssb_pdu.decoded_pdu = true;
      } else {
        rx->ssb_pdu.radiolink_monitoring = RLM_out_of_sync; // TODO to be removed from here
        rx->ssb_pdu.decoded_pdu = false;
      }
    } break;
    case FAPI_NR_CSIRS_IND:
      memcpy(&rx->csirs_measurements, typeSpecific, sizeof(fapi_nr_csirs_measurements_t));
      break;
    default:
    break;
  }

  rx->pdu_type = pdu_type;
  rx_ind->number_pdus = n_pdus;

}

int get_tx_amp_prach(int power_dBm, int power_max_dBm, int N_RB_UL){

  int gain_dB = power_dBm - power_max_dBm, amp_x_100 = -1;

  switch (N_RB_UL) {
  case 6:
  amp_x_100 = AMP;      // PRACH is 6 PRBS so no scale
  break;
  case 15:
  amp_x_100 = 158*AMP;  // 158 = 100*sqrt(15/6)
  break;
  case 25:
  amp_x_100 = 204*AMP;  // 204 = 100*sqrt(25/6)
  break;
  case 50:
  amp_x_100 = 286*AMP;  // 286 = 100*sqrt(50/6)
  break;
  case 75:
  amp_x_100 = 354*AMP;  // 354 = 100*sqrt(75/6)
  break;
  case 100:
  amp_x_100 = 408*AMP;  // 408 = 100*sqrt(100/6)
  break;
  default:
  LOG_E(PHY, "Unknown PRB size %d\n", N_RB_UL);
  return (amp_x_100);
  break;
  }
  if (gain_dB < -30) {
    return (amp_x_100/3162);
  } else if (gain_dB > 0)
    return (amp_x_100);
  else
    return (amp_x_100/gain_table[-gain_dB]);  // 245 corresponds to the factor sqrt(25/6)

  return (amp_x_100);
}

// UL time alignment procedures:
// - If the current tx frame and slot match the TA configuration
//   then timing advance is processed and set to be applied in the next UL transmission
// - Application of timing adjustment according to TS 38.213 p4.2
// - handle RAR TA application as per ch 4.2 TS 38.213
void ue_ta_procedures(PHY_VARS_NR_UE *ue, int slot_tx, int frame_tx)
{
  if (frame_tx == ue->ta_frame && slot_tx == ue->ta_slot) {
    uint16_t ofdm_symbol_size = ue->frame_parms.ofdm_symbol_size;

    // convert time factor "16 * 64 * T_c / (2^mu)" in N_TA calculation in TS38.213 section 4.2 to samples by multiplying with
    // samples per second
    //   16 * 64 * T_c            / (2^mu) * samples_per_second
    // = 16 * T_s                 / (2^mu) * samples_per_second
    // = 16 * 1 / (15 kHz * 2048) / (2^mu) * (15 kHz * 2^mu * ofdm_symbol_size)
    // = 16 * 1 /           2048           *                  ofdm_symbol_size
    // = 16 * ofdm_symbol_size / 2048
    uint16_t bw_scaling = 16 * ofdm_symbol_size / 2048;

    ue->timing_advance += (ue->ta_command - 31) * bw_scaling;

    LOG_D(PHY,
          "[UE %d] [%d.%d] Got timing advance command %u from MAC, new value is %d\n",
          ue->Mod_id,
          frame_tx,
          slot_tx,
          ue->ta_command,
          ue->timing_advance);

    ue->ta_frame = -1;
    ue->ta_slot = -1;
  }
}

void phy_procedures_nrUE_TX(PHY_VARS_NR_UE *ue, const UE_nr_rxtx_proc_t *proc, nr_phy_data_tx_t *phy_data, c16_t **txp)
{
  const int slot_tx = proc->nr_slot_tx;
  const int frame_tx = proc->frame_tx;

  AssertFatal(ue->CC_id == 0, "Transmission on secondary CCs is not supported yet\n");

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_UE_TX,VCD_FUNCTION_IN);

  const int samplesF_per_slot = NR_SYMBOLS_PER_SLOT * ue->frame_parms.ofdm_symbol_size;
  c16_t txdataF_buf[ue->frame_parms.nb_antennas_tx * samplesF_per_slot] __attribute__((aligned(32)));
  memset(txdataF_buf, 0, sizeof(txdataF_buf));
  c16_t *txdataF[ue->frame_parms.nb_antennas_tx]; /* workaround to be compatible with current txdataF usage in all tx procedures. */
  for(int i=0; i< ue->frame_parms.nb_antennas_tx; ++i)
    txdataF[i] = &txdataF_buf[i * samplesF_per_slot];

  LOG_D(PHY,"****** start TX-Chain for AbsSubframe %d.%d ******\n", frame_tx, slot_tx);
  bool was_symbol_used[NR_NUMBER_OF_SYMBOLS_PER_SLOT] = {0};

  start_meas_nr_ue_phy(ue, PHY_PROC_TX);

  nr_ue_ulsch_procedures(ue, frame_tx, slot_tx, phy_data, (c16_t **)&txdataF, was_symbol_used);

  ue_srs_procedures_nr(ue, proc, (c16_t **)&txdataF, phy_data, was_symbol_used);

  pucch_procedures_ue_nr(ue, proc, phy_data, (c16_t **)&txdataF, was_symbol_used);

  LOG_D(PHY, "Sending Uplink data \n");

  // Don't do OFDM Mod if txdata contains prach
  const NR_UE_PRACH *prach_var = ue->prach_vars[proc->gNB_id];
  if (!prach_var->active) {
    start_meas_nr_ue_phy(ue, OFDM_MOD_STATS);
    nr_ue_pusch_common_procedures(ue,
                                  proc->nr_slot_tx,
                                  &ue->frame_parms,
                                  ue->frame_parms.nb_antennas_tx,
                                  (c16_t **)txdataF,
                                  txp,
                                  link_type_ul,
                                  was_symbol_used);
    stop_meas_nr_ue_phy(ue, OFDM_MOD_STATS);
  }

  nr_ue_prach_procedures(ue, proc, txp);

  LOG_D(PHY, "****** end TX-Chain for AbsSubframe %d.%d ******\n", proc->frame_tx, proc->nr_slot_tx);

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_UE_TX, VCD_FUNCTION_OUT);
  stop_meas_nr_ue_phy(ue, PHY_PROC_TX);
}

void nr_ue_measurement_procedures(uint16_t l,
                                  PHY_VARS_NR_UE *ue,
                                  const UE_nr_rxtx_proc_t *proc,
                                  NR_UE_DLSCH_t *dlsch,
                                  uint32_t pdsch_est_size,
                                  int32_t dl_ch_estimates[][pdsch_est_size])
{
  NR_DL_FRAME_PARMS *frame_parms=&ue->frame_parms;
  int nr_slot_rx = proc->nr_slot_rx;
  int gNB_id = proc->gNB_id;
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_MEASUREMENT_PROCEDURES, VCD_FUNCTION_IN);

  if (l==2) {

    LOG_D(PHY,"Doing UE measurement procedures in symbol l %u Ncp %d nr_slot_rx %d, rxdata %p\n",
      l,
      ue->frame_parms.Ncp,
      nr_slot_rx,
      ue->common_vars.rxdata);

    nr_ue_measurements(ue, proc, dlsch, pdsch_est_size, dl_ch_estimates);

#if T_TRACER
    if(nr_slot_rx == 0)
      T(T_UE_PHY_MEAS,
        T_INT(gNB_id),
        T_INT(ue->Mod_id),
        T_INT(proc->frame_rx % 1024),
        T_INT(nr_slot_rx),
        T_INT((int)(10 * log10(ue->measurements.rsrp[0]) - ue->rx_total_gain_dB)),
        T_INT((int)ue->measurements.rx_rssi_dBm[0]),
        T_INT((int)(ue->measurements.rx_power_avg_dB[0] - ue->measurements.n0_power_avg_dB)),
        T_INT((int)ue->measurements.rx_power_avg_dB[0]),
        T_INT((int)ue->measurements.n0_power_avg_dB),
        T_INT((int)ue->measurements.wideband_cqi_avg[0]));
#endif
  }

  // accumulate and filter timing offset estimation every subframe (instead of every frame)
  if (( nr_slot_rx == 2) && (l==(2-frame_parms->Ncp))) {

    // AGC

    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_GAIN_CONTROL, VCD_FUNCTION_IN);


    //printf("start adjust gain power avg db %d\n", ue->measurements.rx_power_avg_dB[gNB_id]);
    phy_adjust_gain_nr (ue,ue->measurements.rx_power_avg_dB[gNB_id],gNB_id);
    
    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_GAIN_CONTROL, VCD_FUNCTION_OUT);

}

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_MEASUREMENT_PROCEDURES, VCD_FUNCTION_OUT);
}

static int nr_ue_pbch_procedures(PHY_VARS_NR_UE *ue,
                                 const UE_nr_rxtx_proc_t *proc,
                                 int estimateSz,
                                 struct complex16 dl_ch_estimates[][estimateSz],
                                 c16_t rxdataF[][ue->frame_parms.samples_per_slot_wCP])
{
  TracyCZone(ctx, true);
  int ret = 0;
  DevAssert(ue);

  int frame_rx = proc->frame_rx;
  int nr_slot_rx = proc->nr_slot_rx;
  int gNB_id = proc->gNB_id;

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_PBCH_PROCEDURES, VCD_FUNCTION_IN);

  LOG_D(PHY,"[UE  %d] Frame %d Slot %d, Trying PBCH (NidCell %d, gNB_id %d)\n",ue->Mod_id,frame_rx,nr_slot_rx,ue->frame_parms.Nid_cell,gNB_id);
  fapiPbch_t result;
  int hf_frame_bit, ssb_index, symb_offset;
  ret = nr_rx_pbch(ue,
                   proc,
                   ue->is_synchronized,
                   estimateSz,
                   dl_ch_estimates,
                   &ue->frame_parms,
                   (ue->frame_parms.ssb_index) & 7,
                   ue->frame_parms.ssb_start_subcarrier,
                   ue->frame_parms.Nid_cell,
                   &result,
                   &hf_frame_bit,
                   &ssb_index,
                   &symb_offset,
                   ue->frame_parms.samples_per_frame_wCP,
                   rxdataF);

  if (ret==0) {
    T(T_NRUE_PHY_MIB, T_INT(frame_rx), T_INT(nr_slot_rx),
      T_INT(ssb_index), T_BUFFER(result.decoded_output, 3));

#ifdef DEBUG_PHY_PROC
    LOG_D(PHY,
          "[UE %d] frame %d, nr_slot_rx %d, Received PBCH (MIB): ssb idx: %d, N_RB_DL %d\n",
          ue->Mod_id,
          frame_rx,
          nr_slot_rx,
          ssb_index,
          ue->frame_parms.N_RB_DL);
#endif

  } else {
    LOG_E(PHY, "[UE %d] frame %d, nr_slot_rx %d, Error decoding PBCH!\n", ue->Mod_id, frame_rx, nr_slot_rx);
  }
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_PBCH_PROCEDURES, VCD_FUNCTION_OUT);
  TracyCZoneEnd(ctx);
  return ret;
}

int nr_ue_pdcch_procedures(PHY_VARS_NR_UE *ue,
                           const UE_nr_rxtx_proc_t *proc,
                           int32_t pdcch_est_size,
                           c16_t pdcch_dl_ch_estimates[][pdcch_est_size],
                           nr_phy_data_t *phy_data,
                           int n_ss,
                           c16_t rxdataF[][ue->frame_parms.samples_per_slot_wCP])
{
  int frame_rx = proc->frame_rx;
  int nr_slot_rx = proc->nr_slot_rx;
  NR_UE_PDCCH_CONFIG *phy_pdcch_config = &phy_data->phy_pdcch_config;

  fapi_nr_dl_config_dci_dl_pdu_rel15_t *rel15 = &phy_pdcch_config->pdcch_config[n_ss];

  start_meas_nr_ue_phy(ue, DLSCH_RX_PDCCH_STATS);

  /// PDCCH/DCI e-sequence (input to rate matching).
  int32_t pdcch_e_rx_size = NR_MAX_PDCCH_SIZE;
  c16_t pdcch_e_rx[pdcch_e_rx_size];

  nr_rx_pdcch(ue, proc, pdcch_est_size, pdcch_dl_ch_estimates, pdcch_e_rx, rel15, rxdataF);

  fapi_nr_dci_indication_t dci_ind;
  nr_dci_decoding_procedure(ue, proc, pdcch_e_rx, &dci_ind, rel15);

  for (int i = 0; i < dci_ind.number_of_dcis; i++) {
    LOG_D(PHY,
          "[UE  %d] AbsSubFrame %d.%d: DCI %i of %d total DCIs found --> rnti %x : format %d\n",
          ue->Mod_id,
          frame_rx % 1024,
          nr_slot_rx,
          i + 1,
          dci_ind.number_of_dcis,
          dci_ind.dci_list[i].rnti,
          dci_ind.dci_list[i].dci_format);
  }

  nr_downlink_indication_t dl_indication;
  // fill dl_indication message
  nr_fill_dl_indication(&dl_indication, &dci_ind, NULL, proc, ue, phy_data);
  //  send to mac
  ue->if_inst->dl_indication(&dl_indication);
  stop_meas_nr_ue_phy(ue, DLSCH_RX_PDCCH_STATS);
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_PDCCH_PROCEDURES, VCD_FUNCTION_OUT);
  return (dci_ind.number_of_dcis);
}

static int nr_ue_pdsch_procedures(PHY_VARS_NR_UE *ue,
                                  const UE_nr_rxtx_proc_t *proc,
                                  NR_UE_DLSCH_t dlsch[2],
                                  int16_t *llr[2],
                                  c16_t rxdataF[][ue->frame_parms.samples_per_slot_wCP],
                                  int G)
{
  int frame_rx = proc->frame_rx;
  int nr_slot_rx = proc->nr_slot_rx;

  // We handle only one CW now
  if (!(NR_MAX_NB_LAYERS>4)) {
    NR_UE_DLSCH_t *dlsch0 = &dlsch[0];
    fapi_nr_dl_config_dlsch_pdu_rel15_t *dlschCfg = &dlsch0->dlsch_config;
    int harq_pid = dlschCfg->harq_process_nbr;
    // dlsch0_harq contains the previous transmissions data for this harq pid
    NR_DL_UE_HARQ_t *dlsch0_harq = &ue->dl_harq_processes[0][harq_pid];

    LOG_D(PHY,
          "[UE %d] frame_rx %d, nr_slot_rx %d, harq_pid %d (%d), BWP start %d, rb_start %d, nb_rb %d, symbol_start %d, nb_symbols %d, DMRS mask "
          "%x, Nl %d\n",
          ue->Mod_id,
          frame_rx,
          nr_slot_rx,
          harq_pid,
          dlsch0_harq->status,
          dlschCfg->BWPStart,
          dlschCfg->start_rb,
          dlschCfg->number_rbs,
          dlschCfg->start_symbol,
          dlschCfg->number_symbols,
          dlschCfg->dlDmrsSymbPos,
          dlsch0->Nl);

    const uint32_t pdsch_est_size = ((ue->frame_parms.symbols_per_slot * ue->frame_parms.ofdm_symbol_size + 15) / 16) * 16;
    fourDimArray_t *toFree = NULL;
    allocCast2D(pdsch_dl_ch_estimates, int32_t, toFree, ue->frame_parms.nb_antennas_rx * dlsch0->Nl, pdsch_est_size, false);

    c16_t ptrs_phase_per_slot[ue->frame_parms.nb_antennas_rx][NR_SYMBOLS_PER_SLOT];
    memset(ptrs_phase_per_slot, 0, sizeof(ptrs_phase_per_slot));

    int32_t ptrs_re_per_slot[ue->frame_parms.nb_antennas_rx][NR_SYMBOLS_PER_SLOT];
    memset(ptrs_re_per_slot, 0, sizeof(ptrs_re_per_slot));

    const uint32_t rx_size_symbol = (dlsch[0].dlsch_config.number_rbs * NR_NB_SC_PER_RB + 15) & ~15;
    fourDimArray_t *toFree2 = NULL;
    allocCast3D(rxdataF_comp,
                int32_t,
                toFree2,
                dlsch[0].Nl,
                ue->frame_parms.nb_antennas_rx,
                rx_size_symbol * NR_SYMBOLS_PER_SLOT,
                false);

    uint32_t nvar = 0;

    start_meas_nr_ue_phy(ue, DLSCH_CHANNEL_ESTIMATION_STATS);
    for (int m = dlschCfg->start_symbol; m < (dlschCfg->start_symbol + dlschCfg->number_symbols); m++) {
      if (dlschCfg->dlDmrsSymbPos & (1 << m)) {
        for (int nl = 0; nl < dlsch0->Nl; nl++) { //for MIMO Config: it shall loop over no_layers
          LOG_D(PHY,"PDSCH Channel estimation layer %d, slot %d, symbol %d\n", nl, nr_slot_rx, m);
          uint32_t nvar_tmp = 0;
          nr_pdsch_channel_estimation(ue,
                                      proc,
                                      nl,
                                      get_dmrs_port(nl, dlschCfg->dmrs_ports),
                                      m,
                                      dlschCfg->nscid,
                                      dlschCfg->dlDmrsScramblingId,
                                      dlschCfg->BWPStart,
                                      dlschCfg->dmrsConfigType,
                                      dlschCfg->n_dmrs_cdm_groups,
                                      dlschCfg->rb_offset,
                                      ue->frame_parms.first_carrier_offset + (dlschCfg->BWPStart + dlschCfg->start_rb) * 12,
                                      dlschCfg->number_rbs,
                                      pdsch_est_size,
                                      pdsch_dl_ch_estimates,
                                      ue->frame_parms.samples_per_slot_wCP,
                                      rxdataF,
                                      &nvar_tmp);
          nvar += nvar_tmp;
#if 0
          ///LOG_M: the channel estimation
          char filename[100];
          for (uint8_t aarx=0; aarx<ue->frame_parms.nb_antennas_rx; aarx++) {
            sprintf(filename,"PDSCH_CHANNEL_frame%d_slot%d_sym%d_port%d_rx%d.m", frame_rx, nr_slot_rx, m, nl, aarx);
            int **dl_ch_estimates = ue->pdsch_vars[gNB_id]->dl_ch_estimates;
            LOG_M(filename,"channel_F",&dl_ch_estimates[nl*ue->frame_parms.nb_antennas_rx+aarx][ue->frame_parms.ofdm_symbol_size*m],ue->frame_parms.ofdm_symbol_size, 1, 1);
          }
#endif
        }
      }
    }
    stop_meas_nr_ue_phy(ue, DLSCH_CHANNEL_ESTIMATION_STATS);

    nvar /= (dlschCfg->number_symbols * dlsch0->Nl * ue->frame_parms.nb_antennas_rx);

    nr_ue_measurement_procedures(2, ue, proc, &dlsch[0], pdsch_est_size, pdsch_dl_ch_estimates);

    if (ue->chest_time == 1) { // averaging time domain channel estimates
      nr_chest_time_domain_avg(&ue->frame_parms,
                               (int32_t **)pdsch_dl_ch_estimates,
                               dlschCfg->number_symbols,
                               dlschCfg->start_symbol,
                               dlschCfg->dlDmrsSymbPos,
                               dlschCfg->number_rbs);
    }

    uint16_t first_symbol_with_data = dlschCfg->start_symbol;
    uint32_t dmrs_data_re;

    if (dlschCfg->dmrsConfigType == NFAPI_NR_DMRS_TYPE1)
      dmrs_data_re = 12 - 6 * dlschCfg->n_dmrs_cdm_groups;
    else
      dmrs_data_re = 12 - 4 * dlschCfg->n_dmrs_cdm_groups;

    while ((dmrs_data_re == 0) && (dlschCfg->dlDmrsSymbPos & (1 << first_symbol_with_data))) {
      first_symbol_with_data++;
    }

    uint32_t dl_valid_re[NR_SYMBOLS_PER_SLOT] = {0};
    uint32_t llr_offset[NR_SYMBOLS_PER_SLOT] = {0};

    int32_t log2_maxh = 0;

    const uint32_t rx_llr_layer_size = (G + dlsch[0].Nl - 1) / dlsch[0].Nl;

    if (dlsch[0].Nl == 0 || rx_llr_layer_size == 0 || rx_llr_layer_size > 10 * 1000 * 1000) {
      LOG_E(PHY, "rx_llr_layer_size %d, G %d, Nl, %d, discarding this pdsch\n", rx_llr_layer_size, G, dlsch[0].Nl);
      return -1;
    }
    __attribute__((aligned(32))) int16_t layer_llr[dlsch[0].Nl][rx_llr_layer_size];

    start_meas_nr_ue_phy(ue, RX_PDSCH_STATS);
    pdsch_scope_req_t scope_req = { .copy_chanest_to_scope = false,
                                    .copy_rxdataF_to_scope = false,
                                    .scope_rxdataF_offset = 0 };
    if (UEScopeHasTryLock(ue)) {
      metadata mt = {.frame = proc->frame_rx, .slot = proc->nr_slot_rx};
      scope_req.copy_chanest_to_scope = UETryLockScopeData(ue,
                                                           pdschChanEstimates,
                                                           sizeof(c16_t),
                                                           1,
                                                           dlschCfg->number_rbs * NR_NB_SC_PER_RB * dlschCfg->number_symbols,
                                                           &mt);
      scope_req.copy_rxdataF_to_scope = UETryLockScopeData(ue,
                                                           pdschRxdataF,
                                                           sizeof(c16_t),
                                                           1,
                                                           dlschCfg->number_rbs * NR_NB_SC_PER_RB * dlschCfg->number_symbols,
                                                           &mt);
    }
    for (int m = dlschCfg->start_symbol; m < (dlschCfg->number_symbols + dlschCfg->start_symbol); m++) {
      bool first_symbol_flag = false;
      if (m == first_symbol_with_data)
        first_symbol_flag = true;

      // process DLSCH received symbols in the slot
      // symbol by symbol processing (if data/DMRS are multiplexed is checked inside the function)
      if (nr_rx_pdsch(ue,
                      proc,
                      dlsch,
                      m,
                      first_symbol_flag,
                      harq_pid,
                      pdsch_est_size,
                      pdsch_dl_ch_estimates,
                      rx_llr_layer_size,
                      layer_llr,
                      llr,
                      dl_valid_re,
                      rxdataF,
                      llr_offset,
                      &log2_maxh,
                      rx_size_symbol,
                      ue->frame_parms.nb_antennas_rx,
                      rxdataF_comp,
                      ptrs_phase_per_slot,
                      ptrs_re_per_slot,
                      G,
                      nvar,
                      &scope_req)
          < 0) {
        if (scope_req.copy_chanest_to_scope) {
          UEunlockScopeData(ue, pdschChanEstimates);
        }
        if (scope_req.copy_rxdataF_to_scope) {
          UEunlockScopeData(ue, pdschRxdataF);
        }
        return -1;
      }
    } // CRNTI active
    stop_meas_nr_ue_phy(ue, RX_PDSCH_STATS);
    if (scope_req.copy_chanest_to_scope) {
      UEunlockScopeData(ue, pdschChanEstimates);
    }
    if (scope_req.copy_rxdataF_to_scope) {
      UEunlockScopeData(ue, pdschRxdataF);
    }
    free(toFree);
    free(toFree2);
  }
  return 0;
}

static uint32_t compute_csi_rm_unav_res(fapi_nr_dl_config_dlsch_pdu_rel15_t *dlsch_config)
{
  uint32_t unav_res = 0;
  for (int i = 0; i < dlsch_config->numCsiRsForRateMatching; i++) {
    fapi_nr_dl_config_csirs_pdu_rel15_t *csi_pdu = &dlsch_config->csiRsForRateMatching[i];
    // check overlapping symbols
    int num_overlap_symb = 0;
    // num of consecutive csi symbols from l0 included
    int num_l0 [18] = {1, 1, 1, 1, 2, 1, 2, 2, 1, 2, 2, 2, 2, 2, 4, 2, 2, 4};
    int num_symb = num_l0[csi_pdu->row - 1];
    for (int s = 0; s < num_symb; s++) {
      int l0_symb = csi_pdu->symb_l0 + s;
      if (l0_symb >= dlsch_config->start_symbol && l0_symb < dlsch_config->start_symbol + dlsch_config->number_symbols)
        num_overlap_symb++;
    }
    // check also l1 if relevant
    if (csi_pdu->row == 13 || csi_pdu->row == 14 || csi_pdu->row == 16 || csi_pdu->row == 17) {
      num_symb += 2;
      for (int s = 0; s < 2; s++) { // two consecutive symbols including l1
        int l1_symb = csi_pdu->symb_l1 + s;
        if (l1_symb >= dlsch_config->start_symbol && l1_symb < dlsch_config->start_symbol + dlsch_config->number_symbols)
          num_overlap_symb++;
      }
    }
    if (num_overlap_symb == 0)
      continue;
    // check number overlapping prbs
    // assuming CSI is spanning the whole BW
    AssertFatal(dlsch_config->BWPSize <= csi_pdu->nr_of_rbs, "Assuming CSI-RS is spanning the whold BWP this shouldn't happen\n");
    int dlsch_start = dlsch_config->start_rb + dlsch_config->BWPStart;
    int num_overlapping_prbs = dlsch_config->number_rbs;
    if (num_overlapping_prbs < 1)
      continue; // no overlapping prbs
    if (csi_pdu->freq_density < 2) { // 0.5 density
      num_overlapping_prbs /= 2;
      // odd number of prbs and the start PRB is even/odd when CSI is in even/odd PRBs
      if ((num_overlapping_prbs % 2) && ((dlsch_start % 2) == csi_pdu->freq_density))
        num_overlapping_prbs += 1;
    }
    // density is number or res per port per rb (over all symbols)
    int ports [18] = {1, 1, 2, 4, 4, 8, 8, 8, 12, 12, 16, 16, 24, 24, 24, 32, 32, 32};
    int num_csi_res_per_prb = csi_pdu->freq_density == 3 ? 3 : 1;
    num_csi_res_per_prb *= ports[csi_pdu->row - 1];
    unav_res += num_overlapping_prbs * num_csi_res_per_prb * num_overlap_symb / num_symb;
  }
  return unav_res;
}

/*! \brief Process the whole DLSCH slot
 */
static void nr_ue_dlsch_procedures(PHY_VARS_NR_UE *ue,
                                   const UE_nr_rxtx_proc_t *proc,
                                   NR_UE_DLSCH_t dlsch[2],
                                   int16_t *llr[2]) {
  if (dlsch[0].active == false) {
    LOG_E(PHY, "DLSCH should be active when calling this function\n");
    return;
  }

  int harq_pid = dlsch[0].dlsch_config.harq_process_nbr;
  int frame_rx = proc->frame_rx;
  int nr_slot_rx = proc->nr_slot_rx;
  NR_DL_UE_HARQ_t *dl_harq0 = &ue->dl_harq_processes[0][harq_pid];
  NR_DL_UE_HARQ_t *dl_harq1 = &ue->dl_harq_processes[1][harq_pid];
  uint16_t dmrs_len = get_num_dmrs(dlsch[0].dlsch_config.dlDmrsSymbPos);
  nr_downlink_indication_t dl_indication;
  fapi_nr_rx_indication_t rx_ind = {0};
  uint16_t number_pdus = 1;

  uint8_t is_cw0_active = dl_harq0->status;
  uint8_t is_cw1_active = dl_harq1->status;
  int nb_dlsch = 0;
  nb_dlsch += (is_cw0_active == ACTIVE) ? 1 : 0;
  nb_dlsch += (is_cw1_active == ACTIVE) ? 1 : 0;
  uint16_t nb_symb_sch = dlsch[0].dlsch_config.number_symbols;
  uint8_t dmrs_type = dlsch[0].dlsch_config.dmrsConfigType;

  uint8_t nb_re_dmrs;
  if (dmrs_type == NFAPI_NR_DMRS_TYPE1) {
    nb_re_dmrs = 6 * dlsch[0].dlsch_config.n_dmrs_cdm_groups;
  } else {
    nb_re_dmrs = 4 * dlsch[0].dlsch_config.n_dmrs_cdm_groups;
  }

  LOG_D(PHY, "AbsSubframe %d.%d Start LDPC Decoder for CW0 [harq_pid %d] ? %d \n", frame_rx % 1024, nr_slot_rx, harq_pid, is_cw0_active);
  LOG_D(PHY, "AbsSubframe %d.%d Start LDPC Decoder for CW1 [harq_pid %d] ? %d \n", frame_rx % 1024, nr_slot_rx, harq_pid, is_cw1_active);

  // exit dlsch procedures as there are no active dlsch
  if (is_cw0_active != ACTIVE && is_cw1_active != ACTIVE) {
    // don't wait anymore
    LOG_E(NR_PHY, "Internal error  nr_ue_dlsch_procedure() called but no active cw on slot %d, harq %d\n", nr_slot_rx, harq_pid);
    if (dlsch[0].dlsch_config.k1_feedback) {
      const int ack_nack_slot_and_frame =
          (proc->nr_slot_rx + dlsch[0].dlsch_config.k1_feedback) + proc->frame_rx * ue->frame_parms.slots_per_frame;
      dynamic_barrier_join(&ue->process_slot_tx_barriers[ack_nack_slot_and_frame % NUM_PROCESS_SLOT_TX_BARRIERS]);
    }
    return;
  }

  int G[2];
  uint8_t DLSCH_ids[nb_dlsch];
  int pdsch_id = 0;
  uint8_t *p_b[2] = {NULL};
  for (uint8_t DLSCH_id = 0; DLSCH_id < 2; DLSCH_id++) {
    NR_DL_UE_HARQ_t *dl_harq = &ue->dl_harq_processes[DLSCH_id][harq_pid];
    if (dl_harq->status != ACTIVE) continue;

    DLSCH_ids[pdsch_id++] = DLSCH_id;
    fapi_nr_dl_config_dlsch_pdu_rel15_t *dlsch_config = &dlsch[DLSCH_id].dlsch_config;
    uint32_t unav_res = 0;
    if (dlsch_config->pduBitmap & 0x1) {
      uint16_t ptrsSymbPos = 0;
      set_ptrs_symb_idx(&ptrsSymbPos, dlsch_config->number_symbols, dlsch_config->start_symbol, 1 << dlsch_config->PTRSTimeDensity,
                        dlsch_config->dlDmrsSymbPos);
      int n_ptrs = (dlsch_config->number_rbs + dlsch_config->PTRSFreqDensity - 1) / dlsch_config->PTRSFreqDensity;
      int ptrsSymbPerSlot = get_ptrs_symbols_in_slot(ptrsSymbPos, dlsch_config->start_symbol, dlsch_config->number_symbols);
      unav_res = n_ptrs * ptrsSymbPerSlot;
    }
    unav_res += compute_csi_rm_unav_res(dlsch_config);
    G[DLSCH_id] = nr_get_G(dlsch_config->number_rbs, nb_symb_sch, nb_re_dmrs, dmrs_len, unav_res, dlsch_config->qamModOrder, dlsch[DLSCH_id].Nl);

    start_meas_nr_ue_phy(ue, DLSCH_UNSCRAMBLING_STATS);
    nr_dlsch_unscrambling(llr[DLSCH_id], G[DLSCH_id], 0, dlsch[DLSCH_id].dlsch_config.dlDataScramblingId, dlsch[DLSCH_id].rnti);
    stop_meas_nr_ue_phy(ue, DLSCH_UNSCRAMBLING_STATS);

    p_b[DLSCH_id] = dl_harq->b;
  }

  start_meas_nr_ue_phy(ue, DLSCH_DECODING_STATS);
  nr_dlsch_decoding(ue, proc, dlsch, llr, p_b, G, nb_dlsch, DLSCH_ids);
  stop_meas_nr_ue_phy(ue, DLSCH_DECODING_STATS);

  int ind_type = -1;
  switch (dlsch[0].rnti_type) {
    case TYPE_RA_RNTI_:
      ind_type = FAPI_NR_RX_PDU_TYPE_RAR;
      break;

    case TYPE_SI_RNTI_:
      ind_type = FAPI_NR_RX_PDU_TYPE_SIB;
      break;

    case TYPE_C_RNTI_:
      ind_type = FAPI_NR_RX_PDU_TYPE_DLSCH;
      break;

    default:
      AssertFatal(true, "Invalid DLSCH type %d\n", dlsch[0].rnti_type);
      break;
  }

  nr_fill_dl_indication(&dl_indication, NULL, &rx_ind, proc, ue, NULL);
  nr_fill_rx_indication(&rx_ind, ind_type, ue, &dlsch[0], NULL, number_pdus, proc, NULL, p_b[0]);

  LOG_D(PHY, "DL PDU length in bits: %d, in bytes: %d \n", dlsch[0].dlsch_config.TBS, dlsch[0].dlsch_config.TBS / 8);
  if (cpumeas(CPUMEAS_GETSTATE)) {
    LOG_D(PHY,
          " --> Unscrambling %5.3f\n",
          ue->phy_cpu_stats.cpu_time_stats[DLSCH_UNSCRAMBLING_STATS].p_time / (cpuf * 1000.0));
    LOG_D(PHY,
          "AbsSubframe %d.%d --> LDPC Decoding %5.3f\n",
          frame_rx % 1024,
          nr_slot_rx,
          ue->phy_cpu_stats.cpu_time_stats[DLSCH_DECODING_STATS].p_time / (cpuf * 1000.0));
  }

  // send to mac
  if (ue->if_inst && ue->if_inst->dl_indication) {
    ue->if_inst->dl_indication(&dl_indication);
  }

  // DLSCH decoding finished! don't wait anymore in Tx process, we know if we should answer ACK/NACK PUCCH
  if ((dlsch[0].rnti_type == TYPE_C_RNTI_ || dlsch[0].rnti_type == TYPE_RA_RNTI_) && dlsch[0].dlsch_config.k1_feedback) {
    const int ack_nack_slot_and_frame =
        (proc->nr_slot_rx + dlsch[0].dlsch_config.k1_feedback) + proc->frame_rx * ue->frame_parms.slots_per_frame;
    dynamic_barrier_join(&ue->process_slot_tx_barriers[ack_nack_slot_and_frame % NUM_PROCESS_SLOT_TX_BARRIERS]);
  }

  int a_segments = MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER * NR_MAX_NB_LAYERS;  // number of segments to be allocated
  int num_rb = dlsch[0].dlsch_config.number_rbs;
  if (num_rb != 273) {
    a_segments = a_segments * num_rb;
    a_segments = (a_segments / 273) + 1;
  }
  uint32_t dlsch_bytes = a_segments * 1056;  // allocated bytes per segment

  if (ue->phy_sim_dlsch_b && is_cw0_active == ACTIVE)
    memcpy(ue->phy_sim_dlsch_b, p_b[0], dlsch_bytes);
  else if (ue->phy_sim_dlsch_b && is_cw1_active == ACTIVE)
    memcpy(ue->phy_sim_dlsch_b, p_b[1], dlsch_bytes);

}

static bool is_ssb_index_transmitted(const PHY_VARS_NR_UE *ue, const int index)
{
  if (ue->received_config_request) {
    const fapi_nr_config_request_t *cfg = &ue->nrUE_config;
    const uint32_t curr_mask = cfg->ssb_table.ssb_mask_list[index / 32].ssb_mask;
    return ((curr_mask >> (31 - (index % 32))) & 0x01);
  } else
    return ue->frame_parms.ssb_index == index;
}

int pbch_pdcch_processing(PHY_VARS_NR_UE *ue, const UE_nr_rxtx_proc_t *proc, nr_phy_data_t *phy_data)
{
  TracyCZone(ctx, true);
  int frame_rx = proc->frame_rx;
  int nr_slot_rx = proc->nr_slot_rx;
  int gNB_id = proc->gNB_id;
  NR_DL_FRAME_PARMS *fp = &ue->frame_parms;
  NR_UE_PDCCH_CONFIG *phy_pdcch_config = &phy_data->phy_pdcch_config;
  int sampleShift = INT_MAX;
  nr_ue_dlsch_init(phy_data->dlsch, NR_MAX_NB_LAYERS>4 ? 2:1, ue->max_ldpc_iterations);
  
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_UE_RX, VCD_FUNCTION_IN);

  LOG_D(PHY," ****** start RX-Chain for Frame.Slot %d.%d ******  \n",
        frame_rx%1024, nr_slot_rx);

  const uint32_t rxdataF_sz = ue->frame_parms.samples_per_slot_wCP;
  __attribute__ ((aligned(32))) c16_t rxdataF[ue->frame_parms.nb_antennas_rx][rxdataF_sz];
  // checking if current frame is compatible with SSB periodicity

  const int default_ssb_period = 2;
  const int ssb_period = ue->received_config_request ? ue->nrUE_config.ssb_table.ssb_period : default_ssb_period;
  if (ssb_period == 0 || !(frame_rx % (1 << (ssb_period - 1)))) {
    const int estimateSz = fp->symbols_per_slot * fp->ofdm_symbol_size;
    // loop over SSB blocks
    for (int ssb_index = 0; ssb_index < fp->Lmax; ssb_index++) {
      // check if current SSB is transmitted
      if (is_ssb_index_transmitted(ue, ssb_index)) {
        int ssb_start_symbol = nr_get_ssb_start_symbol(fp, ssb_index);
        int ssb_slot = ssb_start_symbol/fp->symbols_per_slot;
        int ssb_slot_2 = (ssb_period == 0) ? ssb_slot + (fp->slots_per_frame >> 1) : -1;

        if (ssb_slot == nr_slot_rx || ssb_slot_2 == nr_slot_rx) {
          VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_SLOT_FEP_PBCH, VCD_FUNCTION_IN);
          LOG_D(PHY," ------  PBCH ChannelComp/LLR: frame.slot %d.%d ------  \n", frame_rx%1024, nr_slot_rx);

          __attribute__ ((aligned(32))) struct complex16 dl_ch_estimates[fp->nb_antennas_rx][estimateSz];
          __attribute__ ((aligned(32))) struct complex16 dl_ch_estimates_time[fp->nb_antennas_rx][fp->ofdm_symbol_size];

          for (int i=1; i<4; i++) {
            nr_slot_fep(ue,
                        fp,
                        proc->nr_slot_rx,
                        (ssb_start_symbol + i) % (fp->symbols_per_slot),
                        rxdataF,
                        link_type_dl,
                        0,
                        ue->common_vars.rxdata);

            nr_pbch_channel_estimation(&ue->frame_parms,
                                       NULL,
                                       estimateSz,
                                       dl_ch_estimates,
                                       dl_ch_estimates_time,
                                       proc,
                                       (ssb_start_symbol + i) % (fp->symbols_per_slot),
                                       i - 1,
                                       ssb_index & 7,
                                       ssb_slot_2 == nr_slot_rx,
                                       fp->ssb_start_subcarrier,
                                       rxdataF,
                                       false,
                                       fp->Nid_cell);

            if (i - 1 == 2)
              UEscopeCopy(ue,
                          pbchDlChEstimateTime,
                          (void *)dl_ch_estimates_time,
                          sizeof(c16_t),
                          fp->nb_antennas_rx,
                          fp->ofdm_symbol_size,
                          0);
          }

          nr_ue_ssb_rsrp_measurements(ue, ssb_index, proc, rxdataF);

          // resetting ssb index for PBCH detection if there is a stronger SSB index
          if(ue->measurements.ssb_rsrp_dBm[ssb_index] > ue->measurements.ssb_rsrp_dBm[fp->ssb_index])
            fp->ssb_index = ssb_index;

          if(ssb_index == fp->ssb_index) {

            LOG_D(PHY," ------  Decode MIB: frame.slot %d.%d ------  \n", frame_rx%1024, nr_slot_rx);
            const int pbchSuccess = nr_ue_pbch_procedures(ue, proc, estimateSz, dl_ch_estimates, rxdataF);

            if (ue->no_timing_correction==0 && pbchSuccess == 0) {
              LOG_D(PHY,"start adjust sync slot = %d no timing %d\n", nr_slot_rx, ue->no_timing_correction);
              sampleShift =
                  nr_adjust_synch_ue(fp, ue, gNB_id, fp->ofdm_symbol_size, dl_ch_estimates_time, frame_rx, nr_slot_rx, 16384);
            }

            if (get_nrUE_params()->cont_fo_comp && pbchSuccess == 0) {
              double freq_offset = nr_ue_pbch_freq_offset(fp, estimateSz, dl_ch_estimates);
              LOG_D(PHY,"compensated frequency offset = %.3f Hz, detected residual frequency offset = %.3f Hz, accumulated frequency offset = %.3f Hz\n", ue->freq_offset, freq_offset, ue->freq_off_acc);

              // PI controller
              const double PID_P = get_nrUE_params()->freq_sync_P;
              const double PID_I = get_nrUE_params()->freq_sync_I;
              ue->freq_offset += freq_offset * PID_P + ue->freq_off_acc * PID_I;
              ue->freq_off_acc += freq_offset;
            }
          }
          LOG_D(PHY, "Doing N0 measurements in %s\n", __FUNCTION__);
          nr_ue_rrc_measurements(ue, proc, rxdataF);
          VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_SLOT_FEP_PBCH, VCD_FUNCTION_OUT);
        }
      }
    }
  }

  // Check for PRS slot - section 7.4.1.7.4 in 3GPP rel16 38.211
  for(int gNB_id = 0; gNB_id < ue->prs_active_gNBs; gNB_id++)
  {
    for(int rsc_id = 0; rsc_id < ue->prs_vars[gNB_id]->NumPRSResources; rsc_id++)
    {
      prs_config_t *prs_config = &ue->prs_vars[gNB_id]->prs_resource[rsc_id].prs_cfg;
      for (int i = 0; i < prs_config->PRSResourceRepetition; i++)
      {
        if( (((frame_rx*fp->slots_per_frame + nr_slot_rx) - (prs_config->PRSResourceSetPeriod[1] + prs_config->PRSResourceOffset) + prs_config->PRSResourceSetPeriod[0])%prs_config->PRSResourceSetPeriod[0]) == i*prs_config->PRSResourceTimeGap)
        {
          for(int j = prs_config->SymbolStart; j < (prs_config->SymbolStart+prs_config->NumPRSSymbols); j++)
          {
            nr_slot_fep(ue, fp, proc->nr_slot_rx, (j % fp->symbols_per_slot), rxdataF, link_type_dl, 0, ue->common_vars.rxdata);
          }
          nr_prs_channel_estimation(gNB_id, rsc_id, i, ue, proc, fp, rxdataF);
        }
      } // for i
    } // for rsc_id
  } // for gNB_id

  LOG_D(PHY," ------ --> PDCCH ChannelComp/LLR Frame.slot %d.%d ------  \n", frame_rx%1024, nr_slot_rx);
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_SLOT_FEP_PDCCH, VCD_FUNCTION_IN);

  uint8_t nb_symb_pdcch = phy_pdcch_config->nb_search_space > 0 ? phy_pdcch_config->pdcch_config[0].coreset.duration : 0;
  int first_symb_pdcch = phy_pdcch_config->nb_search_space > 0 ? phy_pdcch_config->pdcch_config[0].coreset.StartSymbolIndex : 0;
  int last_symb_pdcch = first_symb_pdcch + nb_symb_pdcch;
  for (int l = first_symb_pdcch; l < last_symb_pdcch; l++) {
    nr_slot_fep(ue, fp, proc->nr_slot_rx, l, rxdataF, link_type_dl, 0, ue->common_vars.rxdata);
  }

    // Hold the channel estimates in frequency domain.
  int32_t pdcch_est_size = ((((fp->symbols_per_slot*(fp->ofdm_symbol_size+LTE_CE_FILTER_LENGTH))+15)/16)*16);
  __attribute__((aligned(16))) c16_t pdcch_dl_ch_estimates[4 * fp->nb_antennas_rx][pdcch_est_size];

  uint8_t dci_cnt = 0;
  for(int n_ss = 0; n_ss<phy_pdcch_config->nb_search_space; n_ss++) {
    for (int l = first_symb_pdcch; l < last_symb_pdcch; l++) {
      // note: this only works if RBs for PDCCH are contigous!
      nr_pdcch_channel_estimation(ue,
                                  proc,
                                  l,
                                  &phy_pdcch_config->pdcch_config[n_ss].coreset,
                                  fp->first_carrier_offset,
                                  phy_pdcch_config->pdcch_config[n_ss].BWPStart,
                                  pdcch_est_size,
                                  pdcch_dl_ch_estimates,
                                  rxdataF);
    }
    dci_cnt = dci_cnt + nr_ue_pdcch_procedures(ue, proc, pdcch_est_size, pdcch_dl_ch_estimates, phy_data, n_ss, rxdataF);
  }
  LOG_D(PHY, "[UE %d] Frame %d, nr_slot_rx %d: found %d DCIs\n", ue->Mod_id, frame_rx, nr_slot_rx, dci_cnt);
  phy_pdcch_config->nb_search_space = 0;
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_SLOT_FEP_PDCCH, VCD_FUNCTION_OUT);
  TracyCZoneEnd(ctx);
  if (ue->phy_sim_rxdataF)
    memcpy(ue->phy_sim_rxdataF, rxdataF[0], sizeof(int32_t) * nb_symb_pdcch * ue->frame_parms.ofdm_symbol_size);
  return sampleShift;
}

void pdsch_processing(PHY_VARS_NR_UE *ue, const UE_nr_rxtx_proc_t *proc, nr_phy_data_t *phy_data)
{
  int frame_rx = proc->frame_rx;
  int nr_slot_rx = proc->nr_slot_rx;
  int gNB_id = proc->gNB_id;

  NR_UE_DLSCH_t *dlsch = &phy_data->dlsch[0];
  // do procedures for C-RNTI

  bool slot_fep_map[14] = {0};
  const uint32_t rxdataF_sz = ue->frame_parms.samples_per_slot_wCP;
  __attribute__ ((aligned(32))) c16_t rxdataF[ue->frame_parms.nb_antennas_rx][rxdataF_sz];

  // do procedures for CSI-IM
  if (phy_data->csiim_vars.active == 1) {
    for(int symb_idx = 0; symb_idx < 4; symb_idx++) {
      int symb = phy_data->csiim_vars.csiim_config_pdu.l_csiim[symb_idx];
      if (!slot_fep_map[symb]) {
        nr_slot_fep(ue, &ue->frame_parms, proc->nr_slot_rx, symb, rxdataF, link_type_dl, 0, ue->common_vars.rxdata);
        slot_fep_map[symb] = true;
      }
    }
    nr_ue_csi_im_procedures(ue, proc, rxdataF, &phy_data->csiim_vars.csiim_config_pdu);
  }

  // do procedures for CSI-RS
  if (phy_data->csirs_vars.active == 1) {
    for(int symb = 0; symb < NR_SYMBOLS_PER_SLOT; symb++) {
      if(is_csi_rs_in_symbol(phy_data->csirs_vars.csirs_config_pdu, symb)) {
        if (!slot_fep_map[symb]) {
          nr_slot_fep(ue, &ue->frame_parms, proc->nr_slot_rx, symb, rxdataF, link_type_dl, 0, ue->common_vars.rxdata);
          slot_fep_map[symb] = true;
        }
      }
    }
    nr_ue_csi_rs_procedures(ue, proc, rxdataF, &phy_data->csirs_vars.csirs_config_pdu);
  }

  if (dlsch[0].active) {
    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_SLOT_FEP_PDSCH, VCD_FUNCTION_IN);
    fapi_nr_dl_config_dlsch_pdu_rel15_t *dlsch_config = &dlsch[0].dlsch_config;
    uint16_t nb_symb_sch = dlsch_config->number_symbols;
    uint16_t start_symb_sch = dlsch_config->start_symbol;

    LOG_D(PHY," ------ --> PDSCH ChannelComp/LLR Frame.slot %d.%d ------  \n", frame_rx % 1024, nr_slot_rx);

    for (int m = start_symb_sch; m < (nb_symb_sch + start_symb_sch) ; m++) {
      if (!slot_fep_map[m]) {
        nr_slot_fep(ue, &ue->frame_parms, proc->nr_slot_rx, m, rxdataF, link_type_dl, 0, ue->common_vars.rxdata);
        slot_fep_map[m] = true;
      }
    }
    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_SLOT_FEP_PDSCH, VCD_FUNCTION_OUT);

    uint8_t nb_re_dmrs;
    if (dlsch_config->dmrsConfigType == NFAPI_NR_DMRS_TYPE1) {
      nb_re_dmrs = 6 * dlsch_config->n_dmrs_cdm_groups;
    }
    else {
      nb_re_dmrs = 4 * dlsch_config->n_dmrs_cdm_groups;
    }
    uint16_t dmrs_len = get_num_dmrs(dlsch_config->dlDmrsSymbPos);
    uint32_t unav_res = 0;
    if(dlsch_config->pduBitmap & 0x1) {
      uint16_t ptrsSymbPos = 0;
      set_ptrs_symb_idx(&ptrsSymbPos,
                        dlsch_config->number_symbols,
                        dlsch_config->start_symbol,
                        1 << dlsch_config->PTRSTimeDensity,
                        dlsch_config->dlDmrsSymbPos);
      int n_ptrs = (dlsch_config->number_rbs + dlsch_config->PTRSFreqDensity - 1) / dlsch_config->PTRSFreqDensity;
      int ptrsSymbPerSlot = get_ptrs_symbols_in_slot(ptrsSymbPos, dlsch_config->start_symbol, dlsch_config->number_symbols);
      unav_res = n_ptrs * ptrsSymbPerSlot;
    }
    unav_res += compute_csi_rm_unav_res(dlsch_config);
    int G = nr_get_G(dlsch_config->number_rbs,
                     dlsch_config->number_symbols,
                     nb_re_dmrs,
                     dmrs_len,
                     unav_res,
                     dlsch_config->qamModOrder,
                     dlsch[0].Nl);
    const uint32_t rx_llr_buf_sz = ((G + 15) / 16) * 16;
    const uint32_t nb_codewords = NR_MAX_NB_LAYERS > 4 ? 2 : 1;
    int16_t* llr[2];
    for (int i = 0; i < nb_codewords; i++)
      llr[i] = (int16_t *)malloc16_clear(rx_llr_buf_sz * sizeof(int16_t));

    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PDSCH_PROC_C, VCD_FUNCTION_IN);
    // it returns -1 in case of internal failure, or 0 in case of normal result
    int ret_pdsch = nr_ue_pdsch_procedures(ue, proc, dlsch, llr, rxdataF, G);
    TracyCPlot("pdsch mcs", dlsch->dlsch_config.mcs);

    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PDSCH_PROC_C, VCD_FUNCTION_OUT);

    UEscopeCopy(ue, pdschLlr, llr[0], sizeof(int16_t), 1, G, 0);

    LOG_D(PHY, "DLSCH data reception at nr_slot_rx: %d\n", nr_slot_rx);
    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PDSCH_PROC, VCD_FUNCTION_IN);

    start_meas_nr_ue_phy(ue, DLSCH_PROCEDURES_STATS);

    if (ret_pdsch >= 0) {
      nr_ue_dlsch_procedures(ue, proc, dlsch, llr);
    }
    else {
      LOG_E(NR_PHY, "Demodulation impossible, internal error\n");
      if (dlsch_config->k1_feedback) {
        const int ack_nack_slot_and_frame =
            proc->nr_slot_rx + dlsch_config->k1_feedback + proc->frame_rx * ue->frame_parms.slots_per_frame;
        dynamic_barrier_join(&ue->process_slot_tx_barriers[ack_nack_slot_and_frame % NUM_PROCESS_SLOT_TX_BARRIERS]);
      }
      LOG_W(NR_PHY, "nr_ue_pdsch_procedures failed in slot %d\n", proc->nr_slot_rx);
    }

    stop_meas_nr_ue_phy(ue, DLSCH_PROCEDURES_STATS);
    if (cpumeas(CPUMEAS_GETSTATE)) {
      LOG_D(PHY, "[SFN %d] Slot0 Slot1: Dlsch Proc %5.2f\n",nr_slot_rx,ue->phy_cpu_stats.cpu_time_stats[DLSCH_PROCEDURES_STATS].p_time/(cpuf*1000.0));
    }

    if (ue->phy_sim_rxdataF)
      memcpy(ue->phy_sim_rxdataF + start_symb_sch * ue->frame_parms.ofdm_symbol_size * sizeof(c16_t),
             &rxdataF[0][start_symb_sch * ue->frame_parms.ofdm_symbol_size],
             sizeof(int32_t) * nb_symb_sch * ue->frame_parms.ofdm_symbol_size);
    if (ue->phy_sim_pdsch_llr)
      memcpy(ue->phy_sim_pdsch_llr, llr[0], sizeof(int16_t) * rx_llr_buf_sz);

    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PDSCH_PROC, VCD_FUNCTION_OUT);
    for (int i=0; i<nb_codewords; i++)
      free(llr[i]);
  }

  if (nr_slot_rx==9) {
    if (frame_rx % 10 == 0) {
      if ((ue->dlsch_received[gNB_id] - ue->dlsch_received_last[gNB_id]) != 0)
        ue->dlsch_fer[gNB_id] = (100*(ue->dlsch_errors[gNB_id] - ue->dlsch_errors_last[gNB_id]))/(ue->dlsch_received[gNB_id] - ue->dlsch_received_last[gNB_id]);

      ue->dlsch_errors_last[gNB_id] = ue->dlsch_errors[gNB_id];
      ue->dlsch_received_last[gNB_id] = ue->dlsch_received[gNB_id];
    }


    ue->bitrate[gNB_id] = (ue->total_TBS[gNB_id] - ue->total_TBS_last[gNB_id])*100;
    ue->total_TBS_last[gNB_id] = ue->total_TBS[gNB_id];
    LOG_D(PHY,"[UE %d] Calculating bitrate Frame %d: total_TBS = %d, total_TBS_last = %d, bitrate %f kbits\n",
          ue->Mod_id,frame_rx,ue->total_TBS[gNB_id],
          ue->total_TBS_last[gNB_id],(float) ue->bitrate[gNB_id]/1000.0);
  }

#ifdef EMOS
  phy_procedures_emos_UE_RX(ue,slot,gNB_id);
#endif


  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_UE_RX, VCD_FUNCTION_OUT);

  LOG_D(PHY," ****** end RX-Chain  for AbsSubframe %d.%d ******  \n", frame_rx%1024, nr_slot_rx);
  UEscopeCopy(ue, commonRxdataF, rxdataF, sizeof(int32_t), ue->frame_parms.nb_antennas_rx, rxdataF_sz, 0);
}


// todo:
// - power control as per 38.213 ch 7.4
static void nr_ue_prach_procedures(PHY_VARS_NR_UE *ue, const UE_nr_rxtx_proc_t *proc, c16_t **txData)
{
  int gNB_id = proc->gNB_id;
  int frame_tx = proc->frame_tx, nr_slot_tx = proc->nr_slot_tx, prach_power; // tx_amp
  uint8_t mod_id = ue->Mod_id;

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_UE_TX_PRACH, VCD_FUNCTION_IN);

  NR_UE_PRACH *prach_var = ue->prach_vars[gNB_id];
  if (prach_var->active) {
    fapi_nr_ul_config_prach_pdu *prach_pdu = &prach_var->prach_pdu;
    // Generate PRACH in first slot. For L839, the following slots are also filled in this slot.
    if (prach_pdu->prach_slot == nr_slot_tx) {
      ue->tx_power_dBm[nr_slot_tx] = prach_pdu->prach_tx_power;

      LOG_D(PHY,
            "In %s: [UE %d][RAPROC][%d.%d]: Generating PRACH Msg1 (preamble %d, P0_PRACH %d)\n",
            __FUNCTION__,
            mod_id,
            frame_tx,
            nr_slot_tx,
            prach_pdu->ra_PreambleIndex,
            ue->tx_power_dBm[nr_slot_tx]);

      prach_var->amp = AMP;

      VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_GENERATE_PRACH, VCD_FUNCTION_IN);

      start_meas_nr_ue_phy(ue, PRACH_GEN_STATS);
      prach_power = generate_nr_prach(ue, gNB_id, frame_tx, nr_slot_tx, txData);
      stop_meas_nr_ue_phy(ue, PRACH_GEN_STATS);
      if (cpumeas(CPUMEAS_GETSTATE)) {
        LOG_D(PHY,
              "[SFN %d.%d] PRACH Proc %5.2f\n",
              proc->frame_tx,
              proc->nr_slot_tx,
              ue->phy_cpu_stats.cpu_time_stats[PRACH_GEN_STATS].p_time / (cpuf * 1000.0));
      }

      VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_GENERATE_PRACH, VCD_FUNCTION_OUT);

      LOG_D(PHY,
            "In %s: [UE %d][RAPROC][%d.%d]: Generated PRACH Msg1 (TX power PRACH %d dBm, digital power %d dBW (amp %d)\n",
            __FUNCTION__,
            mod_id,
            frame_tx,
            nr_slot_tx,
            ue->tx_power_dBm[nr_slot_tx],
            dB_fixed(prach_power),
            ue->prach_vars[gNB_id]->amp);

      // set duration of prach slots so we know when to skip OFDM modulation
      const int prach_format = ue->prach_vars[gNB_id]->prach_pdu.prach_format;
      const int prach_slots = (prach_format < 4) ? get_long_prach_dur(prach_format, ue->frame_parms.numerology_index) : 1;
      prach_var->num_prach_slots = prach_slots;
    }

    // set as inactive in the last slot
    prach_var->active = !(nr_slot_tx == (prach_pdu->prach_slot + prach_var->num_prach_slots - 1));
  }

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_UE_TX_PRACH, VCD_FUNCTION_OUT);

}
