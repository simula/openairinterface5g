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

/*! \file gNB_scheduler_bch.c
 * \brief procedures related to gNB for the BCH transport channel
 * \author  Navid Nikaein and Raymond Knopp, WEI-TAI CHEN
 * \date 2010 - 2014, 2018
 * \email: navid.nikaein@eurecom.fr, kroempa@gmail.com
 * \version 1.0
 * \company Eurecom, NTUST
 * @ingroup _mac

 */

#include "assertions.h"
#include "NR_MAC_gNB/nr_mac_gNB.h"
#include "NR_MAC_gNB/mac_proto.h"
#include "NR_MAC_COMMON/nr_mac_extern.h"
#include "common/utils/LOG/log.h"
#include "common/utils/LOG/vcd_signal_dumper.h"
#include "UTIL/OPT/opt.h"
#include "common/utils/nr/nr_common.h"

#define ENABLE_MAC_PAYLOAD_DEBUG

#include "common/ran_context.h"

#include "executables/softmodem-common.h"

extern RAN_CONTEXT_t RC;

static void schedule_ssb(frame_t frame,
                         sub_frame_t slot,
                         NR_ServingCellConfigCommon_t *scc,
                         nfapi_nr_dl_tti_request_body_t *dl_req,
                         int i_ssb,
                         int beam_index,
                         uint8_t scoffset,
                         uint16_t offset_pointa,
                         uint32_t payload)
{
  nfapi_nr_dl_tti_request_pdu_t *dl_config_pdu = &dl_req->dl_tti_pdu_list[dl_req->nPDUs];
  memset((void *) dl_config_pdu, 0, sizeof(nfapi_nr_dl_tti_request_pdu_t));
  dl_config_pdu->PDUType = NFAPI_NR_DL_TTI_SSB_PDU_TYPE;
  dl_config_pdu->PDUSize = 2 + sizeof(nfapi_nr_dl_tti_ssb_pdu_rel15_t);

  AssertFatal(scc->physCellId != NULL,"ServingCellConfigCommon->physCellId is null\n");
  AssertFatal(*scc->physCellId < 1008 && *scc->physCellId >=0, "5G physicall cell id out of range: %ld\n", *scc->physCellId);
  dl_config_pdu->ssb_pdu.ssb_pdu_rel15.PhysCellId = *scc->physCellId;
  dl_config_pdu->ssb_pdu.ssb_pdu_rel15.BetaPss = 0;
  dl_config_pdu->ssb_pdu.ssb_pdu_rel15.SsbBlockIndex = i_ssb;
  AssertFatal(scc->downlinkConfigCommon,"scc->downlinkConfigCommonL is null\n");
  AssertFatal(scc->downlinkConfigCommon->frequencyInfoDL,"scc->downlinkConfigCommon->frequencyInfoDL is null\n");
  AssertFatal(scc->downlinkConfigCommon->frequencyInfoDL->absoluteFrequencySSB,
              "scc->downlinkConfigCommon->frequencyInfoDL->absoluteFrequencySSB is null\n");
  AssertFatal(scc->downlinkConfigCommon->frequencyInfoDL->frequencyBandList.list.count == 1,
              "Frequency Band list does not have 1 element (%d)\n",
              scc->downlinkConfigCommon->frequencyInfoDL->frequencyBandList.list.count);
  AssertFatal(scc->ssbSubcarrierSpacing,"ssbSubcarrierSpacing is null\n");
  AssertFatal(scc->downlinkConfigCommon->frequencyInfoDL->frequencyBandList.list.array[0], "band is null\n");

  dl_config_pdu->ssb_pdu.ssb_pdu_rel15.SsbSubcarrierOffset = scoffset; //kSSB
  dl_config_pdu->ssb_pdu.ssb_pdu_rel15.ssbOffsetPointA = offset_pointa;
  dl_config_pdu->ssb_pdu.ssb_pdu_rel15.bchPayloadFlag = 1;
  dl_config_pdu->ssb_pdu.ssb_pdu_rel15.bchPayload = payload;
  dl_config_pdu->ssb_pdu.ssb_pdu_rel15.precoding_and_beamforming.num_prgs = 1;
  dl_config_pdu->ssb_pdu.ssb_pdu_rel15.precoding_and_beamforming.prg_size = 275; //1 PRG of max size for analogue beamforming
  dl_config_pdu->ssb_pdu.ssb_pdu_rel15.precoding_and_beamforming.dig_bf_interfaces = 1;
  dl_config_pdu->ssb_pdu.ssb_pdu_rel15.precoding_and_beamforming.prgs_list[0].pm_idx = 0;
  dl_config_pdu->ssb_pdu.ssb_pdu_rel15.precoding_and_beamforming.prgs_list[0].dig_bf_interface_list[0].beam_idx = beam_index;
  dl_req->nPDUs++;

  LOG_D(MAC,"Scheduling ssb %d at frame %d and slot %d\n", i_ssb, frame, slot);
}

static void fill_ssb_vrb_map(NR_COMMON_channels_t *cc,
                             int rbStart,
                             int ssb_subcarrier_offset,
                             uint16_t symStart,
                             int CC_id,
                             int beam)
{
  AssertFatal(*cc->ServingCellConfigCommon->ssbSubcarrierSpacing !=
              NR_SubcarrierSpacing_kHz240,
              "240kHZ subcarrier won't work with current VRB map because a single SSB might be across 2 slots\n");

  uint16_t *vrb_map = cc[CC_id].vrb_map[beam];
  const int extra_prb = ssb_subcarrier_offset > 0;
  for (int rb = 0; rb < 20 + extra_prb; rb++)
    vrb_map[rbStart + rb] = SL_to_bitmap(symStart % NR_NUMBER_OF_SYMBOLS_PER_SLOT, 4);
}

static int encode_mib(NR_BCCH_BCH_Message_t *mib, frame_t frame, uint8_t *buffer, int buf_size)
{
  int encode_size = 3;
  AssertFatal(buf_size >= encode_size, "buffer of size %d too small, need 3 bytes\n", buf_size);
  int encoded = encode_MIB_NR(mib, frame, buffer, encode_size);
  DevAssert(encoded == encode_size);
  return encode_size;
}

void schedule_nr_mib(module_id_t module_idP, frame_t frameP, sub_frame_t slotP, nfapi_nr_dl_tti_request_t *DL_req)
{
  gNB_MAC_INST *gNB = RC.nrmac[module_idP];
  /* already mutex protected: held in gNB_dlsch_ulsch_scheduler() */
  nfapi_nr_dl_tti_request_body_t *dl_req;

  for (int CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {
    NR_COMMON_channels_t *cc= &gNB->common_channels[CC_id];
    const NR_MIB_t *mib = cc->mib->message.choice.mib;
    NR_ServingCellConfigCommon_t *scc = cc->ServingCellConfigCommon;
    const int slots_per_frame = nr_slots_per_frame[*scc->ssbSubcarrierSpacing];
    dl_req = &DL_req->dl_tti_request_body;

    // get MIB every 8 frames
    if(((slotP == 0) && (frameP & 7) == 0) ||
       gNB->first_MIB) {
      int mib_sdu_length = encode_mib(cc->mib, frameP, cc->MIB_pdu, sizeof(cc->MIB_pdu));

      // flag to avoid sending an empty MIB in the first frames of execution since gNB doesn't get at the beginning in frame 0 slot 0
      gNB->first_MIB = false;

      LOG_D(MAC,
            "[gNB %d] Frame %d : MIB->BCH  CC_id %d, Received %d bytes\n",
            module_idP,
            frameP,
            CC_id,
            mib_sdu_length);
    }

    uint32_t mib_pdu = (*(uint32_t *)cc->MIB_pdu) & ((1 << 24) - 1);

    int8_t ssb_period = *scc->ssb_periodicityServingCell;
    uint8_t ssb_frame_periodicity = 1;  // every how many frames SSB are generated

    if (ssb_period > 1) // 0 is every half frame
      ssb_frame_periodicity = 1 << (ssb_period -1);

    if (!(frameP % ssb_frame_periodicity) && ((slotP < (slots_per_frame >> 1)) || (ssb_period == 0))) {
      // schedule SSB only for given frames according to SSB periodicity
      // and in first half frame unless periodicity of 5ms
      int rel_slot;
      if (ssb_period == 0) // scheduling every half frame
        rel_slot = slotP % (slots_per_frame >> 1);
      else
        rel_slot = slotP;

      NR_SubcarrierSpacing_t scs = *scc->ssbSubcarrierSpacing;
      const long band = *scc->downlinkConfigCommon->frequencyInfoDL->frequencyBandList.list.array[0];
      const uint16_t offset_pointa = gNB->ssb_OffsetPointA;
      uint8_t ssbSubcarrierOffset = gNB->ssb_SubcarrierOffset;

      const BIT_STRING_t *shortBitmap = &scc->ssb_PositionsInBurst->choice.shortBitmap;
      const BIT_STRING_t *mediumBitmap = &scc->ssb_PositionsInBurst->choice.mediumBitmap;
      const BIT_STRING_t *longBitmap = &scc->ssb_PositionsInBurst->choice.longBitmap;

      const int n_slots_frame = nr_slots_per_frame[scs];
      switch (scc->ssb_PositionsInBurst->present) {
        case 1:
          // short bitmap (<3GHz) max 4 SSBs
          for (int i_ssb = 0; i_ssb < 4; i_ssb++) {
            if ((shortBitmap->buf[0] >> (7 - i_ssb)) & 0x01) {
              uint16_t ssb_start_symbol = get_ssb_start_symbol(band, scs, i_ssb);
              // if start symbol is in current slot, schedule current SSB, fill VRB map and call get_type0_PDCCH_CSS_config_parameters
              if ((ssb_start_symbol / 14) == rel_slot) {
                int beam_index = get_fapi_beamforming_index(gNB, i_ssb);
                NR_beam_alloc_t beam = beam_allocation_procedure(&gNB->beam_info, frameP, slotP, beam_index, n_slots_frame);
                AssertFatal(beam.idx >= 0, "Cannot allocate SSB %d in any available beam\n", i_ssb);
                const int prb_offset = offset_pointa >> scs;
                schedule_ssb(frameP, slotP, scc, dl_req, i_ssb, beam_index, ssbSubcarrierOffset, offset_pointa, mib_pdu);
                fill_ssb_vrb_map(cc, prb_offset, ssbSubcarrierOffset, ssb_start_symbol, CC_id, beam.idx);
                if (get_softmodem_params()->sa == 1) {
                  get_type0_PDCCH_CSS_config_parameters(&gNB->type0_PDCCH_CSS_config[i_ssb],
                                                        frameP,
                                                        mib,
                                                        slots_per_frame,
                                                        ssbSubcarrierOffset,
                                                        ssb_start_symbol,
                                                        scs,
                                                        FR1,
                                                        band,
                                                        i_ssb,
                                                        ssb_frame_periodicity,
                                                        prb_offset);
                  gNB->type0_PDCCH_CSS_config[i_ssb].active = true;
                }
              }
            }
          }
          break;
        case 2:
          // medium bitmap (<6GHz) max 8 SSBs
          for (int i_ssb = 0; i_ssb < 8; i_ssb++) {
            if ((mediumBitmap->buf[0] >> (7 - i_ssb)) & 0x01) {
              uint16_t ssb_start_symbol = get_ssb_start_symbol(band, scs, i_ssb);
              // if start symbol is in current slot, schedule current SSB, fill VRB map and call get_type0_PDCCH_CSS_config_parameters
              if ((ssb_start_symbol / 14) == rel_slot) {
                int beam_index = get_fapi_beamforming_index(gNB, i_ssb);
                NR_beam_alloc_t beam = beam_allocation_procedure(&gNB->beam_info, frameP, slotP, beam_index, n_slots_frame);
                AssertFatal(beam.idx >= 0, "Cannot allocate SSB %d in any available beam\n", i_ssb);
                const int prb_offset = offset_pointa >> scs;
                schedule_ssb(frameP, slotP, scc, dl_req, i_ssb, beam_index, ssbSubcarrierOffset, offset_pointa, mib_pdu);
                fill_ssb_vrb_map(cc, prb_offset, ssbSubcarrierOffset, ssb_start_symbol, CC_id, beam.idx);
                if (get_softmodem_params()->sa == 1) {
                  get_type0_PDCCH_CSS_config_parameters(&gNB->type0_PDCCH_CSS_config[i_ssb],
                                                        frameP,
                                                        mib,
                                                        slots_per_frame,
                                                        ssbSubcarrierOffset,
                                                        ssb_start_symbol,
                                                        scs,
                                                        FR1,
                                                        band,
                                                        i_ssb,
                                                        ssb_frame_periodicity,
                                                        prb_offset);
                  gNB->type0_PDCCH_CSS_config[i_ssb].active = true;
                }
              }
            }
          }
          break;
        case 3:
          // long bitmap FR2 max 64 SSBs
          for (int i_ssb = 0; i_ssb < 64; i_ssb++) {
            if ((longBitmap->buf[i_ssb / 8] >> (7 - (i_ssb % 8))) & 0x01) {
              uint16_t ssb_start_symbol = get_ssb_start_symbol(band, scs, i_ssb);
              // if start symbol is in current slot, schedule current SSB, fill VRB map and call get_type0_PDCCH_CSS_config_parameters
              if ((ssb_start_symbol / 14) == rel_slot) {
                int beam_index = get_fapi_beamforming_index(gNB, i_ssb);
                NR_beam_alloc_t beam = beam_allocation_procedure(&gNB->beam_info, frameP, slotP, beam_index, n_slots_frame);
                AssertFatal(beam.idx >= 0, "Cannot allocate SSB %d in any available beam\n", i_ssb);
                const int prb_offset = offset_pointa >> (scs-2); // reference 60kHz
                schedule_ssb(frameP, slotP, scc, dl_req, i_ssb, beam_index, ssbSubcarrierOffset, offset_pointa, mib_pdu);
                fill_ssb_vrb_map(cc, prb_offset, ssbSubcarrierOffset >> (scs - 2), ssb_start_symbol, CC_id, beam.idx);
                if (get_softmodem_params()->sa == 1) {
                  get_type0_PDCCH_CSS_config_parameters(&gNB->type0_PDCCH_CSS_config[i_ssb],
                                                        frameP,
                                                        mib,
                                                        slots_per_frame,
                                                        ssbSubcarrierOffset,
                                                        ssb_start_symbol,
                                                        scs,
                                                        FR2,
                                                        band,
                                                        i_ssb,
                                                        ssb_frame_periodicity,
                                                        prb_offset);
                  gNB->type0_PDCCH_CSS_config[i_ssb].active = true;
                }
              }
            }
          }
          break;
        default:
          AssertFatal(0,"SSB bitmap size value %d undefined (allowed values 1,2,3)\n",
                      scc->ssb_PositionsInBurst->present);
      }
    }
  }
}

uint32_t get_tbs(NR_Type0_PDCCH_CSS_config_t *type0_PDCCH_CSS_config,
                 NR_sched_pdsch_t *pdsch,
                 uint32_t num_total_bytes,
                 uint16_t *vrb_map)
{
  NR_tda_info_t *tda_info = &pdsch->tda_info;

  const uint16_t bwpSize = type0_PDCCH_CSS_config->num_rbs;
  int rbStart = type0_PDCCH_CSS_config->cset_start_rb;

  // Calculate number of PRB_DMRS
  uint8_t N_PRB_DMRS = pdsch->dmrs_parms.N_PRB_DMRS;
  uint16_t dmrs_length = pdsch->dmrs_parms.N_DMRS_SLOT;
  LOG_D(MAC, "dlDmrsSymbPos %x\n", pdsch->dmrs_parms.dl_dmrs_symb_pos);
  int mcsTableIdx = 0;
  int rbSize = 0;
  uint32_t TBS = 0;
  do {
    if (rbSize < bwpSize && !(vrb_map[rbStart + rbSize] & SL_to_bitmap(tda_info->startSymbolIndex, tda_info->nrOfSymbols)))
      rbSize++;
    else {
      if (pdsch->mcs < 10)
        pdsch->mcs++;
      else
        break;
    }
    TBS = nr_compute_tbs(nr_get_Qm_dl(pdsch->mcs, mcsTableIdx),
                         nr_get_code_rate_dl(pdsch->mcs, mcsTableIdx),
                         rbSize,
                         tda_info->nrOfSymbols,
                         N_PRB_DMRS * dmrs_length,
                         0,
                         0,
                         1)
          >> 3;
  } while (TBS < num_total_bytes);

  if (TBS < num_total_bytes) {
    for (int rb = 0; rb < bwpSize; rb++)
      LOG_I(NR_MAC, "vrb_map[%d] %x\n", rbStart + rb, vrb_map[rbStart + rb]);
  }
  AssertFatal(
      TBS >= num_total_bytes,
      "Couldn't allocate enough resources for %d bytes in SIB PDSCH (rbStart %d, rbSize %d, bwpSize %d SLmask %x - [%d,%d])\n",
      num_total_bytes,
      rbStart,
      rbSize,
      bwpSize,
      SL_to_bitmap(tda_info->startSymbolIndex, tda_info->nrOfSymbols),
      tda_info->startSymbolIndex,
      tda_info->nrOfSymbols);

  pdsch->rbSize = rbSize;
  pdsch->rbStart = 0;

  LOG_D(
      NR_MAC,
      "mcs=%i, startSymbolIndex = %i, nrOfSymbols = %i, rbSize = %i, TBS = %i, dmrs_length %d, N_PRB_DMRS = %d, mappingtype = %d\n",
      pdsch->mcs,
      tda_info->startSymbolIndex,
      tda_info->nrOfSymbols,
      pdsch->rbSize,
      TBS,
      dmrs_length,
      N_PRB_DMRS,
      tda_info->mapping_type);
  return TBS;
}

static uint32_t schedule_control_sib1(module_id_t module_id,
                                      int CC_id,
                                      NR_Type0_PDCCH_CSS_config_t *type0_PDCCH_CSS_config,
                                      int time_domain_allocation,
                                      NR_pdsch_dmrs_t *dmrs_parms,
                                      NR_tda_info_t *tda_info,
                                      uint8_t candidate_idx,
                                      int beam,
                                      uint16_t num_total_bytes)
{
  gNB_MAC_INST *gNB_mac = RC.nrmac[module_id];
  NR_COMMON_channels_t *cc = &gNB_mac->common_channels[CC_id];
  NR_ServingCellConfigCommon_t *scc = cc->ServingCellConfigCommon;
  uint16_t *vrb_map = cc->vrb_map[beam];

  if (gNB_mac->sched_ctrlCommon == NULL){
    LOG_D(NR_MAC,"schedule_control_common: Filling nr_mac->sched_ctrlCommon\n");
    gNB_mac->sched_ctrlCommon = calloc(1,sizeof(*gNB_mac->sched_ctrlCommon));
    gNB_mac->sched_ctrlCommon->search_space = calloc(1,sizeof(*gNB_mac->sched_ctrlCommon->search_space));
    gNB_mac->sched_ctrlCommon->coreset = calloc(1,sizeof(*gNB_mac->sched_ctrlCommon->coreset));
    fill_searchSpaceZero(gNB_mac->sched_ctrlCommon->search_space,
                         nr_slots_per_frame[*scc->ssbSubcarrierSpacing],
                         type0_PDCCH_CSS_config);
    fill_coresetZero(gNB_mac->sched_ctrlCommon->coreset, type0_PDCCH_CSS_config);
    gNB_mac->cset0_bwp_start = type0_PDCCH_CSS_config->cset_start_rb;
    gNB_mac->cset0_bwp_size = type0_PDCCH_CSS_config->num_rbs;
    gNB_mac->sched_ctrlCommon->sched_pdcch = set_pdcch_structure(NULL,
                                                                 gNB_mac->sched_ctrlCommon->search_space,
                                                                 gNB_mac->sched_ctrlCommon->coreset,
                                                                 scc,
                                                                 NULL,
                                                                 type0_PDCCH_CSS_config);
  }

  NR_sched_pdsch_t *pdsch = &gNB_mac->sched_ctrlCommon->sched_pdsch;
  pdsch->time_domain_allocation = time_domain_allocation;
  pdsch->dmrs_parms = *dmrs_parms;
  pdsch->tda_info = *tda_info;
  pdsch->mcs = 0; // starting from mcs 0
  gNB_mac->sched_ctrlCommon->num_total_bytes = num_total_bytes;

  uint8_t nr_of_candidates = 0;

  for (int i=0; i<3; i++) {
    find_aggregation_candidates(&gNB_mac->sched_ctrlCommon->aggregation_level, &nr_of_candidates, gNB_mac->sched_ctrlCommon->search_space,4<<i);
    if (nr_of_candidates>0) break; // choosing the lower value of aggregation level available
  }
  AssertFatal(nr_of_candidates>0,"nr_of_candidates is 0\n");
  gNB_mac->sched_ctrlCommon->cce_index = find_pdcch_candidate(gNB_mac,
                                                              CC_id,
                                                              gNB_mac->sched_ctrlCommon->aggregation_level,
                                                              nr_of_candidates,
                                                              beam,
                                                              &gNB_mac->sched_ctrlCommon->sched_pdcch,
                                                              gNB_mac->sched_ctrlCommon->coreset,
                                                              0);

  AssertFatal(gNB_mac->sched_ctrlCommon->cce_index >= 0, "Could not find CCE for coreset0\n");

  uint32_t TBS = get_tbs(type0_PDCCH_CSS_config, &gNB_mac->sched_ctrlCommon->sched_pdsch, gNB_mac->sched_ctrlCommon->num_total_bytes, vrb_map);

  // Mark the corresponding RBs as used
  fill_pdcch_vrb_map(gNB_mac,
                     CC_id,
                     &gNB_mac->sched_ctrlCommon->sched_pdcch,
                     gNB_mac->sched_ctrlCommon->cce_index,
                     gNB_mac->sched_ctrlCommon->aggregation_level,
                     beam);
  for (int rb = 0; rb < pdsch->rbSize; rb++) {
    vrb_map[rb + type0_PDCCH_CSS_config->cset_start_rb] |= SL_to_bitmap(tda_info->startSymbolIndex, tda_info->nrOfSymbols);
  }
  return TBS;
}

static uint32_t schedule_control_other_si(module_id_t module_id,
                                          int CC_id,
                                          NR_Type0_PDCCH_CSS_config_t *type0_PDCCH_CSS_config,
                                          int time_domain_allocation,
                                          NR_pdsch_dmrs_t *dmrs_parms,
                                          NR_tda_info_t *tda_info,
                                          uint8_t candidate_idx,
                                          int beam,
                                          uint16_t num_total_bytes)
{
  gNB_MAC_INST *gNB_mac = RC.nrmac[module_id];
  NR_COMMON_channels_t *cc = &gNB_mac->common_channels[CC_id];
  NR_ServingCellConfigCommon_t *scc = cc->ServingCellConfigCommon;
  uint16_t *vrb_map = cc->vrb_map[beam];

  if (!gNB_mac->sched_osi->coreset) {
    gNB_mac->sched_osi->coreset =
        get_coreset(gNB_mac, scc, NULL, gNB_mac->sched_osi->search_space, NR_SearchSpace__searchSpaceType_PR_common);
    gNB_mac->sched_osi->sched_pdcch =
        set_pdcch_structure(NULL, gNB_mac->sched_osi->search_space, gNB_mac->sched_osi->coreset, scc, NULL, type0_PDCCH_CSS_config);
  }

  NR_sched_pdsch_t *pdsch = &gNB_mac->sched_osi->sched_pdsch;
  pdsch->time_domain_allocation = time_domain_allocation;
  pdsch->dmrs_parms = *dmrs_parms;
  pdsch->tda_info = *tda_info;
  pdsch->mcs = 0; // starting from mcs 0
  gNB_mac->sched_osi->num_total_bytes = num_total_bytes;

  uint8_t nr_of_candidates;

  for (int i = 0; i < 3; i++) {
    find_aggregation_candidates(&gNB_mac->sched_osi->aggregation_level,
                                &nr_of_candidates,
                                gNB_mac->sched_ctrlCommon->search_space,
                                4 << i);
    if (nr_of_candidates > 0)
      break; // choosing the lower value of aggregation level available
  }
  AssertFatal(nr_of_candidates > 0, "nr_of_candidates is 0\n");
  gNB_mac->sched_osi->cce_index = find_pdcch_candidate(gNB_mac,
                                                       CC_id,
                                                       gNB_mac->sched_osi->aggregation_level,
                                                       nr_of_candidates,
                                                       beam,
                                                       &gNB_mac->sched_osi->sched_pdcch,
                                                       gNB_mac->sched_osi->coreset,
                                                       0);

  AssertFatal(gNB_mac->sched_osi->cce_index >= 0, "Could not find CCE for coreset0\n");

  // Calculate number of PRB_DMRS
  uint32_t TBS = get_tbs(type0_PDCCH_CSS_config, &gNB_mac->sched_osi->sched_pdsch, gNB_mac->sched_osi->num_total_bytes, vrb_map);
  // Mark the corresponding RBs as used
  fill_pdcch_vrb_map(gNB_mac,
                     CC_id,
                     &gNB_mac->sched_osi->sched_pdcch,
                     gNB_mac->sched_osi->cce_index,
                     gNB_mac->sched_osi->aggregation_level,
                     beam);
  for (int rb = 0; rb < pdsch->rbSize; rb++) {
    vrb_map[rb + type0_PDCCH_CSS_config->cset_start_rb] |= SL_to_bitmap(tda_info->startSymbolIndex, tda_info->nrOfSymbols);
  }
  return TBS;
}

static void nr_fill_nfapi_dl_SIB_pdu(int Mod_idP,
                                    NR_UE_sched_osi_ctrl_t *sched_osi,
                                    NR_UE_sched_ctrl_t *sched_ctrlCommon,
                                    nfapi_nr_dl_tti_request_body_t *dl_req,
                                    int pdu_index,
                                    NR_Type0_PDCCH_CSS_config_t *type0_PDCCH_CSS_config,
                                    uint32_t TBS,
                                    int StartSymbolIndex,
                                    int NrOfSymbols,
                                    int beam_index)
{
  // will set data according to SIB type (SIB1/OtherSI)
  NR_sched_pdsch_t *pdsch          = (sched_ctrlCommon ? &sched_ctrlCommon->sched_pdsch      : &sched_osi->sched_pdsch);
  NR_sched_pdcch_t *pdcch          = (sched_ctrlCommon ? &sched_ctrlCommon->sched_pdcch      : &sched_osi->sched_pdcch);
  NR_SearchSpace_t *search_space   = (sched_ctrlCommon ? sched_ctrlCommon->search_space      : sched_osi->search_space);
  NR_ControlResourceSet_t *coreset = (sched_ctrlCommon ? sched_ctrlCommon->coreset           : sched_osi->coreset);
  uint8_t aggregation_level        = (sched_ctrlCommon ? sched_ctrlCommon->aggregation_level : sched_osi->aggregation_level);
  int cce_index                    = (sched_ctrlCommon ? sched_ctrlCommon->cce_index         : sched_osi->cce_index);

  gNB_MAC_INST *gNB_mac = RC.nrmac[Mod_idP];
  NR_COMMON_channels_t *cc = gNB_mac->common_channels;
  NR_ServingCellConfigCommon_t *scc = cc->ServingCellConfigCommon;
  int mcsTableIdx = 0;

  nfapi_nr_dl_tti_request_pdu_t *dl_tti_pdcch_pdu = &dl_req->dl_tti_pdu_list[dl_req->nPDUs];
  memset(dl_tti_pdcch_pdu, 0, sizeof(*dl_tti_pdcch_pdu));
  dl_tti_pdcch_pdu->PDUType = NFAPI_NR_DL_TTI_PDCCH_PDU_TYPE;
  dl_tti_pdcch_pdu->PDUSize = (uint16_t)(4+sizeof(nfapi_nr_dl_tti_pdcch_pdu));
  dl_req->nPDUs += 1;
  nfapi_nr_dl_tti_pdcch_pdu_rel15_t *pdcch_pdu_rel15 = &dl_tti_pdcch_pdu->pdcch_pdu.pdcch_pdu_rel15;
  nr_configure_pdcch(pdcch_pdu_rel15,
                     coreset,
                     pdcch,
                     sched_ctrlCommon ? false : true);

  nfapi_nr_dl_tti_request_pdu_t *dl_tti_pdsch_pdu = &dl_req->dl_tti_pdu_list[dl_req->nPDUs];
  memset(dl_tti_pdsch_pdu, 0, sizeof(*dl_tti_pdcch_pdu));
  dl_tti_pdsch_pdu->PDUType = NFAPI_NR_DL_TTI_PDSCH_PDU_TYPE;
  dl_tti_pdsch_pdu->PDUSize = (uint16_t)(4+sizeof(nfapi_nr_dl_tti_pdsch_pdu));
  dl_req->nPDUs += 1;
  nfapi_nr_dl_tti_pdsch_pdu_rel15_t *pdsch_pdu_rel15 = &dl_tti_pdsch_pdu->pdsch_pdu.pdsch_pdu_rel15;

  pdsch_pdu_rel15->precodingAndBeamforming.num_prgs = 0;
  pdsch_pdu_rel15->precodingAndBeamforming.prg_size = 0;
  pdsch_pdu_rel15->precodingAndBeamforming.dig_bf_interfaces = 1;
  pdsch_pdu_rel15->precodingAndBeamforming.prgs_list[0].pm_idx = 0;
  pdsch_pdu_rel15->precodingAndBeamforming.prgs_list[0].dig_bf_interface_list[0].beam_idx = beam_index;

  pdcch_pdu_rel15->CoreSetType = NFAPI_NR_CSET_CONFIG_MIB_SIB1;

  pdsch_pdu_rel15->pduBitmap = 0;
  pdsch_pdu_rel15->rnti = SI_RNTI;
  pdsch_pdu_rel15->pduIndex = pdu_index;

  pdsch_pdu_rel15->BWPSize = type0_PDCCH_CSS_config->num_rbs;
  pdsch_pdu_rel15->BWPStart = type0_PDCCH_CSS_config->cset_start_rb;

  pdsch_pdu_rel15->SubcarrierSpacing = type0_PDCCH_CSS_config->scs_pdcch;
  pdsch_pdu_rel15->CyclicPrefix = 0;

  pdsch_pdu_rel15->NrOfCodewords = 1;
  pdsch_pdu_rel15->targetCodeRate[0] = nr_get_code_rate_dl(pdsch->mcs, mcsTableIdx);
  pdsch_pdu_rel15->qamModOrder[0] = nr_get_Qm_dl(pdsch->mcs, mcsTableIdx);
  pdsch_pdu_rel15->mcsIndex[0] = pdsch->mcs;
  pdsch_pdu_rel15->mcsTable[0] = mcsTableIdx;
  pdsch_pdu_rel15->rvIndex[0] = nr_rv_round_map[0];
  pdsch_pdu_rel15->dataScramblingId = *scc->physCellId;
  pdsch_pdu_rel15->nrOfLayers = 1;
  pdsch_pdu_rel15->transmissionScheme = 0;
  pdsch_pdu_rel15->refPoint = 1;
  pdsch_pdu_rel15->dmrsConfigType = 0;
  pdsch_pdu_rel15->dlDmrsScramblingId = *scc->physCellId;
  pdsch_pdu_rel15->SCID = 0;
  pdsch_pdu_rel15->numDmrsCdmGrpsNoData = pdsch->dmrs_parms.numDmrsCdmGrpsNoData;
  pdsch_pdu_rel15->dmrsPorts = 1;
  pdsch_pdu_rel15->resourceAlloc = 1;
  pdsch_pdu_rel15->rbStart = pdsch->rbStart;
  pdsch_pdu_rel15->rbSize = pdsch->rbSize;
  pdsch_pdu_rel15->VRBtoPRBMapping = 0;
  pdsch_pdu_rel15->TBSize[0] = TBS;
  pdsch_pdu_rel15->StartSymbolIndex = StartSymbolIndex;
  pdsch_pdu_rel15->NrOfSymbols = NrOfSymbols;
  pdsch_pdu_rel15->dlDmrsSymbPos = pdsch->dmrs_parms.dl_dmrs_symb_pos;

  LOG_D(NR_MAC,
        "OtherSI:bwpStart %d, bwpSize %d, rbStart %d, rbSize %d, dlDmrsSymbPos = 0x%x\n",
        pdsch_pdu_rel15->BWPStart,
        pdsch_pdu_rel15->BWPSize,
        pdsch_pdu_rel15->rbStart,
        pdsch_pdu_rel15->rbSize,
        pdsch_pdu_rel15->dlDmrsSymbPos);

  pdsch_pdu_rel15->maintenance_parms_v3.tbSizeLbrmBytes = nr_compute_tbslbrm(0,
                                                                            pdsch_pdu_rel15->BWPSize,
                                                                            1);
  pdsch_pdu_rel15->maintenance_parms_v3.ldpcBaseGraph = get_BG(TBS<<3,pdsch_pdu_rel15->targetCodeRate[0]);

  /* Fill PDCCH DL DCI PDU */
  nfapi_nr_dl_dci_pdu_t *dci_pdu = &pdcch_pdu_rel15->dci_pdu[pdcch_pdu_rel15->numDlDci];
  pdcch_pdu_rel15->numDlDci++;
  dci_pdu->RNTI = SI_RNTI;
  dci_pdu->ScramblingId = *scc->physCellId;
  dci_pdu->ScramblingRNTI = 0;
  dci_pdu->AggregationLevel = aggregation_level;
  dci_pdu->CceIndex = cce_index;
  dci_pdu->beta_PDCCH_1_0 = 0;
  dci_pdu->powerControlOffsetSS = 1;

  dci_pdu->precodingAndBeamforming.num_prgs = 0;
  dci_pdu->precodingAndBeamforming.prg_size = 0;
  dci_pdu->precodingAndBeamforming.dig_bf_interfaces = 1;
  dci_pdu->precodingAndBeamforming.prgs_list[0].pm_idx = 0;
  dci_pdu->precodingAndBeamforming.prgs_list[0].dig_bf_interface_list[0].beam_idx = beam_index;

  /* DCI payload */
  dci_pdu_rel15_t dci_payload;
  memset(&dci_payload, 0, sizeof(dci_pdu_rel15_t));

  dci_payload.bwp_indicator.val = 0;

  // frequency domain assignment
  dci_payload.frequency_domain_assignment.val = PRBalloc_to_locationandbandwidth0(
      pdsch_pdu_rel15->rbSize, pdsch_pdu_rel15->rbStart, type0_PDCCH_CSS_config->num_rbs);

  dci_payload.time_domain_assignment.val = pdsch->time_domain_allocation;
  dci_payload.mcs = pdsch->mcs;
  dci_payload.rv = pdsch_pdu_rel15->rvIndex[0];
  dci_payload.harq_pid.val = 0;
  dci_payload.ndi = 0;
  dci_payload.dai[0].val = 0;
  dci_payload.tpc = 0; // table 7.2.1-1 in 38.213
  dci_payload.pucch_resource_indicator = 0;
  dci_payload.pdsch_to_harq_feedback_timing_indicator.val = 0;
  dci_payload.antenna_ports.val = 0;
  dci_payload.dmrs_sequence_initialization.val = pdsch_pdu_rel15->SCID;
  dci_payload.system_info_indicator = sched_ctrlCommon ? 0 : 1;

  int dci_format = NR_DL_DCI_FORMAT_1_0;
  int rnti_type = TYPE_SI_RNTI_;

  fill_dci_pdu_rel15(NULL,
                     NULL,
                     NULL,
                     &pdcch_pdu_rel15->dci_pdu[pdcch_pdu_rel15->numDlDci - 1],
                     &dci_payload,
                     dci_format,
                     rnti_type,
                     0,
                     search_space,
                     coreset,
                     0,
                     gNB_mac->cset0_bwp_size);

  LOG_D(MAC,
        "BWPSize: %3i, BWPStart: %3i, SubcarrierSpacing: %i, CyclicPrefix: %i, StartSymbolIndex: %i, DurationSymbols: %i, "
        "CceRegMappingType: %i, RegBundleSize: %i, InterleaverSize: %i, CoreSetType: %i, ShiftIndex: %i, precoderGranularity: %i, "
        "numDlDci: %i\n",
        pdcch_pdu_rel15->BWPSize,
        pdcch_pdu_rel15->BWPStart,
        pdcch_pdu_rel15->SubcarrierSpacing,
        pdcch_pdu_rel15->CyclicPrefix,
        pdcch_pdu_rel15->StartSymbolIndex,
        pdcch_pdu_rel15->DurationSymbols,
        pdcch_pdu_rel15->CceRegMappingType,
        pdcch_pdu_rel15->RegBundleSize,
        pdcch_pdu_rel15->InterleaverSize,
        pdcch_pdu_rel15->CoreSetType,
        pdcch_pdu_rel15->ShiftIndex,
        pdcch_pdu_rel15->precoderGranularity,
        pdcch_pdu_rel15->numDlDci);
}

void schedule_nr_sib1(module_id_t module_idP,
                      frame_t frameP,
                      sub_frame_t slotP,
                      nfapi_nr_dl_tti_request_t *DL_req,
                      nfapi_nr_tx_data_request_t *TX_req)
{
  /* already mutex protected: held in gNB_dlsch_ulsch_scheduler() */
  // TODO: Get these values from RRC
  const int CC_id = 0;
  uint8_t candidate_idx = 0;

  gNB_MAC_INST *gNB_mac = RC.nrmac[module_idP];
  NR_ServingCellConfigCommon_t *scc = gNB_mac->common_channels[CC_id].ServingCellConfigCommon;

  int time_domain_allocation = gNB_mac->radio_config.sib1_tda;

  int L_max;
  switch (scc->ssb_PositionsInBurst->present) {
    case 1:
      L_max = 4;
      break;
    case 2:
      L_max = 8;
      break;
    case 3:
      L_max = 64;
      break;
    default:
      AssertFatal(0,"SSB bitmap size value %d undefined (allowed values 1,2,3)\n",
                  scc->ssb_PositionsInBurst->present);
  }

  for (int i = 0; i < L_max; i++) {

    NR_Type0_PDCCH_CSS_config_t *type0_PDCCH_CSS_config = &gNB_mac->type0_PDCCH_CSS_config[i];

    if((frameP % 2 == type0_PDCCH_CSS_config->sfn_c) &&
       (slotP == type0_PDCCH_CSS_config->n_0) &&
       (type0_PDCCH_CSS_config->num_rbs > 0) &&
       (type0_PDCCH_CSS_config->active == true)) {

      const int n_slots_frame = nr_slots_per_frame[*scc->ssbSubcarrierSpacing];
      int beam_index = get_fapi_beamforming_index(gNB_mac, i);
      NR_beam_alloc_t beam = beam_allocation_procedure(&gNB_mac->beam_info, frameP, slotP, beam_index, n_slots_frame);
      AssertFatal(beam.idx >= 0, "Cannot allocate SIB1 corresponding to SSB %d in any available beam\n", i);
      LOG_D(NR_MAC,"(%d.%d) SIB1 transmission: ssb_index %d\n", frameP, slotP, type0_PDCCH_CSS_config->ssb_index);

      default_table_type_t table_type = get_default_table_type(type0_PDCCH_CSS_config->type0_pdcch_ss_mux_pattern);
      // assuming normal CP
      NR_tda_info_t tda_info = get_info_from_tda_tables(table_type, time_domain_allocation, gNB_mac->common_channels->ServingCellConfigCommon->dmrs_TypeA_Position, true);

      AssertFatal((tda_info.startSymbolIndex + tda_info.nrOfSymbols) < 14, "SIB1 TDA %d would cause overlap with CSI-RS. Please select a different SIB1 TDA.\n", time_domain_allocation);

      NR_pdsch_dmrs_t dmrs_parms = get_dl_dmrs_params(scc,
                                                      NULL,
                                                      &tda_info,
                                                      1);


      NR_COMMON_channels_t *cc = &gNB_mac->common_channels[0];
      // Configure sched_ctrlCommon for SIB1
      uint32_t TBS = schedule_control_sib1(module_idP, CC_id,
                                           type0_PDCCH_CSS_config,
                                           time_domain_allocation,
                                           &dmrs_parms,
                                           &tda_info,
                                           candidate_idx,
                                           beam.idx,
                                           cc->sib1_bcch_length);

      nfapi_nr_dl_tti_request_body_t *dl_req = &DL_req->dl_tti_request_body;
      int pdu_index = gNB_mac->pdu_index[0]++;
      nr_fill_nfapi_dl_SIB_pdu(module_idP, 
                              NULL,
                              gNB_mac->sched_ctrlCommon,
                              dl_req,
                              pdu_index,
                              type0_PDCCH_CSS_config,
                              TBS,
                              tda_info.startSymbolIndex,
                              tda_info.nrOfSymbols,
                              beam_index);

      const int ntx_req = TX_req->Number_of_PDUs;
      nfapi_nr_pdu_t *tx_req = &TX_req->pdu_list[ntx_req];

      // Data to be transmitted
      memcpy(tx_req->TLVs[0].value.direct, cc->sib1_bcch_pdu, TBS);

      tx_req->PDU_index  = pdu_index;
      tx_req->num_TLV = 1;
      tx_req->TLVs[0].length = TBS;
      tx_req->PDU_length = compute_PDU_length(tx_req->num_TLV, tx_req->TLVs[0].length);
      TX_req->Number_of_PDUs++;
      TX_req->SFN = frameP;
      TX_req->Slot = slotP;

      type0_PDCCH_CSS_config->active = false;

      T(T_GNB_MAC_DL_PDU_WITH_DATA,
        T_INT(module_idP),
        T_INT(CC_id),
        T_INT(0xffff),
        T_INT(frameP),
        T_INT(slotP),
        T_INT(0 /* harq_pid */),
        T_BUFFER(cc->sib1_bcch_pdu, cc->sib1_bcch_length));
    }
  }
}

struct NR_SchedulingInfo2_r17 *find_sib19_sched_info(const struct NR_SI_SchedulingInfo_v1700 *si_schedulinginfo2_r17)
{
  for (int i = 0; i < si_schedulinginfo2_r17->schedulingInfoList2_r17.list.count; i++) {
    const struct NR_SIB_Mapping_v1700 *sib_MappingInfo_r17 =
        &si_schedulinginfo2_r17->schedulingInfoList2_r17.list.array[i]->sib_MappingInfo_r17;
    for (int j = 0; j < sib_MappingInfo_r17->list.count; j++) {
      if (sib_MappingInfo_r17->list.array[j]->sibType_r17.choice.type1_r17
          == NR_SIB_TypeInfo_v1700__sibType_r17__type1_r17_sibType19) {
        return si_schedulinginfo2_r17->schedulingInfoList2_r17.list.array[i];
      }
    }
  }
  return NULL;
}

void schedule_nr_sib19(module_id_t module_idP,
                      frame_t frameP,
                      sub_frame_t slotP,
                      nfapi_nr_dl_tti_request_t *DL_req,
                      nfapi_nr_tx_data_request_t *TX_req,
                      int sib19_bcch_length,
                      uint8_t *sib19_bcch_pdu)
{
  /* already mutex protected: held in gNB_dlsch_ulsch_scheduler() */

  const int CC_id = 0;
  uint8_t candidate_idx = 0;

  gNB_MAC_INST *gNB_mac = RC.nrmac[module_idP];
  NR_ServingCellConfigCommon_t *scc = gNB_mac->common_channels[CC_id].ServingCellConfigCommon;
  NR_COMMON_channels_t *cc = &gNB_mac->common_channels[0];

  // there is no reason to execute further if sched_osi not initialized
  if (!gNB_mac->sched_osi)
    return;

  // get sib19 scheduling info from sib1
  struct NR_SI_SchedulingInfo_v1700 *si_schedulinginfo2_r17 = cc->sib1->message.choice.c1->choice.systemInformationBlockType1->nonCriticalExtension->nonCriticalExtension->nonCriticalExtension->si_SchedulingInfo_v1700;
  if (!si_schedulinginfo2_r17) {
    LOG_E(GNB_APP, "SIB1 does not contain si_SchedulingInfo_v1700\n");
    return;
  }

  struct NR_SchedulingInfo2_r17 *sib19_sched_info = find_sib19_sched_info(si_schedulinginfo2_r17);
  if (!sib19_sched_info) {
    LOG_D(GNB_APP, "SIB1 does not contain scheduling info for SIB19\n");
    return;
  }

  // convert periodicity enum type into actual frames number
  int frame = 8 << sib19_sched_info->si_Periodicity_r17;

  int time_domain_allocation = 1; // row 0 of pdsch tda common table
  int L_max;
  switch (scc->ssb_PositionsInBurst->present) {
    case 1:
      L_max = 4;
      break;
    case 2:
      L_max = 8;
      break;
    case 3:
      L_max = 64;
      break;
    default:
      AssertFatal(0,"SSB bitmap size value %d undefined (allowed values 1,2,3)\n",
                  scc->ssb_PositionsInBurst->present);
  }

  const int window_length = 10; 
  for (int i = 0; i < L_max; i++) {

    NR_Type0_PDCCH_CSS_config_t *type0_PDCCH_CSS_config = &gNB_mac->type0_PDCCH_CSS_config[i];
    if(((frameP - 1) % frame == 0) &&
       (slotP == ((sib19_sched_info->si_WindowPosition_r17 % window_length) - 1)) &&
       (type0_PDCCH_CSS_config->num_rbs > 0)) {

      const int n_slots_frame = nr_slots_per_frame[*scc->ssbSubcarrierSpacing];
      int beam_index = get_fapi_beamforming_index(gNB_mac, i);
      NR_beam_alloc_t beam = beam_allocation_procedure(&gNB_mac->beam_info, frameP, slotP, beam_index, n_slots_frame);
      AssertFatal(beam.idx >= 0, "Cannot allocate SIB19 corresponding to SSB %d in any available beam\n", i);
      LOG_D(NR_MAC,"(%d.%d) SIB19 transmission: ssb_index %d\n", frameP, slotP, type0_PDCCH_CSS_config->ssb_index);

      NR_tda_info_t tda_info = set_tda_info_from_list(scc->downlinkConfigCommon->initialDownlinkBWP->pdsch_ConfigCommon->choice.setup->pdsch_TimeDomainAllocationList, time_domain_allocation);
      LOG_D(NR_MAC,"tda_info.startSymbolIndex: %d, tda_info.nrOfSymbols: %d\n", tda_info.startSymbolIndex, tda_info.nrOfSymbols);

      AssertFatal((tda_info.startSymbolIndex + tda_info.nrOfSymbols) < 14, "SIB19 TDA %d would cause overlap with CSI-RS. Please select a different SIB1 TDA.\n", time_domain_allocation);

      NR_pdsch_dmrs_t dmrs_parms = get_dl_dmrs_params(scc,
                                                      NULL,
                                                      &tda_info,
                                                      1);

      // Configure sched_osi for SIB19
      uint32_t TBS = schedule_control_other_si(module_idP,
                                               CC_id,
                                               type0_PDCCH_CSS_config,
                                               time_domain_allocation,
                                               &dmrs_parms,
                                               &tda_info,
                                               candidate_idx,
                                               beam.idx,
                                               sib19_bcch_length);

      nfapi_nr_dl_tti_request_body_t *dl_req = &DL_req->dl_tti_request_body;
      int pdu_index = gNB_mac->pdu_index[0]++;

      nr_fill_nfapi_dl_SIB_pdu(module_idP,
                               gNB_mac->sched_osi,
                               NULL,
                               dl_req,
                               pdu_index,
                               type0_PDCCH_CSS_config,
                               TBS,
                               tda_info.startSymbolIndex,
                               tda_info.nrOfSymbols,
                               beam_index);

      const int ntx_req = TX_req->Number_of_PDUs;
      nfapi_nr_pdu_t *tx_req = &TX_req->pdu_list[ntx_req];

      // Data to be transmitted
      memcpy(tx_req->TLVs[0].value.direct, sib19_bcch_pdu, TBS);

      tx_req->PDU_length = TBS;
      tx_req->PDU_index  = pdu_index;
      tx_req->num_TLV = 1;
      tx_req->TLVs[0].length = TBS + 2;
      TX_req->Number_of_PDUs++;
      TX_req->SFN = frameP;
      TX_req->Slot = slotP;

      type0_PDCCH_CSS_config->active = false;

      T(T_GNB_MAC_DL_PDU_WITH_DATA,
        T_INT(module_idP),
        T_INT(CC_id),
        T_INT(SI_RNTI),
        T_INT(frameP),
        T_INT(slotP),
        T_INT(0 /* harq_pid */),
        T_BUFFER(sib19_bcch_pdu, sib19_bcch_length));
    }
  }
}
