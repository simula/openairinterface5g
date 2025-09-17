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

/*! \file PHY/NR_UE_TRANSPORT/nr_dlsch_decoding_slot.c
 */

#include "common/utils/LOG/vcd_signal_dumper.h"
#include "PHY/defs_nr_UE.h"
#include "SCHED_NR_UE/harq_nr.h"
#include "PHY/phy_extern_nr_ue.h"
#include "PHY/CODING/coding_extern.h"
#include "PHY/CODING/coding_defs.h"
#include "PHY/CODING/nrLDPC_coding/nrLDPC_coding_interface.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "SCHED_NR_UE/defs.h"
#include "SIMULATION/TOOLS/sim.h"
#include "executables/nr-uesoftmodem.h"
#include "PHY/CODING/nrLDPC_extern.h"
#include "common/utils/nr/nr_common.h"
#include "openair1/PHY/TOOLS/phy_scope_interface.h"
#include "nfapi/open-nFAPI/nfapi/public_inc/nfapi_nr_interface.h"

static extended_kpi_ue kpiStructure = {0};

extended_kpi_ue* getKPIUE(void) {
  return &kpiStructure;
}

void nr_ue_dlsch_init(NR_UE_DLSCH_t *dlsch_list, int num_dlsch, uint8_t max_ldpc_iterations) {
  for (int i=0; i < num_dlsch; i++) {
    NR_UE_DLSCH_t *dlsch = dlsch_list + i;
    memset(dlsch, 0, sizeof(NR_UE_DLSCH_t));
    dlsch->max_ldpc_iterations = max_ldpc_iterations;
  }
}

void nr_dlsch_unscrambling(int16_t *llr, uint32_t size, uint8_t q, uint32_t Nid, uint32_t n_RNTI)
{
  nr_codeword_unscrambling(llr, size, q, Nid, n_RNTI);
}

/*! \brief Prepare necessary parameters for nrLDPC_coding_interface
 */
void nr_dlsch_decoding(PHY_VARS_NR_UE *phy_vars_ue,
                       const UE_nr_rxtx_proc_t *proc,
                       NR_UE_DLSCH_t *dlsch,
                       int16_t **dlsch_llr,
                       uint8_t **b,
                       int *G,
                       int nb_dlsch,
                       uint8_t *DLSCH_ids)
{
  nrLDPC_TB_decoding_parameters_t TBs[nb_dlsch];
  memset(TBs, 0, sizeof(TBs));
  nrLDPC_slot_decoding_parameters_t slot_parameters = {
    .frame = proc->frame_rx,
    .slot = proc->nr_slot_rx,
    .nb_TBs = nb_dlsch,
    .threadPool = &get_nrUE_params()->Tpool,
    .TBs = TBs
  };

  int max_num_segments = 0;

  for (uint8_t pdsch_id = 0; pdsch_id < nb_dlsch; pdsch_id++) {
    uint8_t DLSCH_id = DLSCH_ids[pdsch_id];
    fapi_nr_dl_config_dlsch_pdu_rel15_t *dlsch_config = &dlsch[DLSCH_id].dlsch_config;
    uint8_t dmrs_Type = dlsch_config->dmrsConfigType;
    int harq_pid = dlsch_config->harq_process_nbr;
    NR_DL_UE_HARQ_t *harq_process = &phy_vars_ue->dl_harq_processes[DLSCH_id][harq_pid];

    AssertFatal(dmrs_Type == 0 || dmrs_Type == 1, "Illegal dmrs_type %d\n", dmrs_Type);
    uint8_t nb_re_dmrs;

    LOG_D(PHY, "Round %d RV idx %d\n", harq_process->DLround, dlsch->dlsch_config.rv);

    if (dmrs_Type == NFAPI_NR_DMRS_TYPE1)
      nb_re_dmrs = 6 * dlsch_config->n_dmrs_cdm_groups;
    else
      nb_re_dmrs = 4 * dlsch_config->n_dmrs_cdm_groups;
    uint16_t dmrs_length = get_num_dmrs(dlsch_config->dlDmrsSymbPos);

    if (!harq_process) {
      LOG_E(PHY, "dlsch_decoding_slot.c: NULL harq_process pointer\n");
      return;
    }

    nrLDPC_TB_decoding_parameters_t *TB_parameters = &TBs[pdsch_id];

    /* Neither harq_pid nor DLSCH_id are unique in the instance
     * but their combination is.
     * Since DLSCH_id < 2
     * then 2 * harq_pid + DLSCH_id is unique.
     */
    TB_parameters->harq_unique_pid = 2 * harq_pid + DLSCH_id;

    // ------------------------------------------------------------------
    TB_parameters->G = G[DLSCH_id];
    TB_parameters->nb_rb = dlsch_config->number_rbs;
    TB_parameters->Qm = dlsch_config->qamModOrder;
    TB_parameters->mcs = dlsch_config->mcs;
    TB_parameters->nb_layers = dlsch[DLSCH_id].Nl;
    TB_parameters->BG = dlsch_config->ldpcBaseGraph;
    TB_parameters->A = dlsch_config->TBS;
    // ------------------------------------------------------------------

    TB_parameters->processedSegments = &harq_process->processedSegments;

    float Coderate = (float)dlsch->dlsch_config.targetCodeRate / 10240.0f;

    LOG_D(
        PHY,
        "%d.%d DLSCH %d Decoding, harq_pid %d TBS %d G %d nb_re_dmrs %d length dmrs %d mcs %d Nl %d nb_symb_sch %d nb_rb %d Qm %d "
        "Coderate %f\n",
        slot_parameters.frame,
        slot_parameters.slot,
        DLSCH_id,
        harq_pid,
        dlsch_config->TBS,
        TB_parameters->G,
        nb_re_dmrs,
        dmrs_length,
        TB_parameters->mcs,
        TB_parameters->nb_layers,
        dlsch_config->number_symbols,
        TB_parameters->nb_rb,
        TB_parameters->Qm,
        Coderate);

    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_DLSCH_SEGMENTATION, VCD_FUNCTION_IN);

    if (harq_process->first_rx == 1) {
      // This is a new packet, so compute quantities regarding segmentation
      nr_segmentation(NULL,
                      NULL,
                      lenWithCrc(1, TB_parameters->A), // We give a max size in case of 1 segment
                      &TB_parameters->C,
                      &TB_parameters->K,
                      &TB_parameters->Z, // [hna] Z is Zc
                      &TB_parameters->F,
                      TB_parameters->BG);
      harq_process->C = TB_parameters->C;
      harq_process->K = TB_parameters->K;
      harq_process->Z = TB_parameters->Z;
      harq_process->F = TB_parameters->F;

      if (harq_process->C > MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER * TB_parameters->nb_layers) {
        LOG_E(PHY, "nr_segmentation.c: too many segments %d, A %d\n", harq_process->C, TB_parameters->A);
        return;
      }

      if (LOG_DEBUGFLAG(DEBUG_DLSCH_DECOD) && (!slot_parameters.frame % 100))
        LOG_I(PHY, "K %d C %d Z %d nl %d \n", harq_process->K, harq_process->C, harq_process->Z, TB_parameters->nb_layers);
      // clear HARQ buffer
      for (int i = 0; i < harq_process->C; i++)
        memset(harq_process->d[i], 0, 5 * 8448 * sizeof(int16_t));
    } else {
      // This is not a new packet, so retrieve previously computed quantities regarding segmentation
      TB_parameters->C = harq_process->C;
      TB_parameters->K = harq_process->K;
      TB_parameters->Z = harq_process->Z;
      TB_parameters->F = harq_process->F;
    }
    max_num_segments = max(max_num_segments, TB_parameters->C);

    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_DLSCH_SEGMENTATION, VCD_FUNCTION_OUT);

    if (LOG_DEBUGFLAG(DEBUG_DLSCH_DECOD))
      LOG_I(PHY, "Segmentation: C %d, K %d\n", harq_process->C, harq_process->K);

    TB_parameters->max_ldpc_iterations = dlsch[DLSCH_id].max_ldpc_iterations;
    TB_parameters->rv_index = dlsch_config->rv;
    TB_parameters->tbslbrm = dlsch_config->tbslbrm;
    TB_parameters->abort_decode = &harq_process->abort_decode;
    set_abort(&harq_process->abort_decode, false);
  }

  nrLDPC_segment_decoding_parameters_t segments[nb_dlsch][max_num_segments];
  memset(segments, 0, sizeof(segments));
  bool d_to_be_cleared[nb_dlsch][max_num_segments];
  memset(d_to_be_cleared, 0, sizeof(d_to_be_cleared));

  for (uint8_t pdsch_id = 0; pdsch_id < nb_dlsch; pdsch_id++) {
    uint8_t DLSCH_id = DLSCH_ids[pdsch_id];
    fapi_nr_dl_config_dlsch_pdu_rel15_t *dlsch_config = &dlsch[DLSCH_id].dlsch_config;
    int harq_pid = dlsch_config->harq_process_nbr;
    NR_DL_UE_HARQ_t *harq_process = &phy_vars_ue->dl_harq_processes[DLSCH_id][harq_pid];

    nrLDPC_TB_decoding_parameters_t *TB_parameters = &TBs[pdsch_id];
    TB_parameters->segments = segments[pdsch_id];

    uint32_t r_offset = 0;
    for (int r = 0; r < TB_parameters->C; r++) {
      if (harq_process->first_rx == 1)
        d_to_be_cleared[pdsch_id][r] = true;
      else
        d_to_be_cleared[pdsch_id][r] = false;
      nrLDPC_segment_decoding_parameters_t *segment_parameters = &TB_parameters->segments[r];
      segment_parameters->E = nr_get_E(TB_parameters->G,
                                       TB_parameters->C,
                                       TB_parameters->Qm,
                                       TB_parameters->nb_layers,
                                       r);
      segment_parameters->R = nr_get_R_ldpc_decoder(TB_parameters->rv_index,
                                                   segment_parameters->E,
                                                   TB_parameters->BG,
                                                   TB_parameters->Z,
                                                   &harq_process->llrLen,
                                                   harq_process->DLround);
      segment_parameters->llr = dlsch_llr[DLSCH_id] + r_offset;
      segment_parameters->d = harq_process->d[r];
      segment_parameters->d_to_be_cleared = &d_to_be_cleared[pdsch_id][r];
      segment_parameters->c = harq_process->c[r];
      segment_parameters->decodeSuccess = false;

      reset_meas(&segment_parameters->ts_deinterleave);
      reset_meas(&segment_parameters->ts_rate_unmatch);
      reset_meas(&segment_parameters->ts_ldpc_decode);

      r_offset += segment_parameters->E;
    }
  }

  int ret_decoder = phy_vars_ue->nrLDPC_coding_interface.nrLDPC_coding_decoder(&slot_parameters);

  if (ret_decoder != 0) {
    LOG_E(PHY, "nrLDPC_coding_decoder returned an error: %d\n", ret_decoder);
    return;
  }

  // post decode
  for (uint8_t pdsch_id = 0; pdsch_id < nb_dlsch; pdsch_id++) {
    uint8_t DLSCH_id = DLSCH_ids[pdsch_id];
    fapi_nr_dl_config_dlsch_pdu_rel15_t *dlsch_config = &dlsch[DLSCH_id].dlsch_config;
    int harq_pid = dlsch_config->harq_process_nbr;
    NR_DL_UE_HARQ_t *harq_process = &phy_vars_ue->dl_harq_processes[DLSCH_id][harq_pid];

    nrLDPC_TB_decoding_parameters_t *TB_parameters = &TBs[pdsch_id];

    uint32_t offset = 0;
    for (int r = 0; r < TB_parameters->C; r++) {
      nrLDPC_segment_decoding_parameters_t *segment_parameters = &TB_parameters->segments[r];
      if (segment_parameters->decodeSuccess) {
        memcpy(b[DLSCH_id] + offset,
               harq_process->c[r],
               (harq_process->K >> 3) - (harq_process->F >> 3) - ((harq_process->C > 1) ? 3 : 0));
      } else {
        fapi_nr_dl_config_dlsch_pdu_rel15_t *dlsch_config = &dlsch[DLSCH_id].dlsch_config;
        LOG_D(PHY, "frame=%d, slot=%d, first_rx=%d, rv_index=%d\n", proc->frame_rx, proc->nr_slot_rx, harq_process->first_rx, dlsch_config->rv);
        LOG_D(PHY, "downlink segment error %d/%d\n", r, harq_process->C);
        LOG_D(PHY, "DLSCH %d in error\n", DLSCH_id);
      }
      offset += (harq_process->K >> 3) - (harq_process->F >> 3) - ((harq_process->C > 1) ? 3 : 0);

      merge_meas(&phy_vars_ue->phy_cpu_stats.cpu_time_stats[DLSCH_DEINTERLEAVING_STATS], &segment_parameters->ts_deinterleave);
      merge_meas(&phy_vars_ue->phy_cpu_stats.cpu_time_stats[DLSCH_RATE_UNMATCHING_STATS], &segment_parameters->ts_rate_unmatch);
      merge_meas(&phy_vars_ue->phy_cpu_stats.cpu_time_stats[DLSCH_LDPC_DECODING_STATS], &segment_parameters->ts_ldpc_decode);

    }

    kpiStructure.nb_total++;
    kpiStructure.blockSize = dlsch_config->TBS;
    kpiStructure.dl_mcs = dlsch_config->mcs;
    kpiStructure.nofRBs = dlsch_config->number_rbs;

    harq_process->decodeResult = harq_process->processedSegments == harq_process->C;

    if (harq_process->decodeResult && harq_process->C > 1) {
      /* check global CRC */
      int A = dlsch->dlsch_config.TBS;
      // we have regrouped the transport block
      if (!check_crc(b[DLSCH_id], lenWithCrc(1, A), crcType(1, A))) {
        LOG_E(PHY,
              " Frame %d.%d LDPC global CRC fails, but individual LDPC CRC succeeded. %d segs\n",
              proc->frame_rx,
              proc->nr_slot_rx,
              harq_process->C);
        harq_process->decodeResult = false;
      }
    }

    if (harq_process->decodeResult) {
      // We search only a reccuring OAI error that propagates all 0 packets with a 0 CRC, so we
      const int sz = dlsch->dlsch_config.TBS / 8;
      if (b[DLSCH_id][sz] == 0 && b[DLSCH_id][sz + 1] == 0) {
        // do the check only if the 2 first bytes of the CRC are 0 (it can be CRC16 or CRC24)
        int i = 0;
        while (b[DLSCH_id][i] == 0 && i < sz)
          i++;
        if (i == sz) {
          LOG_E(PHY,
                "received all 0 pdu, consider it false reception, even if the TS 38.212 7.2.1 says only we should attach the "
                "corresponding CRC, and nothing prevents to have a all 0 packet\n");
          harq_process->decodeResult = false;
        }
      }
    }

    if (harq_process->decodeResult) {
      LOG_D(PHY, "DLSCH received ok \n");
      harq_process->status = SCH_IDLE;
      dlsch->last_iteration_cnt = dlsch->max_ldpc_iterations - 1;
    } else {
      LOG_D(PHY, "DLSCH received nok \n");
      kpiStructure.nb_nack++;
      dlsch->last_iteration_cnt = dlsch->max_ldpc_iterations;
      UEdumpScopeData(phy_vars_ue, proc->nr_slot_rx, proc->frame_rx, "DLSCH_NACK");
    }

    uint8_t dmrs_Type = dlsch_config->dmrsConfigType;
    uint8_t nb_re_dmrs;
    if (dmrs_Type == NFAPI_NR_DMRS_TYPE1)
      nb_re_dmrs = 6 * dlsch_config->n_dmrs_cdm_groups;
    else
      nb_re_dmrs = 4 * dlsch_config->n_dmrs_cdm_groups;
    uint16_t dmrs_length = get_num_dmrs(dlsch_config->dlDmrsSymbPos);
    float Coderate = (float)dlsch->dlsch_config.targetCodeRate / 10240.0f;
    LOG_D(PHY,
          "%d.%d DLSCH Decoded, harq_pid %d, round %d, result: %d TBS %d (%d) G %d nb_re_dmrs %d length dmrs %d mcs %d Nl %d "
          "nb_symb_sch %d "
          "nb_rb %d Qm %d Coderate %f\n",
          proc->frame_rx,
          proc->nr_slot_rx,
          harq_pid,
          harq_process->DLround,
          harq_process->decodeResult,
          dlsch->dlsch_config.TBS,
          dlsch->dlsch_config.TBS / 8,
          G[DLSCH_id],
          nb_re_dmrs,
          dmrs_length,
          dlsch->dlsch_config.mcs,
          dlsch->Nl,
          dlsch_config->number_symbols,
          dlsch_config->number_rbs,
          dlsch_config->qamModOrder,
          Coderate);

  }

}
