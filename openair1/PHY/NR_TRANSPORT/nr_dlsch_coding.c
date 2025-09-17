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

/*! \file PHY/NR_TRANSPORT/nr_dlsch_coding_slot.c
 * \brief Top-level routines for implementing LDPC-coded (DLSCH) transport channels from 38-212, 15.2
 */

#include "PHY/defs_gNB.h"
#include "PHY/CODING/coding_extern.h"
#include "PHY/CODING/coding_defs.h"
#include "PHY/CODING/lte_interleaver_inline.h"
#include "PHY/CODING/nrLDPC_coding/nrLDPC_coding_interface.h"
#include "PHY/CODING/nrLDPC_extern.h"
#include "PHY/NR_TRANSPORT/nr_transport_proto.h"
#include "PHY/NR_TRANSPORT/nr_transport_common_proto.h"
#include "PHY/NR_TRANSPORT/nr_dlsch.h"
#include "SCHED_NR/sched_nr.h"
#include "common/utils/LOG/vcd_signal_dumper.h"
#include "common/utils/LOG/log.h"
#include "common/utils/nr/nr_common.h"
#include <syscall.h>
#include <openair2/UTIL/OPT/opt.h>

// #define DEBUG_DLSCH_CODING
// #define DEBUG_DLSCH_FREE 1

void free_gNB_dlsch(NR_gNB_DLSCH_t *dlsch, uint16_t N_RB, const NR_DL_FRAME_PARMS *frame_parms)
{
  int max_layers = (frame_parms->nb_antennas_tx < NR_MAX_NB_LAYERS) ? frame_parms->nb_antennas_tx : NR_MAX_NB_LAYERS;
  uint16_t a_segments = MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER * max_layers;

  if (N_RB != 273) {
    a_segments = a_segments * N_RB;
    a_segments = a_segments / 273 + 1;
  }

  NR_DL_gNB_HARQ_t *harq = &dlsch->harq_process;
  if (harq->b) {
    free16(harq->b, a_segments * 1056);
    harq->b = NULL;
  }
  if (harq->f) {
    free16(harq->f, N_RB * NR_SYMBOLS_PER_SLOT * NR_NB_SC_PER_RB * 8 * NR_MAX_NB_LAYERS);
    harq->f = NULL;
  }
  for (int r = 0; r < a_segments; r++) {
    free(harq->c[r]);
    harq->c[r] = NULL;
  }
  free(harq->c);
}

NR_gNB_DLSCH_t new_gNB_dlsch(NR_DL_FRAME_PARMS *frame_parms, uint16_t N_RB)
{
  int max_layers = (frame_parms->nb_antennas_tx < NR_MAX_NB_LAYERS) ? frame_parms->nb_antennas_tx : NR_MAX_NB_LAYERS;
  uint16_t a_segments = MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER * max_layers; // number of segments to be allocated

  if (N_RB != 273) {
    a_segments = a_segments * N_RB;
    a_segments = a_segments / 273 + 1;
  }

  LOG_D(PHY, "Allocating %d segments (MAX %d, N_PRB %d)\n", a_segments, MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER, N_RB);
  uint32_t dlsch_bytes = a_segments * 1056; // allocated bytes per segment
  NR_gNB_DLSCH_t dlsch;

  NR_DL_gNB_HARQ_t *harq = &dlsch.harq_process;
  bzero(harq, sizeof(NR_DL_gNB_HARQ_t));
  harq->b = malloc16(dlsch_bytes);
  AssertFatal(harq->b, "cannot allocate memory for harq->b\n");
  bzero(harq->b, dlsch_bytes);

  harq->c = (uint8_t **)malloc16(a_segments * sizeof(uint8_t *));
  for (int r = 0; r < a_segments; r++) {
    // account for filler in first segment and CRCs for multiple segment case
    // [hna] 8448 is the maximum CB size in NR
    //       68*348 = 68*(maximum size of Zc)
    //       In section 5.3.2 in 38.212, the for loop is up to N + 2*Zc (maximum size of N is 66*Zc, therefore 68*Zc)
    harq->c[r] = malloc16(8448);
    AssertFatal(harq->c[r], "cannot allocate harq->c[%d]\n", r);
    bzero(harq->c[r], 8448);
  }

  harq->f = malloc16(N_RB * NR_SYMBOLS_PER_SLOT * NR_NB_SC_PER_RB * 8 * NR_MAX_NB_LAYERS);
  AssertFatal(harq->f, "cannot allocate harq->f\n");
  bzero(harq->f, N_RB * NR_SYMBOLS_PER_SLOT * NR_NB_SC_PER_RB * 8 * NR_MAX_NB_LAYERS);

  return (dlsch);
}

int nr_dlsch_encoding(PHY_VARS_gNB *gNB,
                      processingData_L1tx_t *msgTx,
                      int frame,
                      uint8_t slot,
                      NR_DL_FRAME_PARMS *frame_parms,
                      unsigned char *output,
                      time_stats_t *tinput,
                      time_stats_t *tprep,
                      time_stats_t *tparity,
                      time_stats_t *toutput,
                      time_stats_t *dlsch_rate_matching_stats,
                      time_stats_t *dlsch_interleaving_stats,
                      time_stats_t *dlsch_segmentation_stats)
{
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_gNB_DLSCH_ENCODING, VCD_FUNCTION_IN);

  nrLDPC_TB_encoding_parameters_t TBs[msgTx->num_pdsch_slot];
  memset(TBs, 0, sizeof(TBs));
  nrLDPC_slot_encoding_parameters_t slot_parameters = {.frame = frame,
                                                       .slot = slot,
                                                       .nb_TBs = msgTx->num_pdsch_slot,
                                                       .threadPool = &gNB->threadPool,
                                                       .tinput = tinput,
                                                       .tprep = tprep,
                                                       .tparity = tparity,
                                                       .toutput = toutput,
                                                       .TBs = TBs};

  int num_segments = 0;

  for (int dlsch_id = 0; dlsch_id < msgTx->num_pdsch_slot; dlsch_id++) {
    NR_gNB_DLSCH_t *dlsch = msgTx->dlsch[dlsch_id];

    NR_DL_gNB_HARQ_t *harq = &dlsch->harq_process;
    unsigned int crc = 1;
    nfapi_nr_dl_tti_pdsch_pdu_rel15_t *rel15 = &harq->pdsch_pdu.pdsch_pdu_rel15;
    uint32_t A = rel15->TBSize[0] << 3;
    unsigned char *a = harq->pdu;
    if (rel15->rnti != SI_RNTI) {
      ws_trace_t tmp = {.nr = true,
                        .direction = DIRECTION_DOWNLINK,
                        .pdu_buffer = a,
                        .pdu_buffer_size = rel15->TBSize[0],
                        .ueid = 0,
                        .rntiType = WS_C_RNTI,
                        .rnti = rel15->rnti,
                        .sysFrame = frame,
                        .subframe = slot,
                        .harq_pid = 0, // difficult to find the harq pid here
                        .oob_event = 0,
                        .oob_event_value = 0};
      trace_pdu(&tmp);
    }

    NR_gNB_PHY_STATS_t *phy_stats = NULL;
    if (rel15->rnti != 0xFFFF)
      phy_stats = get_phy_stats(gNB, rel15->rnti);

    if (phy_stats) {
      phy_stats->frame = frame;
      phy_stats->dlsch_stats.total_bytes_tx += rel15->TBSize[0];
      phy_stats->dlsch_stats.current_RI = rel15->nrOfLayers;
      phy_stats->dlsch_stats.current_Qm = rel15->qamModOrder[0];
    }

    int max_bytes = MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER * rel15->nrOfLayers * 1056;
    int B;
    if (A > NR_MAX_PDSCH_TBS) {
      // Add 24-bit crc (polynomial A) to payload
      crc = crc24a(a, A) >> 8;
      a[A >> 3] = ((uint8_t *)&crc)[2];
      a[1 + (A >> 3)] = ((uint8_t *)&crc)[1];
      a[2 + (A >> 3)] = ((uint8_t *)&crc)[0];
      // printf("CRC %x (A %d)\n",crc,A);
      // printf("a0 %d a1 %d a2 %d\n", a[A>>3], a[1+(A>>3)], a[2+(A>>3)]);
      B = A + 24;
      //    harq->b = a;
      AssertFatal((A / 8) + 4 <= max_bytes, "A %d is too big (A/8+4 = %d > %d)\n", A, (A / 8) + 4, max_bytes);
      memcpy(harq->b, a, (A / 8) + 4); // why is this +4 if the CRC is only 3 bytes?
    } else {
      // Add 16-bit crc (polynomial A) to payload
      crc = crc16(a, A) >> 16;
      a[A >> 3] = ((uint8_t *)&crc)[1];
      a[1 + (A >> 3)] = ((uint8_t *)&crc)[0];
      // printf("CRC %x (A %d)\n",crc,A);
      // printf("a0 %d a1 %d \n", a[A>>3], a[1+(A>>3)]);
      B = A + 16;
      //    harq->b = a;
      AssertFatal((A / 8) + 3 <= max_bytes, "A %d is too big (A/8+3 = %d > %d)\n", A, (A / 8) + 3, max_bytes);
      memcpy(harq->b, a, (A / 8) + 3); // using 3 bytes to mimic the case of 24 bit crc
    }

    nrLDPC_TB_encoding_parameters_t *TB_parameters = &TBs[dlsch_id];

    // The harq_pid is not unique among the active HARQ processes in the instance so we use dlsch_id instead
    TB_parameters->harq_unique_pid = dlsch_id;
    TB_parameters->BG = rel15->maintenance_parms_v3.ldpcBaseGraph;
    TB_parameters->Z = harq->Z;
    TB_parameters->A = A;
    start_meas(dlsch_segmentation_stats);
    TB_parameters->Kb = nr_segmentation(harq->b,
                                        harq->c,
                                        B,
                                        &TB_parameters->C,
                                        &TB_parameters->K,
                                        &TB_parameters->Z,
                                        &TB_parameters->F,
                                        TB_parameters->BG);
    stop_meas(dlsch_segmentation_stats);

    if (TB_parameters->C > MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER * rel15->nrOfLayers) {
      LOG_E(PHY, "nr_segmentation.c: too many segments %d, B %d\n", TB_parameters->C, B);
      return (-1);
    }
    num_segments += TB_parameters->C;
  }

  nrLDPC_segment_encoding_parameters_t segments[num_segments];
  memset(segments, 0, sizeof(segments));
  size_t segments_offset = 0;
  size_t dlsch_offset = 0;

  for (int dlsch_id = 0; dlsch_id < msgTx->num_pdsch_slot; dlsch_id++) {
    NR_gNB_DLSCH_t *dlsch = msgTx->dlsch[dlsch_id];
    NR_DL_gNB_HARQ_t *harq = &dlsch->harq_process;
    nfapi_nr_dl_tti_pdsch_pdu_rel15_t *rel15 = &harq->pdsch_pdu.pdsch_pdu_rel15;

    nrLDPC_TB_encoding_parameters_t *TB_parameters = &TBs[dlsch_id];

#ifdef DEBUG_DLSCH_CODING
    for (int r = 0; r < TB_parameters->C; r++) {
      LOG_D(PHY, "Encoder: B %d F %d \n", harq->B, TB_parameters->F);
      LOG_D(PHY, "start ldpc encoder segment %d/%d\n", r, TB_parameters->C);
      LOG_D(PHY, "input %d %d %d %d %d \n", harq->c[r][0], harq->c[r][1], harq->c[r][2], harq->c[r][3], harq->c[r][4]);
      for (int cnt = 0; cnt < 22 * (TB_parameters->Z) / 8; cnt++) {
        LOG_D(PHY, "%d ", harq->c[r][cnt]);
      }
      LOG_D(PHY, "\n");
    }
#endif

    TB_parameters->nb_rb = rel15->rbSize;
    TB_parameters->Qm = rel15->qamModOrder[0];
    TB_parameters->mcs = rel15->mcsIndex[0];
    TB_parameters->nb_layers = rel15->nrOfLayers;
    TB_parameters->rv_index = rel15->rvIndex[0];

    int nb_re_dmrs =
        (rel15->dmrsConfigType == NFAPI_NR_DMRS_TYPE1) ? (6 * rel15->numDmrsCdmGrpsNoData) : (4 * rel15->numDmrsCdmGrpsNoData);
    TB_parameters->G = nr_get_G(rel15->rbSize,
                                rel15->NrOfSymbols,
                                nb_re_dmrs,
                                get_num_dmrs(rel15->dlDmrsSymbPos),
                                harq->unav_res,
                                rel15->qamModOrder[0],
                                rel15->nrOfLayers);

    TB_parameters->tbslbrm = rel15->maintenance_parms_v3.tbSizeLbrmBytes;

    TB_parameters->output = &output[dlsch_offset >> 3];
    TB_parameters->segments = &segments[segments_offset];

    for (int r = 0; r < TB_parameters->C; r++) {
      nrLDPC_segment_encoding_parameters_t *segment_parameters = &TB_parameters->segments[r];
      segment_parameters->c = harq->c[r];
      segment_parameters->E = nr_get_E(TB_parameters->G, TB_parameters->C, TB_parameters->Qm, rel15->nrOfLayers, r);

      reset_meas(&segment_parameters->ts_interleave);
      reset_meas(&segment_parameters->ts_rate_match);
      reset_meas(&segment_parameters->ts_ldpc_encode);
    }

    segments_offset += TB_parameters->C;

    /* output and its parts for each dlsch should be aligned on 64 bytes (or 8 * 64 bits)
     * => dlsch_offset should remain a multiple of 8 * 64 with enough offset to fit each dlsch
     */
    const size_t dlsch_size = rel15->rbSize * NR_SYMBOLS_PER_SLOT * NR_NB_SC_PER_RB * rel15->qamModOrder[0] * rel15->nrOfLayers;
    dlsch_offset += ceil_mod(dlsch_size, 8 * 64);
  }

  gNB->nrLDPC_coding_interface.nrLDPC_coding_encoder(&slot_parameters);

  for (int dlsch_id = 0; dlsch_id < msgTx->num_pdsch_slot; dlsch_id++) {
    nrLDPC_TB_encoding_parameters_t *TB_parameters = &TBs[dlsch_id];
    for (int r = 0; r < TB_parameters->C; r++) {
      nrLDPC_segment_encoding_parameters_t *segment_parameters = &TB_parameters->segments[r];
      merge_meas(dlsch_interleaving_stats, &segment_parameters->ts_interleave);
      merge_meas(dlsch_rate_matching_stats, &segment_parameters->ts_rate_match);
      // merge_meas(, &segment_parameters->ts_ldpc_encode);
    }
  }

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_gNB_DLSCH_ENCODING, VCD_FUNCTION_OUT);
  return 0;
}
