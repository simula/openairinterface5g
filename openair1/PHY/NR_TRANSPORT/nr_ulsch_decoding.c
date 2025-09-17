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

/*! \file PHY/NR_TRANSPORT/nr_ulsch_decoding_slot.c
 * \brief Top-level routines for decoding  LDPC (ULSCH) transport channels from 38.212, V15.4.0 2018-12
 */

// [from gNB coding]
#include "PHY/defs_gNB.h"
#include "PHY/CODING/coding_extern.h"
#include "PHY/CODING/coding_defs.h"
#include "PHY/CODING/lte_interleaver_inline.h"
#include "PHY/CODING/nrLDPC_coding/nrLDPC_coding_interface.h"
#include "PHY/CODING/nrLDPC_extern.h"
#include "PHY/NR_TRANSPORT/nr_transport_common_proto.h"
#include "PHY/NR_TRANSPORT/nr_transport_proto.h"
#include "PHY/NR_TRANSPORT/nr_ulsch.h"
#include "PHY/NR_TRANSPORT/nr_dlsch.h"
#include "SCHED_NR/sched_nr.h"
#include "SCHED_NR/fapi_nr_l1.h"
#include "defs.h"
#include "common/utils/LOG/vcd_signal_dumper.h"
#include "common/utils/LOG/log.h"
#include <syscall.h>
// #define DEBUG_ULSCH_DECODING
// #define gNB_DEBUG_TRACE

#define OAI_UL_LDPC_MAX_NUM_LLR 27000 // 26112 // NR_LDPC_NCOL_BG1*NR_LDPC_ZMAX = 68*384
// #define DEBUG_CRC
#ifdef DEBUG_CRC
#define PRINT_CRC_CHECK(a) a
#else
#define PRINT_CRC_CHECK(a)
#endif

// extern double cpuf;

void free_gNB_ulsch(NR_gNB_ULSCH_t *ulsch, uint16_t N_RB_UL)
{
  uint16_t a_segments = MAX_NUM_NR_ULSCH_SEGMENTS_PER_LAYER * NR_MAX_NB_LAYERS; // number of segments to be allocated

  if (N_RB_UL != 273) {
    a_segments = a_segments * N_RB_UL;
    a_segments = a_segments / 273 + 1;
  }

  if (ulsch->harq_process) {
    if (ulsch->harq_process->b) {
      free_and_zero(ulsch->harq_process->b);
      ulsch->harq_process->b = NULL;
    }
    for (int r = 0; r < a_segments; r++) {
      free_and_zero(ulsch->harq_process->c[r]);
      free_and_zero(ulsch->harq_process->d[r]);
    }
    free_and_zero(ulsch->harq_process->c);
    free_and_zero(ulsch->harq_process->d);
    free_and_zero(ulsch->harq_process->d_to_be_cleared);
    free_and_zero(ulsch->harq_process);
    ulsch->harq_process = NULL;
  }
}

NR_gNB_ULSCH_t new_gNB_ulsch(uint8_t max_ldpc_iterations, uint16_t N_RB_UL)
{
  uint16_t a_segments = MAX_NUM_NR_ULSCH_SEGMENTS_PER_LAYER * NR_MAX_NB_LAYERS; // number of segments to be allocated

  if (N_RB_UL != 273) {
    a_segments = a_segments * N_RB_UL;
    a_segments = a_segments / 273 + 1;
  }

  uint32_t ulsch_bytes = a_segments * 1056; // allocated bytes per segment
  NR_gNB_ULSCH_t ulsch = {0};

  ulsch.max_ldpc_iterations = max_ldpc_iterations;
  ulsch.harq_pid = -1;
  ulsch.active = false;

  NR_UL_gNB_HARQ_t *harq = malloc16_clear(sizeof(*harq));
  init_abort(&harq->abort_decode);
  ulsch.harq_process = harq;
  harq->b = malloc16_clear(ulsch_bytes * sizeof(*harq->b));
  harq->c = malloc16_clear(a_segments * sizeof(*harq->c));
  harq->d = malloc16_clear(a_segments * sizeof(*harq->d));
  for (int r = 0; r < a_segments; r++) {
    harq->c[r] = malloc16_clear(8448 * sizeof(*harq->c[r]));
    harq->d[r] = malloc16_clear(68 * 384 * sizeof(*harq->d[r]));
  }
  harq->d_to_be_cleared = calloc(a_segments, sizeof(bool));
  AssertFatal(harq->d_to_be_cleared != NULL, "out of memory\n");
  return (ulsch);
}

int nr_ulsch_decoding(PHY_VARS_gNB *phy_vars_gNB,
                      NR_DL_FRAME_PARMS *frame_parms,
                      uint32_t frame,
                      uint8_t nr_tti_rx,
                      uint32_t *G,
                      uint8_t *ULSCH_ids,
                      int nb_pusch)
{
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_gNB_ULSCH_DECODING, 1);

  nrLDPC_TB_decoding_parameters_t TBs[nb_pusch];
  memset(TBs, 0, sizeof(TBs));
  nrLDPC_slot_decoding_parameters_t slot_parameters = {.frame = frame,
                                                       .slot = nr_tti_rx,
                                                       .nb_TBs = nb_pusch,
                                                       .threadPool = &phy_vars_gNB->threadPool,
                                                       .TBs = TBs};

  int max_num_segments = 0;

  for (uint8_t pusch_id = 0; pusch_id < nb_pusch; pusch_id++) {
    uint8_t ULSCH_id = ULSCH_ids[pusch_id];
    NR_gNB_ULSCH_t *ulsch = &phy_vars_gNB->ulsch[ULSCH_id];
    NR_gNB_PUSCH *pusch = &phy_vars_gNB->pusch_vars[ULSCH_id];
    NR_UL_gNB_HARQ_t *harq_process = ulsch->harq_process;
    nfapi_nr_pusch_pdu_t *pusch_pdu = &harq_process->ulsch_pdu;

    nrLDPC_TB_decoding_parameters_t *TB_parameters = &TBs[pusch_id];

    TB_parameters->G = G[pusch_id];

    if (!harq_process) {
      LOG_E(PHY, "ulsch_decoding.c: NULL harq_process pointer\n");
      return -1;
    }

    // The harq_pid is not unique among the active HARQ processes in the instance so we use ULSCH_id instead
    TB_parameters->harq_unique_pid = ULSCH_id;

    // ------------------------------------------------------------------
    TB_parameters->nb_rb = pusch_pdu->rb_size;
    TB_parameters->Qm = pusch_pdu->qam_mod_order;
    TB_parameters->mcs = pusch_pdu->mcs_index;
    TB_parameters->nb_layers = pusch_pdu->nrOfLayers;
    // ------------------------------------------------------------------

    TB_parameters->processedSegments = &harq_process->processedSegments;
    harq_process->TBS = pusch_pdu->pusch_data.tb_size;

    TB_parameters->BG = pusch_pdu->maintenance_parms_v3.ldpcBaseGraph;
    TB_parameters->A = (harq_process->TBS) << 3;
    NR_gNB_PHY_STATS_t *stats = get_phy_stats(phy_vars_gNB, ulsch->rnti);
    if (stats) {
      stats->frame = frame;
      stats->ulsch_stats.round_trials[harq_process->round]++;
      for (int aarx = 0; aarx < frame_parms->nb_antennas_rx; aarx++) {
        stats->ulsch_stats.power[aarx] = dB_fixed_x10(pusch->ulsch_power[aarx]);
        stats->ulsch_stats.noise_power[aarx] = dB_fixed_x10(pusch->ulsch_noise_power[aarx]);
      }
      if (!harq_process->harq_to_be_cleared) {
        stats->ulsch_stats.current_Qm = TB_parameters->Qm;
        stats->ulsch_stats.current_RI = TB_parameters->nb_layers;
        stats->ulsch_stats.total_bytes_tx += harq_process->TBS;
      }
    }

    uint8_t harq_pid = ulsch->harq_pid;
    LOG_D(PHY,
          "ULSCH Decoding, harq_pid %d rnti %x TBS %d G %d mcs %d Nl %d nb_rb %d, Qm %d, Coderate %f RV %d round %d new RX %d\n",
          harq_pid,
          ulsch->rnti,
          TB_parameters->A,
          TB_parameters->G,
          TB_parameters->mcs,
          TB_parameters->nb_layers,
          TB_parameters->nb_rb,
          TB_parameters->Qm,
          pusch_pdu->target_code_rate / 10240.0f,
          pusch_pdu->pusch_data.rv_index,
          harq_process->round,
          harq_process->harq_to_be_cleared);

    // [hna] Perform nr_segmenation with input and output set to NULL to calculate only (C, K, Z, F)
    nr_segmentation(NULL,
                    NULL,
                    lenWithCrc(1, TB_parameters->A), // size in case of 1 segment
                    &TB_parameters->C,
                    &TB_parameters->K,
                    &TB_parameters->Z, // [hna] Z is Zc
                    &TB_parameters->F,
                    TB_parameters->BG);
    harq_process->C = TB_parameters->C;
    harq_process->K = TB_parameters->K;
    harq_process->Z = TB_parameters->Z;
    harq_process->F = TB_parameters->F;

    uint16_t a_segments = MAX_NUM_NR_ULSCH_SEGMENTS_PER_LAYER * TB_parameters->nb_layers; // number of segments to be allocated
    if (TB_parameters->C > a_segments) {
      LOG_E(PHY, "nr_segmentation.c: too many segments %d, A %d\n", harq_process->C, TB_parameters->A);
      return (-1);
    }
    if (TB_parameters->nb_rb != 273) {
      a_segments = a_segments * TB_parameters->nb_rb;
      a_segments = a_segments / 273 + 1;
    }
    if (TB_parameters->C > a_segments) {
      LOG_E(PHY, "Illegal harq_process->C %d > %d\n", harq_process->C, a_segments);
      return -1;
    }
    max_num_segments = max(max_num_segments, TB_parameters->C);

#ifdef DEBUG_ULSCH_DECODING
    printf("ulsch decoding nr segmentation Z %d\n", TB_parameters->Z);
    if (!frame % 100)
      printf("K %d C %d Z %d \n", TB_parameters->K, TB_parameters->C, TB_parameters->Z);
    printf("Segmentation: C %d, K %d\n", TB_parameters->C, TB_parameters->K);
#endif

    TB_parameters->max_ldpc_iterations = ulsch->max_ldpc_iterations;
    TB_parameters->rv_index = pusch_pdu->pusch_data.rv_index;
    TB_parameters->tbslbrm = pusch_pdu->maintenance_parms_v3.tbSizeLbrmBytes;
    TB_parameters->abort_decode = &harq_process->abort_decode;
    set_abort(&harq_process->abort_decode, false);
  }

  nrLDPC_segment_decoding_parameters_t segments[nb_pusch][max_num_segments];
  memset(segments, 0, sizeof(segments));

  for (uint8_t pusch_id = 0; pusch_id < nb_pusch; pusch_id++) {
    uint8_t ULSCH_id = ULSCH_ids[pusch_id];
    NR_gNB_ULSCH_t *ulsch = &phy_vars_gNB->ulsch[ULSCH_id];
    NR_UL_gNB_HARQ_t *harq_process = ulsch->harq_process;
    short *ulsch_llr = phy_vars_gNB->pusch_vars[ULSCH_id].llr;

    if (!ulsch_llr) {
      LOG_E(PHY, "ulsch_decoding.c: NULL ulsch_llr pointer\n");
      return -1;
    }

    nrLDPC_TB_decoding_parameters_t *TB_parameters = &TBs[pusch_id];
    TB_parameters->segments = segments[pusch_id];

    uint32_t r_offset = 0;
    for (int r = 0; r < TB_parameters->C; r++) {
      nrLDPC_segment_decoding_parameters_t *segment_parameters = &TB_parameters->segments[r];
      segment_parameters->E = nr_get_E(TB_parameters->G, TB_parameters->C, TB_parameters->Qm, TB_parameters->nb_layers, r);
      segment_parameters->R = nr_get_R_ldpc_decoder(TB_parameters->rv_index,
                                                    segment_parameters->E,
                                                    TB_parameters->BG,
                                                    TB_parameters->Z,
                                                    &harq_process->llrLen,
                                                    harq_process->round);
      segment_parameters->llr = ulsch_llr + r_offset;
      segment_parameters->d = harq_process->d[r];
      segment_parameters->d_to_be_cleared = &harq_process->d_to_be_cleared[r];
      segment_parameters->c = harq_process->c[r];
      segment_parameters->decodeSuccess = false;

      reset_meas(&segment_parameters->ts_deinterleave);
      reset_meas(&segment_parameters->ts_rate_unmatch);
      reset_meas(&segment_parameters->ts_ldpc_decode);

      r_offset += segment_parameters->E;
    }
    if (harq_process->harq_to_be_cleared) {
      for (int r = 0; r < TB_parameters->C; r++) {
        harq_process->d_to_be_cleared[r] = true;
      }
      harq_process->harq_to_be_cleared = false;
    }
  }

  int ret_decoder = phy_vars_gNB->nrLDPC_coding_interface.nrLDPC_coding_decoder(&slot_parameters);

  // post decode
  for (uint8_t pusch_id = 0; pusch_id < nb_pusch; pusch_id++) {
    uint8_t ULSCH_id = ULSCH_ids[pusch_id];
    NR_gNB_ULSCH_t *ulsch = &phy_vars_gNB->ulsch[ULSCH_id];
    NR_UL_gNB_HARQ_t *harq_process = ulsch->harq_process;

    nrLDPC_TB_decoding_parameters_t TB_parameters = TBs[pusch_id];

    uint32_t offset = 0;
    for (int r = 0; r < TB_parameters.C; r++) {
      nrLDPC_segment_decoding_parameters_t nrLDPC_segment_decoding_parameters = TB_parameters.segments[r];
      // Copy c to b in case of decoding success
      if (nrLDPC_segment_decoding_parameters.decodeSuccess) {
        memcpy(harq_process->b + offset,
               harq_process->c[r],
               (harq_process->K >> 3) - (harq_process->F >> 3) - ((harq_process->C > 1) ? 3 : 0));
      } else {
        LOG_D(PHY, "uplink segment error %d/%d\n", r, harq_process->C);
        LOG_D(PHY, "ULSCH %d in error\n", ULSCH_id);
      }
      offset += ((harq_process->K >> 3) - (harq_process->F >> 3) - ((harq_process->C > 1) ? 3 : 0));

      merge_meas(&phy_vars_gNB->ts_deinterleave, &nrLDPC_segment_decoding_parameters.ts_deinterleave);
      merge_meas(&phy_vars_gNB->ts_rate_unmatch, &nrLDPC_segment_decoding_parameters.ts_rate_unmatch);
      merge_meas(&phy_vars_gNB->ts_ldpc_decode, &nrLDPC_segment_decoding_parameters.ts_ldpc_decode);
    }
  }

  return ret_decoder;
}
