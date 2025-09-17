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

#ifndef __NR_UE_PHY_MEAS__H__
#define __NR_UE_PHY_MEAS__H__
#include "time_meas.h"
#include "utils.h"

#define NOOP(a) a
#define FOREACH_NR_PHY_CPU_MEAS(FN) \
  FN(RX_PDSCH_STATS),\
  FN(DLSCH_RX_PDCCH_STATS),\
  FN(RX_DFT_STATS),\
  FN(DLSCH_CHANNEL_ESTIMATION_STATS),\
  FN(DLSCH_DECODING_STATS),\
  FN(DLSCH_RATE_UNMATCHING_STATS),\
  FN(DLSCH_LDPC_DECODING_STATS),\
  FN(DLSCH_DEINTERLEAVING_STATS),\
  FN(DLSCH_EXTRACT_RBS_STATS),\
  FN(DLSCH_CHANNEL_SCALE_STATS),\
  FN(DLSCH_CHANNEL_LEVEL_STATS),\
  FN(DLSCH_MRC_MMSE_STATS),\
  FN(DLSCH_UNSCRAMBLING_STATS),\
  FN(DLSCH_CHANNEL_COMPENSATION_STATS),\
  FN(DLSCH_LLR_STATS),\
  FN(DLSCH_LAYER_DEMAPPING),\
  FN(PHY_RX_PDCCH_STATS),\
  FN(DLSCH_PROCEDURES_STATS),\
  FN(PHY_PROC_TX),\
  FN(PUSCH_PROC_STATS),\
  FN(ULSCH_SEGMENTATION_STATS),\
  FN(ULSCH_LDPC_ENCODING_STATS),\
  FN(ULSCH_RATE_MATCHING_STATS),\
  FN(ULSCH_INTERLEAVING_STATS),\
  FN(ULSCH_ENCODING_STATS),\
  FN(OFDM_MOD_STATS)

typedef enum {
  FOREACH_NR_PHY_CPU_MEAS(NOOP),
  MAX_CPU_STAT_TYPE
} nr_ue_phy_cpu_stat_type_t;

typedef struct nr_ue_phy_cpu_stat_t {
  time_stats_t cpu_time_stats[MAX_CPU_STAT_TYPE];
} nr_ue_phy_cpu_stat_t;

void init_nr_ue_phy_cpu_stats(nr_ue_phy_cpu_stat_t *ue_phy_cpu_stats);
void reset_nr_ue_phy_cpu_stats(nr_ue_phy_cpu_stat_t *ue_phy_cpu_stats);

#endif
