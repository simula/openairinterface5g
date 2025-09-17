/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.0  (the "License"); you may not use this file
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

/*! \file nr_initial_sync.c
 * \brief Routines for initial UE synchronization procedure (PSS,SSS,PBCH and frame format detection)
 * \author R. Knopp, F. Kaltenberger
 * \date 2011
 * \version 0.1
 * \company Eurecom
 * \email: knopp@eurecom.fr,kaltenberger@eurecom.fr
 * \note
 * \warning
 */
#include "PHY/types.h"
#include "PHY/defs_nr_UE.h"
#include "PHY/MODULATION/modulation_UE.h"
#include "nr_transport_proto_ue.h"
#include "PHY/NR_UE_ESTIMATION/nr_estimation.h"
#include "SCHED_NR_UE/defs.h"
#include "common/utils/LOG/vcd_signal_dumper.h"
#include "common/utils/nr/nr_common.h"

#include "common_lib.h"
#include <math.h>

#include "PHY/NR_REFSIG/pss_nr.h"
#include "PHY/NR_REFSIG/sss_nr.h"
#include "PHY/NR_REFSIG/refsig_defs_ue.h"
#include "PHY/TOOLS/tools_defs.h"
#include "nr-uesoftmodem.h"

//#define DEBUG_INITIAL_SYNCH
#define DUMP_PBCH_CH_ESTIMATES 0

// structure used for multiple SSB detection
typedef struct NR_UE_SSB {
  uint i_ssb; // i_ssb between 0 and 7 (it corresponds to ssb_index only for Lmax=4,8)
  uint n_hf; // n_hf = 0,1 for Lmax =4 or n_hf = 0 for Lmax =8,64
  double metric; // metric to order SSB hypothesis
} NR_UE_SSB;

static int ssb_sort(const void *a, const void *b)
{
  return ((NR_UE_SSB *)b)->metric - ((NR_UE_SSB *)a)->metric;
}

static bool nr_pbch_detection(const UE_nr_rxtx_proc_t *proc,
                              const NR_DL_FRAME_PARMS *frame_parms,
                              int Nid_cell,
                              int pbch_initial_symbol,
                              int ssb_start_subcarrier,
                              int *half_frame_bit,
                              int *ssb_index,
                              int *symbol_offset,
                              fapiPbch_t *result,
                              const c16_t rxdataF[][frame_parms->samples_per_slot_wCP])
{
  const int N_L = (frame_parms->Lmax == 4) ? 4 : 8;
  const int N_hf = (frame_parms->Lmax == 4) ? 2 : 1;
  NR_UE_SSB best_ssb[N_L * N_hf];
  NR_UE_SSB *current_ssb = best_ssb;
  // loops over possible pbch dmrs cases to retrieve best estimated i_ssb (and n_hf for Lmax=4) for multiple ssb detection
  for (int hf = 0; hf < N_hf; hf++) {
    for (int l = 0; l < N_L; l++) {
      // computing correlation between received DMRS symbols and transmitted sequence for current i_ssb and n_hf
      cd_t cumul = {0};
      for (int i = pbch_initial_symbol; i < pbch_initial_symbol + 3; i++) {
        c32_t meas = nr_pbch_dmrs_correlation(frame_parms,
                                              proc,
                                              i,
                                              i - pbch_initial_symbol,
                                              Nid_cell,
                                              ssb_start_subcarrier,
                                              nr_gold_pbch(frame_parms->Lmax, Nid_cell, hf, l),
                                              rxdataF);
        csum(cumul, cumul, meas);
      }
      *current_ssb = (NR_UE_SSB){.i_ssb = l, .n_hf = hf, .metric = squaredMod(cumul)};
      current_ssb++;
    }
  }
  qsort(best_ssb, N_L * N_hf, sizeof(NR_UE_SSB), ssb_sort);

  const int nb_ant = frame_parms->nb_antennas_rx;
  for (NR_UE_SSB *ssb = best_ssb; ssb < best_ssb + N_L * N_hf; ssb++) {
    // computing channel estimation for selected best ssb
    const int estimateSz = frame_parms->symbols_per_slot * frame_parms->ofdm_symbol_size;
    __attribute__((aligned(32))) c16_t dl_ch_estimates[nb_ant][estimateSz];
    __attribute__((aligned(32))) c16_t dl_ch_estimates_time[nb_ant][frame_parms->ofdm_symbol_size];

    for(int i=pbch_initial_symbol; i<pbch_initial_symbol+3;i++)
      nr_pbch_channel_estimation(frame_parms,
                                 NULL,
                                 estimateSz,
                                 dl_ch_estimates,
                                 dl_ch_estimates_time,
                                 proc,
                                 i,
                                 i - pbch_initial_symbol,
                                 ssb->i_ssb,
                                 ssb->n_hf,
                                 ssb_start_subcarrier,
                                 rxdataF,
                                 false,
                                 Nid_cell);

    if (0
        == nr_rx_pbch(NULL,
                      proc,
                      false,
                      estimateSz,
                      dl_ch_estimates,
                      frame_parms,
                      ssb->i_ssb,
                      ssb_start_subcarrier,
                      Nid_cell,
                      result,
                      half_frame_bit,
                      ssb_index,
                      symbol_offset,
                      frame_parms->samples_per_frame_wCP,
                      rxdataF)) {
      if (DUMP_PBCH_CH_ESTIMATES) {
        write_output("pbch_ch_estimates.m", "pbch_ch_estimates", dl_ch_estimates, nb_ant * estimateSz, 1, 1);
        write_output("pbch_ch_estimates_time.m",
                     "pbch_ch_estimates_time",
                     dl_ch_estimates_time,
                     nb_ant * frame_parms->ofdm_symbol_size,
                     1,
                     1);
      }
      LOG_I(PHY, "Initial sync: pbch decoded sucessfully, ssb index %d\n", *ssb_index);
      return true;
    }
  }

  LOG_W(PHY, "Initial sync: pbch not decoded, ssb index %d\n", frame_parms->ssb_index);
  return false;
}

static void compensate_freq_offset(c16_t **x, const NR_DL_FRAME_PARMS *fp, const int offset, const int sn)
{
  double s_time = 1 / (1.0e3 * fp->samples_per_subframe); // sampling time
  double off_angle = -2 * M_PI * s_time * (offset); // offset rotation angle compensation per sample

  for (int n = sn * fp->samples_per_frame; n < (sn + 1) * fp->samples_per_frame; n++) {
    for (int ar = 0; ar < fp->nb_antennas_rx; ar++) {
      const double re = x[ar][n].r;
      const double im = x[ar][n].i;
      x[ar][n].r = (short)(round(re * cos(n * off_angle) - im * sin(n * off_angle)));
      x[ar][n].i = (short)(round(re * sin(n * off_angle) + im * cos(n * off_angle)));
    }
  }
}

void nr_scan_ssb(void *arg)
{
  /*   Initial synchronisation
   *
   *                                 1 radio frame = 10 ms
   *     <--------------------------------------------------------------------------->
   *     -----------------------------------------------------------------------------
   *     |                                 Received UE data buffer                    |
   *     ----------------------------------------------------------------------------
   *                     --------------------------
   *     <-------------->| pss | pbch | sss | pbch |
   *                     --------------------------
   *          sync_pos            SS/PBCH block
   */

  nr_ue_ssb_scan_t *ssbInfo = (nr_ue_ssb_scan_t *)arg;
  c16_t **rxdata = ssbInfo->rxdata;
  const NR_DL_FRAME_PARMS *fp = ssbInfo->fp;

  // Generate PSS time signal for this GSCN.
  __attribute__((aligned(32))) c16_t pssTime[NUMBER_PSS_SEQUENCE][fp->ofdm_symbol_size];
  const int pss_sequence = get_softmodem_params()->sl_mode == 0 ? NUMBER_PSS_SEQUENCE : NUMBER_PSS_SEQUENCE_SL;
  for (int nid2 = 0; nid2 < pss_sequence; nid2++)
    generate_pss_nr_time(fp, nid2, ssbInfo->gscnInfo.ssbFirstSC, pssTime[nid2]);

  // initial sync performed on two successive frames, if pbch passes on first frame, no need to process second frame
  // only one frame is used for simulation tools
  for (int frame_id = 0; frame_id < ssbInfo->nFrames && !ssbInfo->syncRes.cell_detected; frame_id++) {
    /* process pss search on received buffer */
    ssbInfo->syncRes.frame_id = frame_id;
    int nid2;
    int freq_offset_pss = 0;
    const int sync_pos = pss_synchro_nr((const c16_t **)rxdata,
                                        fp,
                                        pssTime,
                                        frame_id,
                                        ssbInfo->foFlag,
                                        ssbInfo->targetNidCell,
                                        &nid2,
                                        &freq_offset_pss,
                                        &ssbInfo->pssCorrPeakPower,
                                        &ssbInfo->pssCorrAvgPower);
    if (sync_pos < fp->nb_prefix_samples)
      continue;

    ssbInfo->ssbOffset = sync_pos - fp->nb_prefix_samples;

#ifdef DEBUG_INITIAL_SYNCH
    LOG_I(PHY, "Initial sync : Estimated PSS position %d, Nid2 %d, ssb offset %d\n", sync_pos, nid2, ssbInfo->ssbOffset);
#endif
    /* check that SSS/PBCH block is continuous inside the received buffer */
    if (ssbInfo->ssbOffset + NR_N_SYMBOLS_SSB * (fp->ofdm_symbol_size + fp->nb_prefix_samples) >= fp->samples_per_frame) {
      LOG_D(PHY, "Can't try to decode SSS from PSS position, will retry (PSS circular buffer wrapping): sync_pos %d\n", sync_pos);
      continue;
    }

    // digital compensation of FFO for SSB symbols
    if (ssbInfo->foFlag) {
      compensate_freq_offset(rxdata, fp, freq_offset_pss, frame_id);
    }

    /* slot_fep function works for lte and takes into account begining of frame with prefix for subframe 0 */
    /* for NR this is not the case but slot_fep is still used for computing FFT of samples */
    /* in order to achieve correct processing for NR prefix samples is forced to 0 and then restored after function call */
    /* symbol number are from beginning of SS/PBCH blocks as below:  */
    /*    Signal            PSS  PBCH  SSS  PBCH                     */
    /*    symbol number      0     1    2    3                       */
    /* time samples in buffer rxdata are used as input of FFT -> FFT results are stored in the frequency buffer rxdataF */
    /* rxdataF stores SS/PBCH from beginning of buffers in the same symbol order as in time domain */

    const uint32_t rxdataF_sz = fp->samples_per_slot_wCP;
    __attribute__((aligned(32))) c16_t rxdataF[fp->nb_antennas_rx][rxdataF_sz];
    for (int i = 0; i < NR_N_SYMBOLS_SSB; i++)
      nr_slot_fep(NULL, fp, 0, i, rxdataF, link_type_dl, frame_id * fp->samples_per_frame + ssbInfo->ssbOffset, (c16_t **)rxdata);

    int freq_offset_sss = 0;
    int32_t metric_tdd_ncp = 0;
    uint8_t phase_tdd_ncp;
    ssbInfo->syncRes.cell_detected = rx_sss_nr(fp,
                                               nid2,
                                               ssbInfo->targetNidCell,
                                               freq_offset_pss,
                                               ssbInfo->gscnInfo.ssbFirstSC,
                                               &ssbInfo->nidCell,
                                               &metric_tdd_ncp,
                                               &phase_tdd_ncp,
                                               &freq_offset_sss,
                                               rxdataF);
#ifdef DEBUG_INITIAL_SYNCH
    LOG_I(PHY,
          "TDD Normal prefix: sss detection result; %d, CellId %d metric %d, phase %d, measured offset %d\n",
          ssbInfo->syncRes.cell_detected,
          ssbInfo->nidCell,
          metric_tdd_ncp,
          phase_tdd_ncp,
          ssbInfo->syncRes.rx_offset);
#endif
    ssbInfo->freqOffset = freq_offset_pss + freq_offset_sss;

    if (ssbInfo->syncRes.cell_detected) { // we got sss channel
      ssbInfo->syncRes.cell_detected = nr_pbch_detection(ssbInfo->proc,
                                                         ssbInfo->fp,
                                                         ssbInfo->nidCell,
                                                         1,
                                                         ssbInfo->gscnInfo.ssbFirstSC,
                                                         &ssbInfo->halfFrameBit,
                                                         &ssbInfo->ssbIndex,
                                                         &ssbInfo->symbolOffset,
                                                         &ssbInfo->pbchResult,
                                                         rxdataF); // start pbch detection at first symbol after pss
      if (ssbInfo->syncRes.cell_detected) {
        int rsrp_db_per_re = nr_ue_calculate_ssb_rsrp(ssbInfo->fp, ssbInfo->proc, rxdataF, 0, ssbInfo->gscnInfo.ssbFirstSC);
        ssbInfo->adjust_rxgain = TARGET_RX_POWER - rsrp_db_per_re;
        LOG_I(PHY, "pbch rx ok. rsrp:%d dB/RE, adjust_rxgain:%d dB\n", rsrp_db_per_re, ssbInfo->adjust_rxgain);
      }
    }
  }
}

nr_initial_sync_t nr_initial_sync(UE_nr_rxtx_proc_t *proc,
                                  PHY_VARS_NR_UE *ue,
                                  int n_frames,
                                  int sa,
                                  nr_gscn_info_t gscnInfo[MAX_GSCN_BAND],
                                  int numGscn)
{
  NR_DL_FRAME_PARMS *fp = &ue->frame_parms;

  notifiedFIFO_t nf;
  initNotifiedFIFO(&nf);

  // Perform SSB scanning in parallel. One GSCN per thread.
  LOG_I(NR_PHY,
        "Starting cell search with center freq: %ld, bandwidth: %d. Scanning for %d number of GSCN.\n",
        fp->dl_CarrierFreq,
        fp->N_RB_DL,
        numGscn);
  for (int s = 0; s < numGscn; s++) {
    notifiedFIFO_elt_t *req = newNotifiedFIFO_elt(sizeof(nr_ue_ssb_scan_t), gscnInfo[s].gscn, &nf, &nr_scan_ssb);
    nr_ue_ssb_scan_t *ssbInfo = (nr_ue_ssb_scan_t *)NotifiedFifoData(req);
    *ssbInfo = (nr_ue_ssb_scan_t){.gscnInfo = gscnInfo[s],
                                  .fp = &ue->frame_parms,
                                  .proc = proc,
                                  .syncRes.cell_detected = false,
                                  .nFrames = n_frames,
                                  .foFlag = ue->UE_fo_compensation,
                                  .targetNidCell = ue->target_Nid_cell};
    ssbInfo->rxdata = malloc16_clear(fp->nb_antennas_rx * sizeof(c16_t *));
    for (int ant = 0; ant < fp->nb_antennas_rx; ant++) {
      ssbInfo->rxdata[ant] = malloc16(sizeof(c16_t) * (fp->samples_per_frame * 2 + fp->ofdm_symbol_size));
      memcpy(ssbInfo->rxdata[ant], ue->common_vars.rxdata[ant], sizeof(c16_t) * fp->samples_per_frame * 2);
      memset(ssbInfo->rxdata[ant] + fp->samples_per_frame * 2, 0, fp->ofdm_symbol_size * sizeof(c16_t));
    }
    LOG_I(NR_PHY,
          "Scanning GSCN: %d, with SSB offset: %d, SSB Freq: %lf\n",
          ssbInfo->gscnInfo.gscn,
          ssbInfo->gscnInfo.ssbFirstSC,
          ssbInfo->gscnInfo.ssRef);
    pushTpool(&get_nrUE_params()->Tpool, req);
  }

  // Collect the scan results
  nr_ue_ssb_scan_t res = {0};
  while (numGscn) {
    notifiedFIFO_elt_t *req = pullTpool(&nf, &get_nrUE_params()->Tpool);
    nr_ue_ssb_scan_t *ssbInfo = (nr_ue_ssb_scan_t *)NotifiedFifoData(req);
    if (ssbInfo->syncRes.cell_detected) {
      LOG_A(NR_PHY,
            "Cell Detected with GSCN: %d, SSB SC offset: %d, SSB Ref: %lf, PSS Corr peak: %d dB, PSS Corr Average: %d\n",
            ssbInfo->gscnInfo.gscn,
            ssbInfo->gscnInfo.ssbFirstSC,
            ssbInfo->gscnInfo.ssRef,
            ssbInfo->pssCorrPeakPower,
            ssbInfo->pssCorrAvgPower);
      if (!res.syncRes.cell_detected) { // take the first cell detected
        res = *ssbInfo;
      }
    }
    for (int ant = 0; ant < fp->nb_antennas_rx; ant++) {
      free(ssbInfo->rxdata[ant]);
    }
    free(ssbInfo->rxdata);
    delNotifiedFIFO_elt(req);
    numGscn--;
  }

  // Set globals based on detected cell
  if (res.syncRes.cell_detected) {
    fp->Nid_cell = res.nidCell;
    fp->ssb_start_subcarrier = res.gscnInfo.ssbFirstSC;
    fp->half_frame_bit = res.halfFrameBit;
    fp->ssb_index = res.ssbIndex;
    ue->symbol_offset = res.symbolOffset;
    ue->common_vars.freq_offset = res.freqOffset;
    ue->adjust_rxgain = res.adjust_rxgain;
  }

  // In initial sync, we indicate PBCH to MAC after the scan is complete.
  nr_downlink_indication_t dl_indication;
  fapi_nr_rx_indication_t rx_ind = {0};
  uint16_t number_pdus = 1;
  nr_fill_dl_indication(&dl_indication, NULL, &rx_ind, proc, ue, NULL);
  nr_fill_rx_indication(&rx_ind,
                        FAPI_NR_RX_PDU_TYPE_SSB,
                        ue,
                        NULL,
                        NULL,
                        number_pdus,
                        proc,
                        res.syncRes.cell_detected ? (void *)&res.pbchResult : NULL,
                        NULL);

  if (ue->if_inst && ue->if_inst->dl_indication)
    ue->if_inst->dl_indication(&dl_indication);
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_NR_INITIAL_UE_SYNC, VCD_FUNCTION_IN);

  LOG_D(PHY, "nr_initial sync ue RB_DL %d\n", fp->N_RB_DL);

  if (res.syncRes.cell_detected) {
    // digital compensation of FFO for SSB symbols
    if (res.freqOffset && ue->UE_fo_compensation) {
      // In SA we need to perform frequency offset correction until the end of buffer because we need to decode SIB1
      // and we do not know yet in which slot it goes.
      compensate_freq_offset(ue->common_vars.rxdata, fp, res.freqOffset, res.syncRes.frame_id);
    }
    // sync at symbol ue->symbol_offset
    // computing the offset wrt the beginning of the frame
    int mu = fp->numerology_index;
    // number of symbols with different prefix length
    // every 7*(1<<mu) symbols there is a different prefix length (38.211 5.3.1)
    int n_symb_prefix0 = (res.symbolOffset / (7 * (1 << mu))) + 1;
    const int sync_pos_frame = n_symb_prefix0 * (fp->ofdm_symbol_size + fp->nb_prefix_samples0)
                               + (res.symbolOffset - n_symb_prefix0) * (fp->ofdm_symbol_size + fp->nb_prefix_samples);
    // for a correct computation of frame number to sync with the one decoded at MIB we need to take into account in which of
    // the n_frames we got sync
    ue->init_sync_frame = n_frames - 1 - res.syncRes.frame_id;

    // we also need to take into account the shift by samples_per_frame in case the if is true
    if (res.ssbOffset < sync_pos_frame) {
      res.syncRes.rx_offset = fp->samples_per_frame - sync_pos_frame + res.ssbOffset;
      ue->init_sync_frame += 1;
    } else
      res.syncRes.rx_offset = res.ssbOffset - sync_pos_frame;
  }

    if (res.syncRes.cell_detected) {
      LOG_I(PHY, "[UE%d] In synch, rx_offset %d samples\n", ue->Mod_id, res.syncRes.rx_offset);
      LOG_I(PHY, "[UE %d] Measured Carrier Frequency offset %d Hz\n", ue->Mod_id, res.freqOffset);
    } else {
#ifdef DEBUG_INITIAL_SYNC
    LOG_I(PHY,"[UE%d] Initial sync : PBCH not ok\n",ue->Mod_id);
    LOG_I(PHY, "[UE%d] Initial sync : Estimated PSS position %d, Nid2 %d\n", ue->Mod_id, sync_pos, ue->common_vars.nid2);
    LOG_I(PHY,"[UE%d] Initial sync : Estimated Nid_cell %d, Frame_type %d\n",ue->Mod_id,
          fp->Nid_cell,fp->frame_type);
#endif
    }

  // gain control
    if (!res.syncRes.cell_detected) { // we are not synched, so we cannot use rssi measurement (which is based on channel estimates)
      int rx_power = 0;

      // do a measurement on the best guess of the PSS
      // for (aarx=0; aarx<frame_parms->nb_antennas_rx; aarx++)
      //  rx_power += signal_energy(&ue->common_vars.rxdata[aarx][sync_pos2],
      //			frame_parms->ofdm_symbol_size+frame_parms->nb_prefix_samples);

      /*
      // do a measurement on the full frame
      for (aarx=0; aarx<frame_parms->nb_antennas_rx; aarx++)
      rx_power += signal_energy(&ue->common_vars.rxdata[aarx][0],
      frame_parms->samples_per_subframe*10);
      */

      // we might add a low-pass filter here later
      ue->measurements.rx_power_avg[0] = rx_power / fp->nb_antennas_rx;

      ue->measurements.rx_power_avg_dB[0] = dB_fixed(ue->measurements.rx_power_avg[0]);

#ifdef DEBUG_INITIAL_SYNCH
    LOG_I(PHY, "[UE%d] Initial sync failed : Estimated power: %d dB\n", ue->Mod_id, ue->measurements.rx_power_avg_dB[0]);
#endif
    } else {
      LOG_A(PHY, "Initial sync successful, PCI: %d\n",fp->Nid_cell);
    }
  //  exit_fun("debug exit");
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_NR_INITIAL_UE_SYNC, VCD_FUNCTION_OUT);
  return res.syncRes;
}
