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

/*
  gnb_config.c
  -------------------
  AUTHOR  : Lionel GAUTHIER, navid nikaein, Laurent Winckel, WEI-TAI CHEN
  COMPANY : EURECOM, NTUST
  EMAIL   : Lionel.Gauthier@eurecom.fr, navid.nikaein@eurecom.fr, kroempa@gmail.com
*/

#include "executables/softmodem-common.h"
#include "gnb_config.h"
#include "enb_paramdef.h"
#include "UTIL/OTG/otg.h"
#include "sctp_default_values.h"
#include "f1ap_common.h"
#include "PHY/INIT/nr_phy_init.h"
#include "radio/ETHERNET/ethernet_lib.h"
#include "nfapi_vnf.h"
#include "nfapi_pnf.h"
#include "prs_nr_paramdef.h"
#include "L1_nr_paramdef.h"
#include "MACRLC_nr_paramdef.h"
#include "gnb_paramdef.h"
#include "NR_MAC_gNB/mac_proto.h"
#include "RRC/NR/nr_rrc_extern.h"
#include "nfapi/oai_integration/vendor_ext.h"
#ifdef ENABLE_AERIAL
#include "nfapi/oai_integration/aerial/fapi_vnf_p5.h"
#endif
#include "lib/f1ap_interface_management.h"

static int DEFBANDS[] = {7};
static int DEFENBS[] = {0};
static int DEFBFW[] = {0x00007fff};
static int DEFRUTPCORES[] = {-1,-1,-1,-1};

/**
 * @brief Helper define to allocate and initialize SetupRelease structures
 */
#define INIT_SETUP_RELEASE(type, element)                                    \
  do {                                                                       \
    element = calloc_or_fail(1, sizeof(*element));                           \
    (element)->present = NR_SetupRelease_##type##_PR_setup;                  \
    (element)->choice.setup = CALLOC(1, sizeof(*((element)->choice.setup))); \
  } while (0)

/**
 * Allocate memory and initialize ServingCellConfigCommon struct members
 */
void prepare_scc(NR_ServingCellConfigCommon_t *scc)
{
  // NR_ServingCellConfigCommon
  scc->physCellId = calloc_or_fail(1, sizeof(*scc->physCellId));
  scc->n_TimingAdvanceOffset = calloc_or_fail(1, sizeof(*scc->n_TimingAdvanceOffset));
  scc->ssb_PositionsInBurst = calloc_or_fail(1, sizeof(*scc->ssb_PositionsInBurst));
  scc->ssb_periodicityServingCell = calloc_or_fail(1, sizeof(*scc->ssb_periodicityServingCell));
  scc->ssbSubcarrierSpacing = calloc_or_fail(1, sizeof(*scc->ssbSubcarrierSpacing));
  scc->tdd_UL_DL_ConfigurationCommon = calloc_or_fail(1, sizeof(*scc->tdd_UL_DL_ConfigurationCommon));
  scc->tdd_UL_DL_ConfigurationCommon->pattern2 = calloc_or_fail(1, sizeof(*scc->tdd_UL_DL_ConfigurationCommon->pattern2));
  scc->downlinkConfigCommon = calloc_or_fail(1, sizeof(*scc->downlinkConfigCommon));
  scc->downlinkConfigCommon->frequencyInfoDL = calloc_or_fail(1, sizeof(*scc->downlinkConfigCommon->frequencyInfoDL));
  scc->downlinkConfigCommon->initialDownlinkBWP = calloc_or_fail(1, sizeof(*scc->downlinkConfigCommon->initialDownlinkBWP));

  NR_FrequencyInfoDL_t *frequencyInfoDL = scc->downlinkConfigCommon->frequencyInfoDL;
  frequencyInfoDL->absoluteFrequencySSB = calloc_or_fail(1, sizeof(*frequencyInfoDL->absoluteFrequencySSB));
  NR_FreqBandIndicatorNR_t *dl_frequencyBandList = calloc_or_fail(1, sizeof(*dl_frequencyBandList));
  asn1cSeqAdd(&frequencyInfoDL->frequencyBandList.list, dl_frequencyBandList);
  struct NR_SCS_SpecificCarrier *dl_scs_SpecificCarrierList = calloc_or_fail(1, sizeof(*dl_scs_SpecificCarrierList));
  asn1cSeqAdd(&frequencyInfoDL->scs_SpecificCarrierList.list, dl_scs_SpecificCarrierList);

  INIT_SETUP_RELEASE(PDCCH_ConfigCommon, scc->downlinkConfigCommon->initialDownlinkBWP->pdcch_ConfigCommon);
  NR_SetupRelease_PDCCH_ConfigCommon_t *pdcch_ConfigCommon = scc->downlinkConfigCommon->initialDownlinkBWP->pdcch_ConfigCommon;
  struct NR_PDCCH_ConfigCommon *pdcchCCSetup = pdcch_ConfigCommon->choice.setup;
  pdcchCCSetup->controlResourceSetZero = calloc_or_fail(1, sizeof(*pdcchCCSetup->controlResourceSetZero));
  pdcchCCSetup->searchSpaceZero = calloc_or_fail(1, sizeof(*pdcchCCSetup->searchSpaceZero));
  pdcchCCSetup->commonControlResourceSet = NULL;

  INIT_SETUP_RELEASE(PDSCH_ConfigCommon, scc->downlinkConfigCommon->initialDownlinkBWP->pdsch_ConfigCommon);
  NR_SetupRelease_PDSCH_ConfigCommon_t *pdsch_ConfigCommon = scc->downlinkConfigCommon->initialDownlinkBWP->pdsch_ConfigCommon;
  pdsch_ConfigCommon->choice.setup->pdsch_TimeDomainAllocationList =
      calloc_or_fail(1, sizeof(*pdsch_ConfigCommon->choice.setup->pdsch_TimeDomainAllocationList));

  scc->uplinkConfigCommon = calloc_or_fail(1, sizeof(*scc->uplinkConfigCommon));
  scc->uplinkConfigCommon->frequencyInfoUL = calloc_or_fail(1, sizeof(*scc->uplinkConfigCommon->frequencyInfoUL));
  scc->uplinkConfigCommon->initialUplinkBWP = calloc_or_fail(1, sizeof(*scc->uplinkConfigCommon->initialUplinkBWP));

  NR_FrequencyInfoUL_t *frequencyInfoUL = scc->uplinkConfigCommon->frequencyInfoUL;
  NR_FreqBandIndicatorNR_t *ul_frequencyBandList = calloc_or_fail(1, sizeof(*ul_frequencyBandList));
  frequencyInfoUL->frequencyBandList = calloc_or_fail(1, sizeof(*frequencyInfoUL->frequencyBandList));
  asn1cSeqAdd(&frequencyInfoUL->frequencyBandList->list, ul_frequencyBandList);
  frequencyInfoUL->absoluteFrequencyPointA = calloc_or_fail(1, sizeof(*frequencyInfoUL->absoluteFrequencyPointA));
  frequencyInfoUL->p_Max = calloc_or_fail(1, sizeof(*frequencyInfoUL->p_Max));

  struct NR_SCS_SpecificCarrier *ul_scs_SpecificCarrierList = calloc_or_fail(1, sizeof(*ul_scs_SpecificCarrierList));
  asn1cSeqAdd(&frequencyInfoUL->scs_SpecificCarrierList.list, ul_scs_SpecificCarrierList);

  INIT_SETUP_RELEASE(RACH_ConfigCommon, scc->uplinkConfigCommon->initialUplinkBWP->rach_ConfigCommon);
  NR_SetupRelease_RACH_ConfigCommon_t *rach_ConfigCommon = scc->uplinkConfigCommon->initialUplinkBWP->rach_ConfigCommon;
  struct NR_RACH_ConfigCommon *rachCCSetup = rach_ConfigCommon->choice.setup;
  rachCCSetup->ssb_perRACH_OccasionAndCB_PreamblesPerSSB =
      calloc_or_fail(1, sizeof(*rachCCSetup->ssb_perRACH_OccasionAndCB_PreamblesPerSSB));
  rachCCSetup->rsrp_ThresholdSSB = calloc_or_fail(1, sizeof(*rachCCSetup->rsrp_ThresholdSSB));
  rachCCSetup->msg1_SubcarrierSpacing = calloc_or_fail(1, sizeof(*rachCCSetup->msg1_SubcarrierSpacing));
  rachCCSetup->msg3_transformPrecoder = calloc_or_fail(1, sizeof(*rachCCSetup->msg3_transformPrecoder));
  *rachCCSetup->msg3_transformPrecoder = NR_PUSCH_Config__transformPrecoder_disabled;

  INIT_SETUP_RELEASE(PUSCH_ConfigCommon, scc->uplinkConfigCommon->initialUplinkBWP->pusch_ConfigCommon);
  NR_SetupRelease_PUSCH_ConfigCommon_t *pusch_ConfigCommon = scc->uplinkConfigCommon->initialUplinkBWP->pusch_ConfigCommon;
  struct NR_PUSCH_ConfigCommon *puschCCSetup = pusch_ConfigCommon->choice.setup;
  puschCCSetup->groupHoppingEnabledTransformPrecoding = NULL;
  puschCCSetup->pusch_TimeDomainAllocationList = calloc_or_fail(1, sizeof(*puschCCSetup->pusch_TimeDomainAllocationList));
  puschCCSetup->msg3_DeltaPreamble = calloc_or_fail(1, sizeof(*puschCCSetup->msg3_DeltaPreamble));
  puschCCSetup->p0_NominalWithGrant = calloc_or_fail(1, sizeof(*puschCCSetup->p0_NominalWithGrant));

  INIT_SETUP_RELEASE(PUCCH_ConfigCommon, scc->uplinkConfigCommon->initialUplinkBWP->pucch_ConfigCommon);
  NR_SetupRelease_PUCCH_ConfigCommon_t *pucch_ConfigCommon = scc->uplinkConfigCommon->initialUplinkBWP->pucch_ConfigCommon;
  struct NR_PUCCH_ConfigCommon *pucchCCSetup = pucch_ConfigCommon->choice.setup;
  pucchCCSetup->p0_nominal = calloc_or_fail(1, sizeof(*pucchCCSetup->p0_nominal));
  pucchCCSetup->pucch_ResourceCommon = calloc_or_fail(1, sizeof(*pucchCCSetup->pucch_ResourceCommon));
  pucchCCSetup->hoppingId = calloc_or_fail(1, sizeof(*pucchCCSetup->hoppingId));

  scc->ext2 = calloc_or_fail(1, sizeof(*scc->ext2));
  scc->ext2->ntn_Config_r17 = calloc_or_fail(1, sizeof(*scc->ext2->ntn_Config_r17));
  scc->ext2->ntn_Config_r17->cellSpecificKoffset_r17 = calloc_or_fail(1, sizeof(*scc->ext2->ntn_Config_r17->cellSpecificKoffset_r17));

  scc->ext2->ntn_Config_r17->ephemerisInfo_r17 = calloc_or_fail(1, sizeof(*scc->ext2->ntn_Config_r17->ephemerisInfo_r17));
  scc->ext2->ntn_Config_r17->ta_Info_r17 = calloc_or_fail(1, sizeof(*scc->ext2->ntn_Config_r17->ta_Info_r17));

  scc->ext2->ntn_Config_r17->ephemerisInfo_r17->present = NR_EphemerisInfo_r17_PR_positionVelocity_r17;
  scc->ext2->ntn_Config_r17->ephemerisInfo_r17->choice.positionVelocity_r17 =
      calloc_or_fail(1, sizeof(*scc->ext2->ntn_Config_r17->ephemerisInfo_r17->choice.positionVelocity_r17));
}
void prepare_msgA_scc(NR_ServingCellConfigCommon_t *scc) {
  NR_BWP_UplinkCommon_t *initialUplinkBWP = scc->uplinkConfigCommon->initialUplinkBWP;
  // Add the struct ext1
  initialUplinkBWP->ext1 = calloc(1, sizeof(*initialUplinkBWP->ext1));
  initialUplinkBWP->ext1->msgA_ConfigCommon_r16 = calloc(1, sizeof(*initialUplinkBWP->ext1->msgA_ConfigCommon_r16));
  initialUplinkBWP->ext1->msgA_ConfigCommon_r16->present = NR_SetupRelease_MsgA_ConfigCommon_r16_PR_setup;
  initialUplinkBWP->ext1->msgA_ConfigCommon_r16->choice.setup =
      calloc(1, sizeof(*initialUplinkBWP->ext1->msgA_ConfigCommon_r16->choice.setup));
  NR_MsgA_ConfigCommon_r16_t *NR_MsgA_ConfigCommon_r16 = initialUplinkBWP->ext1->msgA_ConfigCommon_r16->choice.setup;
  NR_MsgA_ConfigCommon_r16->rach_ConfigCommonTwoStepRA_r16.rach_ConfigGenericTwoStepRA_r16.msgB_ResponseWindow_r16 =
      calloc(1, sizeof(long));
  NR_MsgA_ConfigCommon_r16->rach_ConfigCommonTwoStepRA_r16.msgA_RSRP_Threshold_r16 = calloc(1, sizeof(NR_RSRP_Range_t));

  NR_MsgA_ConfigCommon_r16->rach_ConfigCommonTwoStepRA_r16.msgA_CB_PreamblesPerSSB_PerSharedRO_r16 = calloc(1, sizeof(long));

  NR_MsgA_ConfigCommon_r16->msgA_PUSCH_Config_r16 = calloc(1, sizeof(NR_MsgA_PUSCH_Config_r16_t));
  NR_MsgA_PUSCH_Config_r16_t *msgA_PUSCH_Config_r16 = NR_MsgA_ConfigCommon_r16->msgA_PUSCH_Config_r16;
  msgA_PUSCH_Config_r16->msgA_PUSCH_ResourceGroupA_r16 = calloc(1, sizeof(NR_MsgA_PUSCH_Resource_r16_t));
  NR_MsgA_PUSCH_Resource_r16_t *msgA_PUSCH_Resource = msgA_PUSCH_Config_r16->msgA_PUSCH_ResourceGroupA_r16;
  msgA_PUSCH_Resource->startSymbolAndLengthMsgA_PO_r16 = calloc(1, sizeof(long));
  msgA_PUSCH_Config_r16->msgA_TransformPrecoder_r16 = calloc(1, sizeof(long));
}

// Section 4.1 in 38.213
NR_ServingCellConfigCommon__ssb_PositionsInBurst_PR get_ssb_len(NR_ServingCellConfigCommon_t *scc)
{
  NR_FrequencyInfoDL_t *frequencyInfoDL = scc->downlinkConfigCommon->frequencyInfoDL;
  int scs = *scc->ssbSubcarrierSpacing;
  int nr_band = *frequencyInfoDL->frequencyBandList.list.array[0];
  long freq = from_nrarfcn(nr_band, scs, frequencyInfoDL->absoluteFrequencyPointA);
  frame_type_t frame_type = get_frame_type(nr_band, scs);
  if (scs == 0) {
    if (freq < 3000000000)
      return NR_ServingCellConfigCommon__ssb_PositionsInBurst_PR_shortBitmap;
    else
      return NR_ServingCellConfigCommon__ssb_PositionsInBurst_PR_mediumBitmap;
  }
  if (scs == 1) {
    if (nr_band == 5 || nr_band == 24 || nr_band == 66 || frame_type == FDD) { // case B or paired spectrum
      if (freq < 3000000000)
        return NR_ServingCellConfigCommon__ssb_PositionsInBurst_PR_shortBitmap;
      else
        return NR_ServingCellConfigCommon__ssb_PositionsInBurst_PR_mediumBitmap;
    }
    else {  // case C and unpaired spectrum
      if (freq < 1880000000)
        return NR_ServingCellConfigCommon__ssb_PositionsInBurst_PR_shortBitmap;
      else
        return NR_ServingCellConfigCommon__ssb_PositionsInBurst_PR_mediumBitmap;
    }
  }
  // FR2
  return NR_ServingCellConfigCommon__ssb_PositionsInBurst_PR_longBitmap;
}

static struct NR_SCS_SpecificCarrier configure_scs_carrier(int mu, int N_RB)
{
  struct NR_SCS_SpecificCarrier scs_sc = {0};
  scs_sc.offsetToCarrier = 0;
  scs_sc.subcarrierSpacing = mu;
  scs_sc.carrierBandwidth = N_RB;
  return scs_sc;
}

static struct NR_PUSCH_TimeDomainResourceAllocation *add_PUSCH_TimeDomainResourceAllocation(int startSymbolAndLength)
{
  struct NR_PUSCH_TimeDomainResourceAllocation *pusch_alloc = calloc_or_fail(1, sizeof(*pusch_alloc));
  pusch_alloc->k2 = calloc_or_fail(1, sizeof(*pusch_alloc->k2));
  *pusch_alloc->k2 = 6;
  pusch_alloc->mappingType = NR_PUSCH_TimeDomainResourceAllocation__mappingType_typeB;
  pusch_alloc->startSymbolAndLength = startSymbolAndLength;
  return pusch_alloc;
}

/**
 * Fill ServingCellConfigCommon struct members for unitary simulators
 */
void fill_scc_sim(NR_ServingCellConfigCommon_t *scc, uint64_t *ssb_bitmap, int N_RB_DL, int N_RB_UL, int mu_dl, int mu_ul)
{
  *scc->physCellId = 0;
  *scc->ssb_periodicityServingCell = NR_ServingCellConfigCommon__ssb_periodicityServingCell_ms20;
  scc->dmrs_TypeA_Position = NR_ServingCellConfigCommon__dmrs_TypeA_Position_pos2;
  *scc->ssbSubcarrierSpacing = mu_dl;

  struct NR_FrequencyInfoDL *frequencyInfoDL = scc->downlinkConfigCommon->frequencyInfoDL;
  if (mu_dl == 0) {
    *frequencyInfoDL->absoluteFrequencySSB = 520432;
    *frequencyInfoDL->frequencyBandList.list.array[0] = 38;
    frequencyInfoDL->absoluteFrequencyPointA = 520000;
  } else {
    *frequencyInfoDL->absoluteFrequencySSB = 641032;
    *frequencyInfoDL->frequencyBandList.list.array[0] = 78;
    frequencyInfoDL->absoluteFrequencyPointA = 640000;
  }

  *frequencyInfoDL->scs_SpecificCarrierList.list.array[0] = configure_scs_carrier(mu_dl, N_RB_DL);
  struct NR_BWP_DownlinkCommon *initialDownlinkBWP = scc->downlinkConfigCommon->initialDownlinkBWP;

  initialDownlinkBWP->genericParameters.locationAndBandwidth = PRBalloc_to_locationandbandwidth(N_RB_DL, 0);
  initialDownlinkBWP->genericParameters.subcarrierSpacing = mu_dl;
  *initialDownlinkBWP->pdcch_ConfigCommon->choice.setup->controlResourceSetZero = 12;
  *initialDownlinkBWP->pdcch_ConfigCommon->choice.setup->searchSpaceZero = 0;
  struct NR_PDSCH_TimeDomainResourceAllocation *timedomainresourceallocation0 =
      calloc_or_fail(1, sizeof(*timedomainresourceallocation0));
  timedomainresourceallocation0->mappingType=NR_PDSCH_TimeDomainResourceAllocation__mappingType_typeA;
  timedomainresourceallocation0->startSymbolAndLength = 54;
  asn1cSeqAdd(&initialDownlinkBWP->pdsch_ConfigCommon->choice.setup->pdsch_TimeDomainAllocationList->list,
              timedomainresourceallocation0);
  struct NR_PDSCH_TimeDomainResourceAllocation *timedomainresourceallocation1 =
      calloc_or_fail(1, sizeof(*timedomainresourceallocation1));
  timedomainresourceallocation1->mappingType = NR_PDSCH_TimeDomainResourceAllocation__mappingType_typeA;
  timedomainresourceallocation1->startSymbolAndLength = 57;
  asn1cSeqAdd(&initialDownlinkBWP->pdsch_ConfigCommon->choice.setup->pdsch_TimeDomainAllocationList->list,
              timedomainresourceallocation1);

  struct NR_FrequencyInfoUL *frequencyInfoUL = scc->uplinkConfigCommon->frequencyInfoUL;
  *frequencyInfoUL->frequencyBandList->list.array[0] = mu_ul ? 78 : 38;
  *frequencyInfoUL->absoluteFrequencyPointA = -1;
  *frequencyInfoUL->scs_SpecificCarrierList.list.array[0] = configure_scs_carrier(mu_ul, N_RB_UL);
  *frequencyInfoUL->p_Max = 20;

  struct NR_BWP_UplinkCommon *initialUplinkBWP = scc->uplinkConfigCommon->initialUplinkBWP;
  initialUplinkBWP->genericParameters.locationAndBandwidth = PRBalloc_to_locationandbandwidth(N_RB_UL, 0);
  initialUplinkBWP->genericParameters.subcarrierSpacing = mu_ul;

  struct NR_SetupRelease_RACH_ConfigCommon *rach_ConfigCommon = initialUplinkBWP->rach_ConfigCommon;
  rach_ConfigCommon->choice.setup->rach_ConfigGeneric.prach_ConfigurationIndex = 98;
  rach_ConfigCommon->choice.setup->rach_ConfigGeneric.msg1_FDM = NR_RACH_ConfigGeneric__msg1_FDM_one;
  rach_ConfigCommon->choice.setup->rach_ConfigGeneric.msg1_FrequencyStart = 0;
  rach_ConfigCommon->choice.setup->rach_ConfigGeneric.zeroCorrelationZoneConfig = 13;
  rach_ConfigCommon->choice.setup->rach_ConfigGeneric.preambleReceivedTargetPower = -118;
  rach_ConfigCommon->choice.setup->rach_ConfigGeneric.preambleTransMax = NR_RACH_ConfigGeneric__preambleTransMax_n10;
  rach_ConfigCommon->choice.setup->rach_ConfigGeneric.powerRampingStep = NR_RACH_ConfigGeneric__powerRampingStep_dB2;
  rach_ConfigCommon->choice.setup->rach_ConfigGeneric.ra_ResponseWindow = NR_RACH_ConfigGeneric__ra_ResponseWindow_sl20;
  rach_ConfigCommon->choice.setup->ssb_perRACH_OccasionAndCB_PreamblesPerSSB->present =
      NR_RACH_ConfigCommon__ssb_perRACH_OccasionAndCB_PreamblesPerSSB_PR_one;
  rach_ConfigCommon->choice.setup->ssb_perRACH_OccasionAndCB_PreamblesPerSSB->choice.one =
      NR_RACH_ConfigCommon__ssb_perRACH_OccasionAndCB_PreamblesPerSSB__one_n64;
  rach_ConfigCommon->choice.setup->ra_ContentionResolutionTimer = NR_RACH_ConfigCommon__ra_ContentionResolutionTimer_sf64;
  *rach_ConfigCommon->choice.setup->rsrp_ThresholdSSB = 19;
  rach_ConfigCommon->choice.setup->prach_RootSequenceIndex.present = NR_RACH_ConfigCommon__prach_RootSequenceIndex_PR_l139;
  rach_ConfigCommon->choice.setup->prach_RootSequenceIndex.choice.l139 = 0;
  rach_ConfigCommon->choice.setup->restrictedSetConfig = NR_RACH_ConfigCommon__restrictedSetConfig_unrestrictedSet;
  *rach_ConfigCommon->choice.setup->msg1_SubcarrierSpacing = mu_ul;
  struct NR_SetupRelease_PUSCH_ConfigCommon *pusch_ConfigCommon = initialUplinkBWP->pusch_ConfigCommon;

  asn1cSeqAdd(&pusch_ConfigCommon->choice.setup->pusch_TimeDomainAllocationList->list, add_PUSCH_TimeDomainResourceAllocation(55));
  asn1cSeqAdd(&pusch_ConfigCommon->choice.setup->pusch_TimeDomainAllocationList->list, add_PUSCH_TimeDomainResourceAllocation(38));

  *pusch_ConfigCommon->choice.setup->msg3_DeltaPreamble = 1;
  *pusch_ConfigCommon->choice.setup->p0_NominalWithGrant = -90;
  struct NR_SetupRelease_PUCCH_ConfigCommon *pucch_ConfigCommon = initialUplinkBWP->pucch_ConfigCommon;
  pucch_ConfigCommon->choice.setup->pucch_GroupHopping = NR_PUCCH_ConfigCommon__pucch_GroupHopping_neither;
  *pucch_ConfigCommon->choice.setup->hoppingId = 40;
  *pucch_ConfigCommon->choice.setup->p0_nominal = -90;
  scc->ssb_PositionsInBurst->present = NR_ServingCellConfigCommon__ssb_PositionsInBurst_PR_mediumBitmap;
  *ssb_bitmap = 0xff;

  struct NR_TDD_UL_DL_ConfigCommon *tdd_UL_DL_ConfigurationCommon = scc->tdd_UL_DL_ConfigurationCommon;
  tdd_UL_DL_ConfigurationCommon->referenceSubcarrierSpacing = mu_dl;

  NR_TDD_UL_DL_Pattern_t *p1 = &tdd_UL_DL_ConfigurationCommon->pattern1;
  p1->dl_UL_TransmissionPeriodicity = (mu_dl == 0) ? NR_TDD_UL_DL_Pattern__dl_UL_TransmissionPeriodicity_ms10
                                                   : NR_TDD_UL_DL_Pattern__dl_UL_TransmissionPeriodicity_ms5;
  p1->nrofDownlinkSlots = 7;
  p1->nrofDownlinkSymbols = 6;
  p1->nrofUplinkSlots = 2;
  p1->nrofUplinkSymbols = 4;

  struct NR_TDD_UL_DL_Pattern *p2 = tdd_UL_DL_ConfigurationCommon->pattern2;
  p2->dl_UL_TransmissionPeriodicity = 321;
  p2->nrofDownlinkSlots = -1;
  p2->nrofDownlinkSymbols = -1;
  p2->nrofUplinkSlots = -1;
  p2->nrofUplinkSymbols = -1;

  scc->ss_PBCH_BlockPower = 20;
}


void fix_scc(NR_ServingCellConfigCommon_t *scc, uint64_t ssbmap)
{
  scc->ssb_PositionsInBurst->present = get_ssb_len(scc);
  uint8_t curr_bit;

  // changing endianicity of ssbmap and filling the ssb_PositionsInBurst buffers
  if(scc->ssb_PositionsInBurst->present == NR_ServingCellConfigCommon__ssb_PositionsInBurst_PR_shortBitmap) {
    scc->ssb_PositionsInBurst->choice.shortBitmap.size = 1;
    scc->ssb_PositionsInBurst->choice.shortBitmap.bits_unused = 4;
    scc->ssb_PositionsInBurst->choice.shortBitmap.buf = CALLOC(1, 1);
    scc->ssb_PositionsInBurst->choice.shortBitmap.buf[0] = 0;
    for (int i = 0; i < 8; i++) {
      if (i<scc->ssb_PositionsInBurst->choice.shortBitmap.bits_unused)
        curr_bit = 0;
      else
        curr_bit = (ssbmap >> (7 - i)) & 0x01;
      scc->ssb_PositionsInBurst->choice.shortBitmap.buf[0] |= curr_bit << i;
    }
  } else if(scc->ssb_PositionsInBurst->present == NR_ServingCellConfigCommon__ssb_PositionsInBurst_PR_mediumBitmap) {
    scc->ssb_PositionsInBurst->choice.mediumBitmap.size = 1;
    scc->ssb_PositionsInBurst->choice.mediumBitmap.bits_unused = 0;
    scc->ssb_PositionsInBurst->choice.mediumBitmap.buf = CALLOC(1, 1);
    scc->ssb_PositionsInBurst->choice.mediumBitmap.buf[0] = 0;
    for (int i = 0; i < 8; i++)
      scc->ssb_PositionsInBurst->choice.mediumBitmap.buf[0] |= (((ssbmap >> (7 - i)) & 0x01) << i);
  } else {
    scc->ssb_PositionsInBurst->choice.longBitmap.size = 8;
    scc->ssb_PositionsInBurst->choice.longBitmap.bits_unused = 0;
    scc->ssb_PositionsInBurst->choice.longBitmap.buf = CALLOC(1, 8);
    for (int j = 0; j < 8; j++) {
       scc->ssb_PositionsInBurst->choice.longBitmap.buf[j] = 0;
       curr_bit = (ssbmap >> (j << 3)) & 0xff;
       for (int i = 0; i < 8; i++)
         scc->ssb_PositionsInBurst->choice.longBitmap.buf[j] |= (((curr_bit >> (7 - i)) & 0x01) << i);
    }
  }

  // fix SS0 and Coreset0
  NR_PDCCH_ConfigCommon_t *pdcch_cc = scc->downlinkConfigCommon->initialDownlinkBWP->pdcch_ConfigCommon->choice.setup;
  if((int)*pdcch_cc->searchSpaceZero == -1) {
    free(pdcch_cc->searchSpaceZero);
    pdcch_cc->searchSpaceZero = NULL;
  }
  if((int)*pdcch_cc->controlResourceSetZero == -1) {
    free(pdcch_cc->controlResourceSetZero);
    pdcch_cc->controlResourceSetZero = NULL;
  }

  // fix UL absolute frequency
  if ((int)*scc->uplinkConfigCommon->frequencyInfoUL->absoluteFrequencyPointA==-1) {
     free(scc->uplinkConfigCommon->frequencyInfoUL->absoluteFrequencyPointA);
     scc->uplinkConfigCommon->frequencyInfoUL->absoluteFrequencyPointA = NULL;
  }

  // default value for msg3 precoder is NULL (0 means enabled)
  if (*scc->uplinkConfigCommon->initialUplinkBWP->rach_ConfigCommon->choice.setup->msg3_transformPrecoder!=0)
    scc->uplinkConfigCommon->initialUplinkBWP->rach_ConfigCommon->choice.setup->msg3_transformPrecoder = NULL;

  frame_type_t frame_type = get_frame_type((int)*scc->downlinkConfigCommon->frequencyInfoDL->frequencyBandList.list.array[0], *scc->ssbSubcarrierSpacing);

  // prepare DL Allocation lists
  nr_rrc_config_dl_tda(scc->downlinkConfigCommon->initialDownlinkBWP->pdsch_ConfigCommon->choice.setup->pdsch_TimeDomainAllocationList,
                       frame_type, scc->tdd_UL_DL_ConfigurationCommon,
                       scc->downlinkConfigCommon->frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth);

  if (frame_type == FDD) {
    ASN_STRUCT_FREE(asn_DEF_NR_TDD_UL_DL_ConfigCommon, scc->tdd_UL_DL_ConfigurationCommon);
    scc->tdd_UL_DL_ConfigurationCommon = NULL;
  } else { // TDD
    if (scc->tdd_UL_DL_ConfigurationCommon->pattern2->dl_UL_TransmissionPeriodicity > 320 ) {
      free(scc->tdd_UL_DL_ConfigurationCommon->pattern2);
      scc->tdd_UL_DL_ConfigurationCommon->pattern2 = NULL;
    }

  }

  if ((int)*scc->uplinkConfigCommon->initialUplinkBWP->rach_ConfigCommon->choice.setup->msg1_SubcarrierSpacing == -1) {
    free(scc->uplinkConfigCommon->initialUplinkBWP->rach_ConfigCommon->choice.setup->msg1_SubcarrierSpacing);
    scc->uplinkConfigCommon->initialUplinkBWP->rach_ConfigCommon->choice.setup->msg1_SubcarrierSpacing=NULL;
  }

  if (scc->uplinkConfigCommon->initialUplinkBWP->ext1
      && (int)scc->uplinkConfigCommon->initialUplinkBWP->ext1->msgA_ConfigCommon_r16->choice.setup->msgA_PUSCH_Config_r16
                 ->msgA_PUSCH_ResourceGroupA_r16->msgA_PUSCH_TimeDomainOffset_r16
             == 0) {
    free(scc->uplinkConfigCommon->initialUplinkBWP->ext1);
    scc->uplinkConfigCommon->initialUplinkBWP->ext1 = NULL;
  }

  if ((int)*scc->n_TimingAdvanceOffset == -1) {
    free(scc->n_TimingAdvanceOffset);
    scc->n_TimingAdvanceOffset = NULL;
  }

  // check pucch_ResourceConfig
  AssertFatal(*scc->uplinkConfigCommon->initialUplinkBWP->pucch_ConfigCommon->choice.setup->pucch_ResourceCommon < 2,
	      "pucch_ResourceConfig should be 0 or 1 for now\n");

  if (*scc->ext2->ntn_Config_r17->cellSpecificKoffset_r17 == 0) {
    free(scc->ext2->ntn_Config_r17->cellSpecificKoffset_r17);
    scc->ext2->ntn_Config_r17->cellSpecificKoffset_r17 = NULL;
  }
  if (scc->ext2->ntn_Config_r17->ta_Info_r17->ta_Common_r17 == -1) {
    free(scc->ext2->ntn_Config_r17->ta_Info_r17);
    scc->ext2->ntn_Config_r17->ta_Info_r17 = NULL;
  }
  if (scc->ext2->ntn_Config_r17->ephemerisInfo_r17->choice.positionVelocity_r17->positionX_r17 == LONG_MAX
      && scc->ext2->ntn_Config_r17->ephemerisInfo_r17->choice.positionVelocity_r17->positionY_r17 == LONG_MAX
      && scc->ext2->ntn_Config_r17->ephemerisInfo_r17->choice.positionVelocity_r17->positionZ_r17 == LONG_MAX
      && scc->ext2->ntn_Config_r17->ephemerisInfo_r17->choice.positionVelocity_r17->velocityVX_r17 == LONG_MAX
      && scc->ext2->ntn_Config_r17->ephemerisInfo_r17->choice.positionVelocity_r17->velocityVY_r17 == LONG_MAX
      && scc->ext2->ntn_Config_r17->ephemerisInfo_r17->choice.positionVelocity_r17->velocityVZ_r17 == LONG_MAX) {
    free(scc->ext2->ntn_Config_r17->ephemerisInfo_r17->choice.positionVelocity_r17);
    free(scc->ext2->ntn_Config_r17->ephemerisInfo_r17);
    scc->ext2->ntn_Config_r17->ephemerisInfo_r17 = NULL;
  }

  if (!scc->ext2->ntn_Config_r17->cellSpecificKoffset_r17 && !scc->ext2->ntn_Config_r17->ta_Info_r17
      && !scc->ext2->ntn_Config_r17->ephemerisInfo_r17) {
    free(scc->ext2->ntn_Config_r17);
    free(scc->ext2);
    scc->ext2 = NULL;
  }
}

/* Function to allocate dedicated serving cell config strutures */
void prepare_scd(NR_ServingCellConfig_t *scd) {
  // Allocate downlink structures
  scd->downlinkBWP_ToAddModList = calloc_or_fail(1, sizeof(*scd->downlinkBWP_ToAddModList));
  scd->uplinkConfig = calloc_or_fail(1, sizeof(*scd->uplinkConfig));
  scd->uplinkConfig->uplinkBWP_ToAddModList = calloc_or_fail(1, sizeof(*scd->uplinkConfig->uplinkBWP_ToAddModList));
  scd->bwp_InactivityTimer = calloc_or_fail(1, sizeof(*scd->bwp_InactivityTimer));
  scd->uplinkConfig->firstActiveUplinkBWP_Id = calloc_or_fail(1, sizeof(*scd->uplinkConfig->firstActiveUplinkBWP_Id));
  scd->firstActiveDownlinkBWP_Id = calloc_or_fail(1, sizeof(*scd->firstActiveDownlinkBWP_Id));
  *scd->firstActiveDownlinkBWP_Id = 1;
  *scd->uplinkConfig->firstActiveUplinkBWP_Id = 1;
  scd->defaultDownlinkBWP_Id = calloc_or_fail(1, sizeof(*scd->defaultDownlinkBWP_Id));
  *scd->defaultDownlinkBWP_Id = 0;

  for (int j = 0; j < NR_MAX_NUM_BWP; j++) {

    // Downlink bandwidth part
    NR_BWP_Downlink_t *bwp = calloc_or_fail(1, sizeof(*bwp));
    bwp->bwp_Id = j+1;

    // Allocate downlink dedicated bandwidth part and PDSCH structures
    bwp->bwp_Common = calloc_or_fail(1, sizeof(*bwp->bwp_Common));
    bwp->bwp_Common->pdcch_ConfigCommon = calloc_or_fail(1, sizeof(*bwp->bwp_Common->pdcch_ConfigCommon));
    bwp->bwp_Common->pdsch_ConfigCommon = calloc_or_fail(1, sizeof(*bwp->bwp_Common->pdsch_ConfigCommon));
    bwp->bwp_Dedicated = calloc_or_fail(1, sizeof(*bwp->bwp_Dedicated));
    bwp->bwp_Dedicated->pdsch_Config = calloc_or_fail(1, sizeof(*bwp->bwp_Dedicated->pdsch_Config));
    struct NR_SetupRelease_PDSCH_Config *pdsch_Config = bwp->bwp_Dedicated->pdsch_Config;
    pdsch_Config->present = NR_SetupRelease_PDSCH_Config_PR_setup;
    pdsch_Config->choice.setup = calloc_or_fail(1, sizeof(*pdsch_Config->choice.setup));
    struct NR_PDSCH_Config *pc_setup = pdsch_Config->choice.setup;
    pc_setup->dmrs_DownlinkForPDSCH_MappingTypeA = calloc_or_fail(1, sizeof(*pc_setup->dmrs_DownlinkForPDSCH_MappingTypeA));
    struct NR_SetupRelease_DMRS_DownlinkConfig *typeA = pc_setup->dmrs_DownlinkForPDSCH_MappingTypeA;
    typeA->present = NR_SetupRelease_DMRS_DownlinkConfig_PR_setup;

    // Allocate DL DMRS and PTRS configuration
    typeA->choice.setup = calloc_or_fail(1, sizeof(*typeA->choice.setup));
    NR_DMRS_DownlinkConfig_t *NR_DMRS_DownlinkCfg = typeA->choice.setup;
    NR_DMRS_DownlinkCfg->phaseTrackingRS = calloc_or_fail(1, sizeof(*NR_DMRS_DownlinkCfg->phaseTrackingRS));
    NR_DMRS_DownlinkCfg->phaseTrackingRS->present = NR_SetupRelease_PTRS_DownlinkConfig_PR_setup;
    NR_DMRS_DownlinkCfg->phaseTrackingRS->choice.setup =
        calloc_or_fail(1, sizeof(*NR_DMRS_DownlinkCfg->phaseTrackingRS->choice.setup));
    NR_PTRS_DownlinkConfig_t *NR_PTRS_DownlinkCfg = NR_DMRS_DownlinkCfg->phaseTrackingRS->choice.setup;
    NR_PTRS_DownlinkCfg->frequencyDensity = calloc_or_fail(1, sizeof(*NR_PTRS_DownlinkCfg->frequencyDensity));
    long *dl_rbs = calloc_or_fail(2, sizeof(*dl_rbs));
    for (int i=0;i<2;i++) {
      asn1cSeqAdd(&NR_PTRS_DownlinkCfg->frequencyDensity->list, &dl_rbs[i]);
    }
    NR_PTRS_DownlinkCfg->timeDensity = calloc_or_fail(1, sizeof(*NR_PTRS_DownlinkCfg->timeDensity));
    long *dl_mcs = calloc_or_fail(3, sizeof(*dl_mcs));
    for (int i=0;i<3;i++) {
      asn1cSeqAdd(&NR_PTRS_DownlinkCfg->timeDensity->list, &dl_mcs[i]);
    }
    NR_PTRS_DownlinkCfg->epre_Ratio = calloc_or_fail(1, sizeof(*NR_PTRS_DownlinkCfg->epre_Ratio));
    NR_PTRS_DownlinkCfg->resourceElementOffset = calloc_or_fail(1, sizeof(*NR_PTRS_DownlinkCfg->resourceElementOffset));
    *NR_PTRS_DownlinkCfg->resourceElementOffset = 0;
    asn1cSeqAdd(&scd->downlinkBWP_ToAddModList->list,bwp);

    // Allocate uplink structures

    NR_PUSCH_Config_t *pusch_Config = calloc_or_fail(1, sizeof(*pusch_Config));

    // Allocate UL DMRS and PTRS structures
    pusch_Config->dmrs_UplinkForPUSCH_MappingTypeB = calloc_or_fail(1, sizeof(*pusch_Config->dmrs_UplinkForPUSCH_MappingTypeB));
    pusch_Config->dmrs_UplinkForPUSCH_MappingTypeB->present = NR_SetupRelease_DMRS_UplinkConfig_PR_setup;
    pusch_Config->dmrs_UplinkForPUSCH_MappingTypeB->choice.setup =
        calloc_or_fail(1, sizeof(*pusch_Config->dmrs_UplinkForPUSCH_MappingTypeB->choice.setup));
    NR_DMRS_UplinkConfig_t *NR_DMRS_UplinkConfig = pusch_Config->dmrs_UplinkForPUSCH_MappingTypeB->choice.setup;
    NR_DMRS_UplinkConfig->phaseTrackingRS = calloc_or_fail(1, sizeof(*NR_DMRS_UplinkConfig->phaseTrackingRS));
    NR_DMRS_UplinkConfig->phaseTrackingRS->present = NR_SetupRelease_PTRS_UplinkConfig_PR_setup;
    NR_DMRS_UplinkConfig->phaseTrackingRS->choice.setup =
        calloc_or_fail(1, sizeof(*NR_DMRS_UplinkConfig->phaseTrackingRS->choice.setup));
    NR_PTRS_UplinkConfig_t *NR_PTRS_UplinkConfig = NR_DMRS_UplinkConfig->phaseTrackingRS->choice.setup;
    NR_PTRS_UplinkConfig->transformPrecoderDisabled = calloc_or_fail(1, sizeof(*NR_PTRS_UplinkConfig->transformPrecoderDisabled));
    NR_PTRS_UplinkConfig->transformPrecoderDisabled->frequencyDensity =
        calloc_or_fail(1, sizeof(*NR_PTRS_UplinkConfig->transformPrecoderDisabled->frequencyDensity));
    long *n_rbs = calloc_or_fail(2, sizeof(*n_rbs));
    for (int i=0;i<2;i++) {
      asn1cSeqAdd(&NR_PTRS_UplinkConfig->transformPrecoderDisabled->frequencyDensity->list, &n_rbs[i]);
    }
    NR_PTRS_UplinkConfig->transformPrecoderDisabled->timeDensity =
        calloc_or_fail(1, sizeof(*NR_PTRS_UplinkConfig->transformPrecoderDisabled->timeDensity));
    long *ptrs_mcs = calloc_or_fail(3, sizeof(*ptrs_mcs));
    for (int i = 0; i < 3; i++) {
      asn1cSeqAdd(&NR_PTRS_UplinkConfig->transformPrecoderDisabled->timeDensity->list, &ptrs_mcs[i]);
    }
    NR_PTRS_UplinkConfig->transformPrecoderDisabled->resourceElementOffset =
        calloc_or_fail(1, sizeof(*NR_PTRS_UplinkConfig->transformPrecoderDisabled->resourceElementOffset));
    *NR_PTRS_UplinkConfig->transformPrecoderDisabled->resourceElementOffset = 0;

    // UL bandwidth part
    NR_BWP_Uplink_t *ubwp = calloc_or_fail(1, sizeof(*ubwp));
    ubwp->bwp_Id = j+1;
    ubwp->bwp_Common = calloc_or_fail(1, sizeof(*ubwp->bwp_Common));
    ubwp->bwp_Dedicated = calloc_or_fail(1, sizeof(*ubwp->bwp_Dedicated));

    ubwp->bwp_Dedicated->pusch_Config = calloc_or_fail(1, sizeof(*ubwp->bwp_Dedicated->pusch_Config));
    ubwp->bwp_Dedicated->pusch_Config->present = NR_SetupRelease_PUSCH_Config_PR_setup;
    ubwp->bwp_Dedicated->pusch_Config->choice.setup = pusch_Config;

    asn1cSeqAdd(&scd->uplinkConfig->uplinkBWP_ToAddModList->list,ubwp);
  }
}

/* This function checks dedicated serving cell configuration and performs fixes as needed */
void fix_scd(NR_ServingCellConfig_t *scd) {

  // Remove unused BWPs
  int b = 0;
  while (b<scd->downlinkBWP_ToAddModList->list.count) {
    if (scd->downlinkBWP_ToAddModList->list.array[b]->bwp_Common->genericParameters.locationAndBandwidth == 0) {
      asn_sequence_del(&scd->downlinkBWP_ToAddModList->list,b,1);
    } else {
      b++;
    }
  }

  b = 0;
  while (b<scd->uplinkConfig->uplinkBWP_ToAddModList->list.count) {
    if (scd->uplinkConfig->uplinkBWP_ToAddModList->list.array[b]->bwp_Common->genericParameters.locationAndBandwidth == 0) {
      asn_sequence_del(&scd->uplinkConfig->uplinkBWP_ToAddModList->list,b,1);
    } else {
      b++;
    }
  }

  // Check for DL PTRS parameters validity
  for (int bwp_i = 0 ; bwp_i<scd->downlinkBWP_ToAddModList->list.count; bwp_i++) {

    NR_DMRS_DownlinkConfig_t *dmrs_dl_config = scd->downlinkBWP_ToAddModList->list.array[bwp_i]->bwp_Dedicated->pdsch_Config->choice.setup->dmrs_DownlinkForPDSCH_MappingTypeA->choice.setup;
    
    if (dmrs_dl_config->phaseTrackingRS) {
      // If any of the frequencyDensity values are not set or are out of bounds, PTRS is assumed to be not present
      for (int i = dmrs_dl_config->phaseTrackingRS->choice.setup->frequencyDensity->list.count - 1; i >= 0; i--) {
        if ((*dmrs_dl_config->phaseTrackingRS->choice.setup->frequencyDensity->list.array[i] < 1)
            || (*dmrs_dl_config->phaseTrackingRS->choice.setup->frequencyDensity->list.array[i] > 276)) {
          LOG_I(GNB_APP, "DL PTRS frequencyDensity %d not set. Assuming PTRS not present! \n", i);
          free(dmrs_dl_config->phaseTrackingRS);
          dmrs_dl_config->phaseTrackingRS = NULL;
          break;
        }
      }
    }

    if (dmrs_dl_config->phaseTrackingRS) {
      // If any of the timeDensity values are not set or are out of bounds, PTRS is assumed to be not present
      for (int i = dmrs_dl_config->phaseTrackingRS->choice.setup->timeDensity->list.count - 1; i >= 0; i--) {
        if ((*dmrs_dl_config->phaseTrackingRS->choice.setup->timeDensity->list.array[i] < 0)
            || (*dmrs_dl_config->phaseTrackingRS->choice.setup->timeDensity->list.array[i] > 29)) {
          LOG_I(GNB_APP, "DL PTRS timeDensity %d not set. Assuming PTRS not present! \n", i);
          free(dmrs_dl_config->phaseTrackingRS);
          dmrs_dl_config->phaseTrackingRS = NULL;
          break;
        }
      }
    }

    if (dmrs_dl_config->phaseTrackingRS) {
      if (*dmrs_dl_config->phaseTrackingRS->choice.setup->resourceElementOffset > 2) {
        LOG_I(GNB_APP, "Freeing DL PTRS resourceElementOffset \n");
        free(dmrs_dl_config->phaseTrackingRS->choice.setup->resourceElementOffset);
        dmrs_dl_config->phaseTrackingRS->choice.setup->resourceElementOffset = NULL;
      }
      if (*dmrs_dl_config->phaseTrackingRS->choice.setup->epre_Ratio > 1) {
        LOG_I(GNB_APP, "Freeing DL PTRS epre_Ratio \n");
        free(dmrs_dl_config->phaseTrackingRS->choice.setup->epre_Ratio);
        dmrs_dl_config->phaseTrackingRS->choice.setup->epre_Ratio = NULL;
      }
    }
  }

  // Check for UL PTRS parameters validity
  for (int bwp_i = 0 ; bwp_i<scd->uplinkConfig->uplinkBWP_ToAddModList->list.count; bwp_i++) {

    NR_DMRS_UplinkConfig_t *dmrs_ul_config = scd->uplinkConfig->uplinkBWP_ToAddModList->list.array[bwp_i]->bwp_Dedicated->pusch_Config->choice.setup->dmrs_UplinkForPUSCH_MappingTypeB->choice.setup;
    
    if (dmrs_ul_config->phaseTrackingRS) {
      // If any of the frequencyDensity values are not set or are out of bounds, PTRS is assumed to be not present
      for (int i = dmrs_ul_config->phaseTrackingRS->choice.setup->transformPrecoderDisabled->frequencyDensity->list.count-1; i >= 0; i--) {
        if ((*dmrs_ul_config->phaseTrackingRS->choice.setup->transformPrecoderDisabled->frequencyDensity->list.array[i] < 1)
            || (*dmrs_ul_config->phaseTrackingRS->choice.setup->transformPrecoderDisabled->frequencyDensity->list.array[i] > 276)) {
          LOG_I(GNB_APP, "UL PTRS frequencyDensity %d not set. Assuming PTRS not present! \n", i);
          free(dmrs_ul_config->phaseTrackingRS);
          dmrs_ul_config->phaseTrackingRS = NULL;
          break;
        }
      }
    }

    if (dmrs_ul_config->phaseTrackingRS) {
      // If any of the timeDensity values are not set or are out of bounds, PTRS is assumed to be not present
      for (int i = dmrs_ul_config->phaseTrackingRS->choice.setup->transformPrecoderDisabled->timeDensity->list.count-1; i >= 0; i--) {
        if ((*dmrs_ul_config->phaseTrackingRS->choice.setup->transformPrecoderDisabled->timeDensity->list.array[i] < 0)
            || (*dmrs_ul_config->phaseTrackingRS->choice.setup->transformPrecoderDisabled->timeDensity->list.array[i] > 29)) {
          LOG_I(GNB_APP, "UL PTRS timeDensity %d not set. Assuming PTRS not present! \n", i);
          free(dmrs_ul_config->phaseTrackingRS);
          dmrs_ul_config->phaseTrackingRS = NULL;
          break;
        }
      }
    }

    if (dmrs_ul_config->phaseTrackingRS) {
      // Check for UL PTRS parameters validity
      if (*dmrs_ul_config->phaseTrackingRS->choice.setup->transformPrecoderDisabled->resourceElementOffset > 2) {
        LOG_I(GNB_APP, "Freeing UL PTRS resourceElementOffset \n");
        free(dmrs_ul_config->phaseTrackingRS->choice.setup->transformPrecoderDisabled->resourceElementOffset);
        dmrs_ul_config->phaseTrackingRS->choice.setup->transformPrecoderDisabled->resourceElementOffset = NULL;
      }
    }

  }

  if (scd->downlinkBWP_ToAddModList->list.count == 0) {
    free(scd->downlinkBWP_ToAddModList);
    scd->downlinkBWP_ToAddModList = NULL;
  }

  if (scd->uplinkConfig->uplinkBWP_ToAddModList->list.count == 0) {
    free(scd->uplinkConfig->uplinkBWP_ToAddModList);
    scd->uplinkConfig->uplinkBWP_ToAddModList = NULL;
  }
}

static void verify_gnb_param_notset(paramdef_t *params, int paramidx, const char *paramname)
{
  char aprefix[MAX_OPTNAME_SIZE * 2 + 8];
  sprintf(aprefix, "%s.[%i]", GNB_CONFIG_STRING_GNB_LIST, 0);
  AssertFatal(!config_isparamset(params, paramidx),
              "Option \"%s." GNB_CONFIG_STRING_SERVINGCELLCONFIGCOMMON
              ".%s\" is not allowed in this config, please remove it\n",
              aprefix,
              paramname);
}
static void verify_section_notset(configmodule_interface_t *cfg, char *aprefix, const char *secname)
{
  paramlist_def_t pl = {0};
  strncpy(pl.listname, secname, sizeof(pl.listname) - 1);
  config_getlist(cfg, &pl, NULL, 0, aprefix);
  AssertFatal(pl.numelt == 0, "Section \"%s.%s\" not allowed in this config, please remove it\n", aprefix ? aprefix : "", secname);
}
void RCconfig_verify(configmodule_interface_t *cfg, ngran_node_t node_type)
{
  paramdef_t GNBSParams[] = GNBSPARAMS_DESC;
  config_get(cfg, GNBSParams, sizeof(GNBSParams) / sizeof(paramdef_t), NULL);
  int num_gnbs = GNBSParams[GNB_ACTIVE_GNBS_IDX].numelt;
  AssertFatal(num_gnbs == 1, "need to have a " GNB_CONFIG_STRING_GNB_LIST " section, but %d found\n", num_gnbs);
  paramlist_def_t GNBParamList = {GNB_CONFIG_STRING_GNB_LIST, NULL, 0};
  paramdef_t GNBParams[] = GNBPARAMS_DESC;
  config_getlist(cfg, &GNBParamList, GNBParams, sizeof(GNBParams) / sizeof(paramdef_t), NULL);
  paramdef_t *gnbp = GNBParamList.paramarray[0];

  if (NODE_IS_CU(node_type)) {
    // verify that there is no SCC and radio config in the case of CU
    verify_section_notset(cfg, GNB_CONFIG_STRING_GNB_LIST ".[0]", GNB_CONFIG_STRING_SERVINGCELLCONFIGCOMMON);

    verify_gnb_param_notset(gnbp, GNB_PDSCH_ANTENNAPORTS_N1_IDX, GNB_CONFIG_STRING_PDSCHANTENNAPORTS_N1);
    verify_gnb_param_notset(gnbp, GNB_PDSCH_ANTENNAPORTS_N2_IDX, GNB_CONFIG_STRING_PDSCHANTENNAPORTS_N2);
    verify_gnb_param_notset(gnbp, GNB_PDSCH_ANTENNAPORTS_XP_IDX, GNB_CONFIG_STRING_PDSCHANTENNAPORTS_XP);
    verify_gnb_param_notset(gnbp, GNB_PUSCH_ANTENNAPORTS_IDX, GNB_CONFIG_STRING_PUSCHANTENNAPORTS);
    verify_gnb_param_notset(gnbp, GNB_MINRXTXTIME_IDX, GNB_CONFIG_STRING_MINRXTXTIME);
    verify_gnb_param_notset(gnbp, GNB_SIB1_TDA_IDX, GNB_CONFIG_STRING_SIB1TDA);
    verify_gnb_param_notset(gnbp, GNB_DO_CSIRS_IDX, GNB_CONFIG_STRING_DOCSIRS);
    verify_gnb_param_notset(gnbp, GNB_DO_SRS_IDX, GNB_CONFIG_STRING_DOSRS);
    verify_gnb_param_notset(gnbp, GNB_FORCE256QAMOFF_IDX, GNB_CONFIG_STRING_FORCE256QAMOFF);
    verify_gnb_param_notset(gnbp, GNB_MAXMIMOLAYERS_IDX, GNB_CONFIG_STRING_MAXMIMOLAYERS);
    verify_gnb_param_notset(gnbp, GNB_DISABLE_HARQ_IDX, GNB_CONFIG_STRING_DISABLE_HARQ);
    verify_gnb_param_notset(gnbp, GNB_NUM_DL_HARQ_IDX, GNB_CONFIG_STRING_NUM_DL_HARQPROCESSES);
    verify_gnb_param_notset(gnbp, GNB_NUM_UL_HARQ_IDX, GNB_CONFIG_STRING_NUM_UL_HARQPROCESSES);

    // check for some general sections
    verify_section_notset(cfg, NULL, CONFIG_STRING_L1_LIST);
    verify_section_notset(cfg, NULL, CONFIG_STRING_RU_LIST);
    verify_section_notset(cfg, NULL, CONFIG_STRING_MACRLC_LIST);
  } else if (NODE_IS_DU(node_type)) {
    // verify that there is no bearer config
    verify_gnb_param_notset(gnbp, GNB_ENABLE_SDAP_IDX, GNB_CONFIG_STRING_ENABLE_SDAP);
    verify_gnb_param_notset(gnbp, GNB_DRBS, GNB_CONFIG_STRING_DRBS);

    verify_section_notset(cfg, GNB_CONFIG_STRING_GNB_LIST ".", GNB_CONFIG_STRING_AMF_IP_ADDRESS);
    verify_section_notset(cfg, NULL, CONFIG_STRING_SECURITY);
  } // else nothing to be checked

  /* other possible verifications: PNF, VNF, CU-CP, CU-UP, ...? */
}

void RCconfig_nr_prs(void)
{
  uint16_t  j = 0, k = 0;
  prs_config_t *prs_config = NULL;
  char str[7][100] = {0};

  paramdef_t PRS_Params[] = PRS_PARAMS_DESC;
  paramlist_def_t PRS_ParamList = {CONFIG_STRING_PRS_CONFIG,NULL,0};
  if (RC.gNB == NULL) {
    RC.gNB                       = (PHY_VARS_gNB **)malloc((1+NUMBER_OF_gNB_MAX)*sizeof(PHY_VARS_gNB*));
    LOG_I(NR_PHY,"RC.gNB = %p\n",RC.gNB);
    memset(RC.gNB,0,(1+NUMBER_OF_gNB_MAX)*sizeof(PHY_VARS_gNB*));
  }

  config_getlist(config_get_if(), &PRS_ParamList, PRS_Params, sizeofArray(PRS_Params), NULL);

  if (PRS_ParamList.numelt > 0) {
    for (j = 0; j < RC.nb_nr_L1_inst; j++) {

      if (RC.gNB[j] == NULL) {
        RC.gNB[j]                       = (PHY_VARS_gNB *)malloc(sizeof(PHY_VARS_gNB));
        LOG_I(NR_PHY,"RC.gNB[%d] = %p\n",j,RC.gNB[j]);
        memset(RC.gNB[j],0,sizeof(PHY_VARS_gNB));
	      RC.gNB[j]->Mod_id  = j;
      }

      RC.gNB[j]->prs_vars.NumPRSResources = *(PRS_ParamList.paramarray[j][NUM_PRS_RESOURCES].uptr);
      for (k = 0; k < RC.gNB[j]->prs_vars.NumPRSResources; k++)
      {
        prs_config = &RC.gNB[j]->prs_vars.prs_cfg[k];
        prs_config->PRSResourceSetPeriod[0]  = PRS_ParamList.paramarray[j][PRS_RESOURCE_SET_PERIOD_LIST].uptr[0];
        prs_config->PRSResourceSetPeriod[1]  = PRS_ParamList.paramarray[j][PRS_RESOURCE_SET_PERIOD_LIST].uptr[1];
        // per PRS resources parameters
        prs_config->SymbolStart              = PRS_ParamList.paramarray[j][PRS_SYMBOL_START_LIST].uptr[k];
        prs_config->NumPRSSymbols            = PRS_ParamList.paramarray[j][PRS_NUM_SYMBOLS_LIST].uptr[k];
        prs_config->REOffset                 = PRS_ParamList.paramarray[j][PRS_RE_OFFSET_LIST].uptr[k];
        prs_config->PRSResourceOffset        = PRS_ParamList.paramarray[j][PRS_RESOURCE_OFFSET_LIST].uptr[k];
        prs_config->NPRSID                   = PRS_ParamList.paramarray[j][PRS_ID_LIST].uptr[k];
        // Common parameters to all PRS resources
        prs_config->NumRB                    = *(PRS_ParamList.paramarray[j][PRS_NUM_RB].uptr);
        prs_config->RBOffset                 = *(PRS_ParamList.paramarray[j][PRS_RB_OFFSET].uptr);
        prs_config->CombSize                 = *(PRS_ParamList.paramarray[j][PRS_COMB_SIZE].uptr);
        prs_config->PRSResourceRepetition    = *(PRS_ParamList.paramarray[j][PRS_RESOURCE_REPETITION].uptr);
        prs_config->PRSResourceTimeGap       = *(PRS_ParamList.paramarray[j][PRS_RESOURCE_TIME_GAP].uptr);
        prs_config->MutingBitRepetition      = *(PRS_ParamList.paramarray[j][PRS_MUTING_BIT_REPETITION].uptr);
        for (int l = 0; l < PRS_ParamList.paramarray[j][PRS_MUTING_PATTERN1_LIST].numelt; l++)
        {
          prs_config->MutingPattern1[l]      = PRS_ParamList.paramarray[j][PRS_MUTING_PATTERN1_LIST].uptr[l];
          if (k == 0) // print only for 0th resource 
            snprintf(str[5]+strlen(str[5]),sizeof(str[5])-strlen(str[5]),"%d, ",prs_config->MutingPattern1[l]);
        }
        for (int l = 0; l < PRS_ParamList.paramarray[j][PRS_MUTING_PATTERN2_LIST].numelt; l++)
        {
          prs_config->MutingPattern2[l]      = PRS_ParamList.paramarray[j][PRS_MUTING_PATTERN2_LIST].uptr[l];
          if (k == 0) // print only for 0th resource
            snprintf(str[6]+strlen(str[6]),sizeof(str[6])-strlen(str[6]),"%d, ",prs_config->MutingPattern2[l]);
        }

        // print to buffer
        snprintf(str[0]+strlen(str[0]),sizeof(str[0])-strlen(str[0]),"%d, ",prs_config->SymbolStart);
        snprintf(str[1]+strlen(str[1]),sizeof(str[1])-strlen(str[1]),"%d, ",prs_config->NumPRSSymbols);
        snprintf(str[2]+strlen(str[2]),sizeof(str[2])-strlen(str[2]),"%d, ",prs_config->REOffset);
        snprintf(str[3]+strlen(str[3]),sizeof(str[3])-strlen(str[3]),"%d, ",prs_config->PRSResourceOffset);
        snprintf(str[4]+strlen(str[4]),sizeof(str[4])-strlen(str[4]),"%d, ",prs_config->NPRSID);
      } // for k

      prs_config = &RC.gNB[j]->prs_vars.prs_cfg[0];
      LOG_I(PHY, "-----------------------------------------\n");
      LOG_I(PHY, "PRS Config for gNB_id %d @ %p\n", j, prs_config);
      LOG_I(PHY, "-----------------------------------------\n");
      LOG_I(PHY, "NumPRSResources \t%d\n", RC.gNB[j]->prs_vars.NumPRSResources);
      LOG_I(PHY, "PRSResourceSetPeriod \t[%d, %d]\n", prs_config->PRSResourceSetPeriod[0], prs_config->PRSResourceSetPeriod[1]);
      LOG_I(PHY, "NumRB \t\t\t%d\n", prs_config->NumRB);
      LOG_I(PHY, "RBOffset \t\t%d\n", prs_config->RBOffset);
      LOG_I(PHY, "CombSize \t\t%d\n", prs_config->CombSize);
      LOG_I(PHY, "PRSResourceRepetition \t%d\n", prs_config->PRSResourceRepetition);
      LOG_I(PHY, "PRSResourceTimeGap \t%d\n", prs_config->PRSResourceTimeGap);
      LOG_I(PHY, "MutingBitRepetition \t%d\n", prs_config->MutingBitRepetition);
      LOG_I(PHY, "SymbolStart \t\t[%s\b\b]\n", str[0]);
      LOG_I(PHY, "NumPRSSymbols \t\t[%s\b\b]\n", str[1]);
      LOG_I(PHY, "REOffset \t\t[%s\b\b]\n", str[2]);
      LOG_I(PHY, "PRSResourceOffset \t[%s\b\b]\n", str[3]);
      LOG_I(PHY, "NPRS_ID \t\t[%s\b\b]\n", str[4]);
      LOG_I(PHY, "MutingPattern1 \t\t[%s\b\b]\n", str[5]);
      LOG_I(PHY, "MutingPattern2 \t\t[%s\b\b]\n", str[6]);
      LOG_I(PHY, "-----------------------------------------\n");
    } // for j
  }
  else
  {
    LOG_I(PHY,"No " CONFIG_STRING_PRS_CONFIG " configuration found..!!\n");
  }
}

void RCconfig_NR_L1(void)
{
  int j = 0;
  if (RC.gNB == NULL) {
    RC.gNB = (PHY_VARS_gNB **)malloc((1 + NUMBER_OF_gNB_MAX) * sizeof(PHY_VARS_gNB *));
    LOG_I(NR_PHY, "RC.gNB = %p\n", RC.gNB);
    memset(RC.gNB, 0, (1 + NUMBER_OF_gNB_MAX) * sizeof(PHY_VARS_gNB *));

    if (RC.gNB[j] == NULL) {
      RC.gNB[j] = calloc(1, sizeof(PHY_VARS_gNB));
    }
  }
  if (NFAPI_MODE != NFAPI_MODE_PNF) {
    paramdef_t GNBSParams[] = GNBSPARAMS_DESC;
    ////////// Identification parameters
    paramdef_t GNBParams[] = GNBPARAMS_DESC;

    paramlist_def_t GNBParamList = {GNB_CONFIG_STRING_GNB_LIST, NULL, 0};

    config_get(config_get_if(), GNBSParams, sizeofArray(GNBSParams), NULL);
    int num_gnbs = GNBSParams[GNB_ACTIVE_GNBS_IDX].numelt;
    AssertFatal(num_gnbs > 0, "Failed to parse config file no gnbs %s \n", GNB_CONFIG_STRING_ACTIVE_GNBS);

    config_getlist(config_get_if(), &GNBParamList, GNBParams, sizeofArray(GNBParams), NULL);
    int N1 = *GNBParamList.paramarray[0][GNB_PDSCH_ANTENNAPORTS_N1_IDX].iptr;
    int N2 = *GNBParamList.paramarray[0][GNB_PDSCH_ANTENNAPORTS_N2_IDX].iptr;
    int XP = *GNBParamList.paramarray[0][GNB_PDSCH_ANTENNAPORTS_XP_IDX].iptr;
    char *ulprbbl = *GNBParamList.paramarray[0][GNB_ULPRBBLACKLIST_IDX].strptr;
    if (ulprbbl)
      LOG_I(NR_PHY, "PRB blacklist %s\n", ulprbbl);
    char *save = NULL;
    char *pt = strtok_r(ulprbbl, ",", &save);
    int prbbl[275];
    int num_prbbl = 0;
    memset(prbbl, 0, 275 * sizeof(int));

    while (pt) {
      const int rb = atoi(pt);
      AssertFatal(rb < 275, "RB %d out of bounds (max 275)\n", rb);
      prbbl[rb] = 0x3FFF; // all symbols taken
      LOG_I(NR_PHY, "Blacklisting prb %d\n", atoi(pt));
      pt = strtok_r(NULL, ",", &save);
      num_prbbl++;
    }

    RC.gNB[j]->num_ulprbbl = num_prbbl;
    LOG_I(NR_PHY, "Copying %d blacklisted PRB to L1 context\n", num_prbbl);
    memcpy(RC.gNB[j]->ulprbbl, prbbl, 275 * sizeof(int));

    RC.gNB[j]->ap_N1 = N1;
    RC.gNB[j]->ap_N2 = N2;
    RC.gNB[j]->ap_XP = XP;
  }

  paramdef_t L1_Params[] = L1PARAMS_DESC;
  paramlist_def_t L1_ParamList = {CONFIG_STRING_L1_LIST, NULL, 0};

  config_getlist(config_get_if(), &L1_ParamList, L1_Params, sizeofArray(L1_Params), NULL);

  if (L1_ParamList.numelt > 0) {
    for (j = 0; j < RC.nb_nr_L1_inst; j++) {
      if (RC.gNB[j] == NULL) {
        RC.gNB[j] = (PHY_VARS_gNB *)malloc(sizeof(PHY_VARS_gNB));
        LOG_I(NR_PHY, "RC.gNB[%d] = %p\n", j, RC.gNB[j]);
        memset(RC.gNB[j], 0, sizeof(PHY_VARS_gNB));
        RC.gNB[j]->Mod_id = j;
      }
      AssertFatal(*L1_ParamList.paramarray[j][L1_THREAD_POOL_SIZE].uptr == 2022, "thread_pool_size removed, please use --thread-pool\n");
      RC.gNB[j]->ofdm_offset_divisor = *(L1_ParamList.paramarray[j][L1_OFDM_OFFSET_DIVISOR].uptr);
      RC.gNB[j]->pucch0_thres = *(L1_ParamList.paramarray[j][L1_PUCCH0_DTX_THRESHOLD].uptr);
      RC.gNB[j]->prach_thres = *(L1_ParamList.paramarray[j][L1_PRACH_DTX_THRESHOLD].uptr);
      RC.gNB[j]->pusch_thres = *(L1_ParamList.paramarray[j][L1_PUSCH_DTX_THRESHOLD].uptr);
      RC.gNB[j]->srs_thres = *(L1_ParamList.paramarray[j][L1_SRS_DTX_THRESHOLD].uptr);
      RC.gNB[j]->max_ldpc_iterations = *(L1_ParamList.paramarray[j][L1_MAX_LDPC_ITERATIONS].uptr);
      RC.gNB[j]->L1_rx_thread_core = *(L1_ParamList.paramarray[j][L1_RX_THREAD_CORE].iptr);
      RC.gNB[j]->L1_tx_thread_core = *(L1_ParamList.paramarray[j][L1_TX_THREAD_CORE].iptr);
      LOG_I(PHY,"L1_RX_THREAD_CORE %d (%d)\n",*(L1_ParamList.paramarray[j][L1_RX_THREAD_CORE].iptr),L1_RX_THREAD_CORE);
      RC.gNB[j]->TX_AMP = (int16_t)(32767.0 / pow(10.0, .05 * (double)(*L1_ParamList.paramarray[j][L1_TX_AMP_BACKOFF_dB].uptr)));
      RC.gNB[j]->phase_comp = *L1_ParamList.paramarray[j][L1_PHASE_COMP].uptr;
      LOG_I(PHY, "TX_AMP = %d (-%d dBFS)\n", RC.gNB[j]->TX_AMP, *L1_ParamList.paramarray[j][L1_TX_AMP_BACKOFF_dB].uptr);
      AssertFatal(RC.gNB[j]->TX_AMP > 300, "TX_AMP is too small, must be larger than 300 (is %d)\n", RC.gNB[j]->TX_AMP);
      if (strcmp(*(L1_ParamList.paramarray[j][L1_TRANSPORT_N_PREFERENCE_IDX].strptr), "local_mac") == 0) {
        // sf_ahead = 2; // Need 4 subframe gap between RX and TX
      } else if (strcmp(*(L1_ParamList.paramarray[j][L1_TRANSPORT_N_PREFERENCE_IDX].strptr), "nfapi") == 0) {
        RC.gNB[j]->eth_params_n.my_addr = strdup(*(L1_ParamList.paramarray[j][L1_LOCAL_N_ADDRESS_IDX].strptr));
        RC.gNB[j]->eth_params_n.remote_addr = strdup(*(L1_ParamList.paramarray[j][L1_REMOTE_N_ADDRESS_IDX].strptr));
        RC.gNB[j]->eth_params_n.my_portc = *(L1_ParamList.paramarray[j][L1_LOCAL_N_PORTC_IDX].iptr);
        RC.gNB[j]->eth_params_n.remote_portc = *(L1_ParamList.paramarray[j][L1_REMOTE_N_PORTC_IDX].iptr);
        RC.gNB[j]->eth_params_n.my_portd = *(L1_ParamList.paramarray[j][L1_LOCAL_N_PORTD_IDX].iptr);
        RC.gNB[j]->eth_params_n.remote_portd = *(L1_ParamList.paramarray[j][L1_REMOTE_N_PORTD_IDX].iptr);
        RC.gNB[j]->eth_params_n.transp_preference = ETH_UDP_MODE;

        // sf_ahead = 2; // Cannot cope with 4 subframes betweem RX and TX - set it to 2

        RC.nb_nr_macrlc_inst = 1; // This is used by mac_top_init_gNB()

        // This is used by init_gNB_afterRU()
        RC.nb_nr_CC = (int *)malloc((1 + RC.nb_nr_inst) * sizeof(int));
        RC.nb_nr_CC[0] = 1;

        LOG_I(PHY, "%s() NFAPI PNF mode - RC.nb_nr_inst=1 this is because phy_init_RU() uses that to index and not RC.num_gNB - why the 2 similar variables?\n", __FUNCTION__);
        LOG_I(PHY, "%s() NFAPI PNF mode - RC.nb_nr_CC[0]=%d for init_gNB_afterRU()\n", __FUNCTION__, RC.nb_nr_CC[0]);
        LOG_I(PHY, "%s() NFAPI PNF mode - RC.nb_nr_macrlc_inst:%d because used by mac_top_init_gNB()\n", __FUNCTION__, RC.nb_nr_macrlc_inst);

        configure_nr_nfapi_pnf(RC.gNB[j]->eth_params_n.remote_addr,
                               RC.gNB[j]->eth_params_n.remote_portc,
                               RC.gNB[j]->eth_params_n.my_addr,
                               RC.gNB[j]->eth_params_n.my_portd,
                               RC.gNB[j]->eth_params_n.remote_portd);
      } else { // other midhaul
      }
    } // for (j = 0; j < RC.nb_nr_L1_inst; j++)
    printf("Initializing northbound interface for L1\n");
    l1_north_init_gNB();
  } else {
    LOG_I(PHY, "No " CONFIG_STRING_L1_LIST " configuration found");

    // need to create some structures for VNF

    j = 0;

    if (RC.gNB[j] == NULL) {
      RC.gNB[j] = (PHY_VARS_gNB *)malloc(sizeof(PHY_VARS_gNB));
      memset((void *)RC.gNB[j], 0, sizeof(PHY_VARS_gNB));
      LOG_I(PHY, "RC.gNB[%d] = %p\n", j, RC.gNB[j]);
      RC.gNB[j]->Mod_id = j;
    }
  }
}

static NR_ServingCellConfigCommon_t *get_scc_config(configmodule_interface_t *cfg, int minRXTXTIME)
{
  NR_ServingCellConfigCommon_t *scc = calloc_or_fail(1, sizeof(*scc));
  uint64_t ssb_bitmap=0xff;
  prepare_scc(scc);
  paramdef_t SCCsParams[] = SCCPARAMS_DESC(scc);
  prepare_msgA_scc(scc);
  paramdef_t MsgASCCsParams[] = MSGASCCPARAMS_DESC(scc);
  paramlist_def_t SCCsParamList = {GNB_CONFIG_STRING_SERVINGCELLCONFIGCOMMON, NULL, 0};
  paramlist_def_t MsgASCCsParamList = {GNB_CONFIG_STRING_SERVINGCELLCONFIGCOMMON, NULL, 0};

  char aprefix[MAX_OPTNAME_SIZE*2 + 8];
  sprintf(aprefix, "%s.[%i]", GNB_CONFIG_STRING_GNB_LIST, 0);
  config_getlist(cfg, &SCCsParamList, NULL, 0, aprefix);
  config_getlist(cfg, &MsgASCCsParamList, NULL, 0, aprefix);

  if (SCCsParamList.numelt > 0 || MsgASCCsParamList.numelt > 0) {
    sprintf(aprefix, "%s.[%i].%s.[%i]", GNB_CONFIG_STRING_GNB_LIST,0,GNB_CONFIG_STRING_SERVINGCELLCONFIGCOMMON, 0);
    config_get(cfg, SCCsParams, sizeofArray(SCCsParams), aprefix);
    config_get(cfg, MsgASCCsParams, sizeofArray(MsgASCCsParams), aprefix);
    struct NR_FrequencyInfoDL *frequencyInfoDL = scc->downlinkConfigCommon->frequencyInfoDL;
    LOG_I(RRC,
          "Read in ServingCellConfigCommon (PhysCellId %d, ABSFREQSSB %d, DLBand %d, ABSFREQPOINTA %d, DLBW "
          "%d,RACH_TargetReceivedPower %d\n",
          (int)*scc->physCellId,
          (int)*frequencyInfoDL->absoluteFrequencySSB,
          (int)*frequencyInfoDL->frequencyBandList.list.array[0],
          (int)frequencyInfoDL->absoluteFrequencyPointA,
          (int)frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth,
          (int)scc->uplinkConfigCommon->initialUplinkBWP->rach_ConfigCommon->choice.setup->rach_ConfigGeneric
              .preambleReceivedTargetPower);
    // SSB of the PCell is always on the sync raster
    uint64_t ssb_freq = from_nrarfcn(*frequencyInfoDL->frequencyBandList.list.array[0],
                                     *scc->ssbSubcarrierSpacing,
                                     *frequencyInfoDL->absoluteFrequencySSB);
    LOG_I(RRC, "absoluteFrequencySSB %ld corresponds to %lu Hz\n", *frequencyInfoDL->absoluteFrequencySSB, ssb_freq);
    if (get_softmodem_params()->sa)
      check_ssb_raster(ssb_freq, *frequencyInfoDL->frequencyBandList.list.array[0], *scc->ssbSubcarrierSpacing);
    fix_scc(scc, ssb_bitmap);
  }
  nr_rrc_config_ul_tda(scc, minRXTXTIME);

  // the gNB uses the servingCellConfigCommon everywhere, even when it should use the servingCellConfigCommonSIB.
  // previously (before this commit), the following fields were indirectly populated through get_SIB1_NR().
  // since this might lead to memory problems (e.g., double frees), it has been moved here.
  // note that the "right solution" would be to not populate the servingCellConfigCommon here, and use
  // an "abstraction struct" that contains the corresponding values, from which SCC/SIB1/... is generated.
  NR_PDCCH_ConfigCommon_t *pcc = scc->downlinkConfigCommon->initialDownlinkBWP->pdcch_ConfigCommon->choice.setup;
  AssertFatal(pcc != NULL && pcc->commonSearchSpaceList == NULL, "memory leak\n");
  pcc->commonSearchSpaceList = calloc_or_fail(1, sizeof(*pcc->commonSearchSpaceList));

  NR_SearchSpace_t *ss1 = rrc_searchspace_config(true, 1, 0);
  asn1cSeqAdd(&pcc->commonSearchSpaceList->list, ss1);
  NR_SearchSpace_t *ss2 = rrc_searchspace_config(true, 2, 0);
  asn1cSeqAdd(&pcc->commonSearchSpaceList->list, ss2);
  NR_SearchSpace_t *ss3 = rrc_searchspace_config(true, 3, 0);
  asn1cSeqAdd(&pcc->commonSearchSpaceList->list, ss3);

  asn1cCallocOne(pcc->searchSpaceSIB1,  0);
  asn1cCallocOne(pcc->ra_SearchSpace, 1);
  asn1cCallocOne(pcc->pagingSearchSpace, 2);
  asn1cCallocOne(pcc->searchSpaceOtherSystemInformation, 3);

  return scc;
}

static NR_ServingCellConfig_t *get_scd_config(configmodule_interface_t *cfg)
{
  NR_ServingCellConfig_t *scd = calloc(1, sizeof(*scd));
  prepare_scd(scd);
  paramdef_t SCDsParams[] = SCDPARAMS_DESC(scd);
  paramlist_def_t SCDsParamList = {GNB_CONFIG_STRING_SERVINGCELLCONFIGDEDICATED, NULL, 0};

  char aprefix[MAX_OPTNAME_SIZE * 2 + 8];
  snprintf(aprefix, sizeof(aprefix), "%s.[%i]", GNB_CONFIG_STRING_GNB_LIST, 0);
  config_getlist(cfg, &SCDsParamList, NULL, 0, aprefix);
  if (SCDsParamList.numelt > 0) {
    sprintf(aprefix, "%s.[%i].%s.[%i]", GNB_CONFIG_STRING_GNB_LIST, 0, GNB_CONFIG_STRING_SERVINGCELLCONFIGDEDICATED, 0);
    config_get(cfg, SCDsParams, sizeof(SCDsParams) / sizeof(paramdef_t), aprefix);
    const NR_BWP_UplinkDedicated_t *bwp_Dedicated = scd->uplinkConfig->uplinkBWP_ToAddModList->list.array[0]->bwp_Dedicated;
    const NR_PTRS_UplinkConfig_t *setup =
        bwp_Dedicated->pusch_Config->choice.setup->dmrs_UplinkForPUSCH_MappingTypeB->choice.setup->phaseTrackingRS->choice.setup;
    LOG_I(RRC,
          "Read in ServingCellConfigDedicated UL (FreqDensity_0 %ld, FreqDensity_1 %ld, TimeDensity_0 %ld, TimeDensity_1 %ld, "
          "TimeDensity_2 %ld, RE offset %ld, First_active_BWP_ID %ld SCS %ld, LocationandBW %ld\n",
          *setup->transformPrecoderDisabled->frequencyDensity->list.array[0],
          *setup->transformPrecoderDisabled->frequencyDensity->list.array[1],
          *setup->transformPrecoderDisabled->timeDensity->list.array[0],
          *setup->transformPrecoderDisabled->timeDensity->list.array[1],
          *setup->transformPrecoderDisabled->timeDensity->list.array[2],
          *setup->transformPrecoderDisabled->resourceElementOffset,
          *scd->firstActiveDownlinkBWP_Id,
          scd->downlinkBWP_ToAddModList->list.array[0]->bwp_Common->genericParameters.subcarrierSpacing,
          scd->downlinkBWP_ToAddModList->list.array[0]->bwp_Common->genericParameters.locationAndBandwidth);
  }
  fix_scd(scd);

  return scd;
}

static int read_du_cell_info(configmodule_interface_t *cfg,
                             bool separate_du,
                             uint32_t *gnb_id,
                             uint64_t *gnb_du_id,
                             char **name,
                             f1ap_served_cell_info_t *info,
                             int max_cell_info)
{
  AssertFatal(max_cell_info == 1, "only one cell supported\n");
  memset(info, 0, sizeof(*info));

  paramdef_t GNBSParams[] = GNBSPARAMS_DESC;
  paramdef_t GNBParams[]  = GNBPARAMS_DESC;
  paramlist_def_t GNBParamList = {GNB_CONFIG_STRING_GNB_LIST,NULL,0};
  config_get(cfg, GNBSParams, sizeof(GNBSParams) / sizeof(paramdef_t), NULL);
  int num_gnbs = GNBSParams[GNB_ACTIVE_GNBS_IDX].numelt;
  AssertFatal(num_gnbs == 1, "cannot configure DU: required config section \"gNBs\" missing\n");

  // Output a list of all eNBs.
  config_getlist(cfg, &GNBParamList, GNBParams, sizeof(GNBParams) / sizeof(paramdef_t), NULL);

  // read the gNB-ID. The DU itself only needs the gNB-DU ID, but some (e.g.,
  // E2 agent) need the gNB-ID as well if it is running in a separate process
  AssertFatal(config_isparamset(GNBParamList.paramarray[0], GNB_GNB_ID_IDX), "%s is not defined in configuration file\n", GNB_CONFIG_STRING_GNB_ID);
  *gnb_id = *GNBParamList.paramarray[0][GNB_GNB_ID_IDX].uptr;

  AssertFatal(strcmp(GNBSParams[GNB_ACTIVE_GNBS_IDX].strlistptr[0], *GNBParamList.paramarray[0][GNB_GNB_NAME_IDX].strptr) == 0,
              "no active gNB found/mismatch of gNBs: %s vs %s\n",
              GNBSParams[GNB_ACTIVE_GNBS_IDX].strlistptr[0],
              *GNBParamList.paramarray[0][GNB_GNB_NAME_IDX].strptr);

  char aprefix[MAX_OPTNAME_SIZE * 2 + 8];
  sprintf(aprefix, "%s.[0]", GNB_CONFIG_STRING_GNB_LIST);
  paramdef_t PLMNParams[] = GNBPLMNPARAMS_DESC;
  /* map parameter checking array instances to parameter definition array instances */
  checkedparam_t config_check_PLMNParams[] = PLMNPARAMS_CHECK;
  for (int I = 0; I < sizeof(PLMNParams) / sizeof(paramdef_t); ++I)
    PLMNParams[I].chkPptr = &(config_check_PLMNParams[I]);
  paramlist_def_t PLMNParamList = {GNB_CONFIG_STRING_PLMN_LIST, NULL, 0};
  config_getlist(cfg, &PLMNParamList, PLMNParams, sizeof(PLMNParams) / sizeof(paramdef_t), aprefix);
  AssertFatal(PLMNParamList.numelt > 0, "need to have a PLMN in PLMN section\n");
  AssertFatal(PLMNParamList.numelt == 1, "cannot have more than one PLMN\n");

  // if fronthaul is F1, require gNB_DU_ID, else use gNB_ID
  if (separate_du) {
    AssertFatal(config_isparamset(GNBParamList.paramarray[0], GNB_GNB_DU_ID_IDX), "%s is not defined in configuration file\n", GNB_CONFIG_STRING_GNB_DU_ID);
    *gnb_du_id = *GNBParamList.paramarray[0][GNB_GNB_DU_ID_IDX].u64ptr;
  } else {
    AssertFatal(!config_isparamset(GNBParamList.paramarray[0], GNB_GNB_DU_ID_IDX),
                "%s should not be defined in configuration file for monolithic gNB\n",
                GNB_CONFIG_STRING_GNB_DU_ID);
    *gnb_du_id = *gnb_id; // the gNB-DU ID is equal to the gNB ID, since the config has no gNB-DU ID
  }

  *name = strdup(*(GNBParamList.paramarray[0][GNB_GNB_NAME_IDX].strptr));
  info->tac = malloc(sizeof(*info->tac));
  AssertFatal(info->tac != NULL, "out of memory\n");
  *info->tac = *GNBParamList.paramarray[0][GNB_TRACKING_AREA_CODE_IDX].uptr;
  info->plmn.mcc = *PLMNParamList.paramarray[0][GNB_MOBILE_COUNTRY_CODE_IDX].uptr;
  info->plmn.mnc = *PLMNParamList.paramarray[0][GNB_MOBILE_NETWORK_CODE_IDX].uptr;
  info->plmn.mnc_digit_length = *PLMNParamList.paramarray[0][GNB_MNC_DIGIT_LENGTH].u8ptr;
  AssertFatal((info->plmn.mnc_digit_length == 2) || (info->plmn.mnc_digit_length == 3),
              "BAD MNC DIGIT LENGTH %d",
              info->plmn.mnc_digit_length);
  info->nr_cellid = (uint64_t) * (GNBParamList.paramarray[0][GNB_NRCELLID_IDX].u64ptr);

  paramdef_t SNSSAIParams[] = GNBSNSSAIPARAMS_DESC;
  paramlist_def_t SNSSAIParamList = {GNB_CONFIG_STRING_SNSSAI_LIST, NULL, 0};
  checkedparam_t config_check_SNSSAIParams[] = SNSSAIPARAMS_CHECK;
  for (int J = 0; J < sizeofArray(SNSSAIParams); ++J)
    SNSSAIParams[J].chkPptr = &(config_check_SNSSAIParams[J]);
  char snssaistr[MAX_OPTNAME_SIZE * 2 + 8];
  sprintf(snssaistr, "%s.[0].%s.[0]", GNB_CONFIG_STRING_GNB_LIST, GNB_CONFIG_STRING_PLMN_LIST);
  config_getlist(config_get_if(), &SNSSAIParamList, SNSSAIParams, sizeofArray(SNSSAIParams), snssaistr);
  info->num_ssi = SNSSAIParamList.numelt;
  for (int s = 0; s < info->num_ssi; ++s) {
    info->nssai[s].sst = *SNSSAIParamList.paramarray[s][GNB_SLICE_SERVICE_TYPE_IDX].uptr;
    info->nssai[s].sd = *SNSSAIParamList.paramarray[s][GNB_SLICE_DIFFERENTIATOR_IDX].uptr;
    AssertFatal(info->nssai[s].sd <= 0xffffff, "SD cannot be bigger than 0xffffff, but is %d\n", info->nssai[s].sd);
  }

  return 1;
}

static f1ap_tdd_info_t read_tdd_config(const NR_ServingCellConfigCommon_t *scc)
{
  const NR_FrequencyInfoDL_t *dl = scc->downlinkConfigCommon->frequencyInfoDL;
  f1ap_tdd_info_t tdd = {
      .freqinfo.arfcn = dl->absoluteFrequencyPointA,
      .freqinfo.band = *dl->frequencyBandList.list.array[0],
      .tbw.scs = dl->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing,
      .tbw.nrb = dl->scs_SpecificCarrierList.list.array[0]->carrierBandwidth,
  };
  return tdd;
}

static f1ap_fdd_info_t read_fdd_config(const NR_ServingCellConfigCommon_t *scc)
{
  const NR_FrequencyInfoDL_t *dl = scc->downlinkConfigCommon->frequencyInfoDL;
  const NR_FrequencyInfoUL_t *ul = scc->uplinkConfigCommon->frequencyInfoUL;
  f1ap_fdd_info_t fdd = {
      .dl_freqinfo.arfcn = dl->absoluteFrequencyPointA,
      .ul_freqinfo.arfcn = *ul->absoluteFrequencyPointA,
      .dl_tbw.scs = dl->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing,
      .ul_tbw.scs = ul->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing,
      .dl_tbw.nrb = dl->scs_SpecificCarrierList.list.array[0]->carrierBandwidth,
      .ul_tbw.nrb = ul->scs_SpecificCarrierList.list.array[0]->carrierBandwidth,
      .dl_freqinfo.band = *dl->frequencyBandList.list.array[0],
      .ul_freqinfo.band = *ul->frequencyBandList->list.array[0],
  };
  return fdd;
}

static f1ap_setup_req_t *RC_read_F1Setup(uint64_t id,
                                         const char *name,
                                         const f1ap_served_cell_info_t *info,
                                         const NR_ServingCellConfigCommon_t *scc,
                                         NR_BCCH_BCH_Message_t *mib,
                                         const NR_BCCH_DL_SCH_Message_t *sib1)
{
  f1ap_setup_req_t *req = calloc(1, sizeof(*req));
  AssertFatal(req != NULL, "out of memory\n");
  req->gNB_DU_id = id;
  req->gNB_DU_name = strdup(name);
  req->num_cells_available = 1;
  req->cell[0].info = *info;
  LOG_I(GNB_APP,
        "F1AP: gNB idx %d gNB_DU_id %ld, gNB_DU_name %s, TAC %d MCC/MNC/length %d/%d/%d cellID %ld\n",
        0,
        req->gNB_DU_id,
        req->gNB_DU_name,
        *req->cell[0].info.tac,
        req->cell[0].info.plmn.mcc,
        req->cell[0].info.plmn.mnc,
        req->cell[0].info.plmn.mnc_digit_length,
        req->cell[0].info.nr_cellid);

  req->cell[0].info.nr_pci = *scc->physCellId;
  if (scc->tdd_UL_DL_ConfigurationCommon) {
    LOG_I(GNB_APP, "ngran_DU: Configuring Cell %d for TDD\n", 0);
    req->cell[0].info.mode = F1AP_MODE_TDD;
    req->cell[0].info.tdd = read_tdd_config(scc);
  } else {
    LOG_I(GNB_APP, "ngran_DU: Configuring Cell %d for FDD\n", 0);
    req->cell[0].info.mode = F1AP_MODE_FDD;
    req->cell[0].info.fdd = read_fdd_config(scc);
  }

  NR_MeasurementTimingConfiguration_t *mtc = get_new_MeasurementTimingConfiguration(scc);
  uint8_t buf[1024];
  int len = encode_MeasurementTimingConfiguration(mtc, buf, sizeof(buf));
  DevAssert(len <= sizeof(buf));
  free_MeasurementTimingConfiguration(mtc);
  uint8_t *mtc_buf = calloc(len, sizeof(*mtc_buf));
  AssertFatal(mtc_buf != NULL, "out of memory\n");
  memcpy(mtc_buf, buf, len);
  req->cell[0].info.measurement_timing_config = mtc_buf;
  req->cell[0].info.measurement_timing_config_len = len;

  if (get_softmodem_params()->sa) {
    // in NSA we don't transmit SIB1, so cannot fill DU system information
    // so cannot send MIB either

    int buf_len = 3; // this is what we assume in monolithic
    req->cell[0].sys_info = calloc(1, sizeof(*req->cell[0].sys_info));
    AssertFatal(req->cell[0].sys_info != NULL, "out of memory\n");
    f1ap_gnb_du_system_info_t *sys_info = req->cell[0].sys_info;
    sys_info->mib = calloc(buf_len, sizeof(*sys_info->mib));
    DevAssert(sys_info->mib != NULL);
    DevAssert(mib != NULL);
    // encode only the mib message itself
    sys_info->mib_length = encode_MIB_NR_setup(mib->message.choice.mib, 0, sys_info->mib, buf_len);
    DevAssert(sys_info->mib_length == buf_len);

    DevAssert(sib1 != NULL);
    NR_SIB1_t *bcch_SIB1 = sib1->message.choice.c1->choice.systemInformationBlockType1;
    sys_info->sib1 = calloc(NR_MAX_SIB_LENGTH / 8, sizeof(*sys_info->sib1));
    asn_enc_rval_t enc_rval = uper_encode_to_buffer(&asn_DEF_NR_SIB1, NULL, (void *)bcch_SIB1, sys_info->sib1, NR_MAX_SIB_LENGTH / 8);
    AssertFatal(enc_rval.encoded > 0, "ASN1 message encoding failed (%s, %lu)!\n", enc_rval.failed_type->name, enc_rval.encoded);
    sys_info->sib1_length = (enc_rval.encoded + 7) / 8;
  }

  int num = read_version(TO_STRING(NR_RRC_VERSION), &req->rrc_ver[0], &req->rrc_ver[1], &req->rrc_ver[2]);
  AssertFatal(num == 3, "could not read RRC version string %s\n", TO_STRING(NR_RRC_VERSION));

  return req;
}

void RCconfig_nr_macrlc(configmodule_interface_t *cfg)
{
  int j = 0;
  uint16_t prbbl[275] = {0};
  int num_prbbl = 0;

  paramlist_def_t GNBParamList = {GNB_CONFIG_STRING_GNB_LIST, NULL, 0};
  paramdef_t GNBSParams[] = GNBSPARAMS_DESC;
  config_get(cfg, GNBSParams, sizeofArray(GNBSParams), NULL);
  int num_gnbs = GNBSParams[GNB_ACTIVE_GNBS_IDX].numelt;
  AssertFatal(num_gnbs == 1,
              "Failed to parse config file: number of gnbs for gNB %s is %d != 1\n",
              GNB_CONFIG_STRING_ACTIVE_GNBS,
              num_gnbs);
  paramdef_t GNBParams[] = GNBPARAMS_DESC;
  /* map parameter checking array instances to parameter definition array instances */
  checkedparam_t config_check_GNBParams[] = GNBPARAMS_CHECK;
  for (int i = 0; i < sizeofArray(GNBParams); ++i)
    GNBParams[i].chkPptr = &(config_check_GNBParams[i]);
  config_getlist(cfg, &GNBParamList, GNBParams, sizeofArray(GNBParams), NULL);

  if (NFAPI_MODE != NFAPI_MODE_PNF) {
    ////////// Identification parameters

    char *ulprbbl = *GNBParamList.paramarray[0][GNB_ULPRBBLACKLIST_IDX].strptr;
    char *save = NULL;
    char *pt = strtok_r(ulprbbl, ",", &save);
    memset(prbbl, 0, sizeof(prbbl));
    while (pt) {
      const int prb = atoi(pt);
      AssertFatal(prb < 275, "RB %d out of bounds (max 275)\n", prb);
      prbbl[prb] = 0x3FFF; // all symbols taken
      pt = strtok_r(NULL, ",", &save);
      num_prbbl++;
    }
  }
  paramdef_t MacRLC_Params[] = MACRLCPARAMS_DESC;
  paramlist_def_t MacRLC_ParamList = {CONFIG_STRING_MACRLC_LIST, NULL, 0};
  /* map parameter checking array instances to parameter definition array instances */
  checkedparam_t config_check_MacRLCParams[] = MACRLCPARAMS_CHECK;
  for (int i = 0; i < sizeofArray(MacRLC_Params); ++i)
    MacRLC_Params[i].chkPptr = &(config_check_MacRLCParams[i]);
  config_getlist(config_get_if(), &MacRLC_ParamList, MacRLC_Params, sizeofArray(MacRLC_Params), NULL);

  nr_mac_config_t config = {0};
  config.pdsch_AntennaPorts.N1 = *GNBParamList.paramarray[0][GNB_PDSCH_ANTENNAPORTS_N1_IDX].iptr;
  config.pdsch_AntennaPorts.N2 = *GNBParamList.paramarray[0][GNB_PDSCH_ANTENNAPORTS_N2_IDX].iptr;
  config.pdsch_AntennaPorts.XP = *GNBParamList.paramarray[0][GNB_PDSCH_ANTENNAPORTS_XP_IDX].iptr;
  config.pusch_AntennaPorts = *GNBParamList.paramarray[0][GNB_PUSCH_ANTENNAPORTS_IDX].iptr;
  LOG_I(GNB_APP,
        "pdsch_AntennaPorts N1 %d N2 %d XP %d pusch_AntennaPorts %d\n",
        config.pdsch_AntennaPorts.N1,
        config.pdsch_AntennaPorts.N2,
        config.pdsch_AntennaPorts.XP,
        config.pusch_AntennaPorts);

  paramdef_t RUParams[] = RUPARAMS_DESC;
  paramlist_def_t RUParamList = {CONFIG_STRING_RU_LIST, NULL, 0};
  config_getlist(config_get_if(), &RUParamList, RUParams, sizeofArray(RUParams), NULL);
  int num_tx = 0;
  if (RUParamList.numelt > 0) {
    for (int i = 0; i < RUParamList.numelt; i++)
      num_tx += *(RUParamList.paramarray[i][RU_NB_TX_IDX].uptr);
    AssertFatal(num_tx >= config.pdsch_AntennaPorts.XP * config.pdsch_AntennaPorts.N1 * config.pdsch_AntennaPorts.N2,
                "Number of logical antenna ports (set in config file with pdsch_AntennaPorts) cannot be larger than physical antennas (nb_tx)\n");
  }
  else {
    // TODO temporary solution for 3rd party RU or nFAPI, in which case we don't have RU section present in the config file
    num_tx = config.pdsch_AntennaPorts.XP * config.pdsch_AntennaPorts.N1 * config.pdsch_AntennaPorts.N2;
    LOG_E(GNB_APP, "RU information not present in config file. Assuming physical antenna ports equal to logical antenna ports %d\n", num_tx);
  }
  config.minRXTXTIME = *GNBParamList.paramarray[0][GNB_MINRXTXTIME_IDX].iptr;
  LOG_I(GNB_APP, "minTXRXTIME %d\n", config.minRXTXTIME);
  config.sib1_tda = *GNBParamList.paramarray[0][GNB_SIB1_TDA_IDX].iptr;
  LOG_I(GNB_APP, "SIB1 TDA %d\n", config.sib1_tda);
  config.do_CSIRS = *GNBParamList.paramarray[0][GNB_DO_CSIRS_IDX].iptr;
  config.do_SRS = *GNBParamList.paramarray[0][GNB_DO_SRS_IDX].iptr;
  config.force_256qam_off = *GNBParamList.paramarray[0][GNB_FORCE256QAMOFF_IDX].iptr;
  config.force_UL256qam_off = *GNBParamList.paramarray[0][GNB_FORCEUL256QAMOFF_IDX].iptr;
  config.use_deltaMCS = *GNBParamList.paramarray[0][GNB_USE_DELTA_MCS_IDX].iptr != 0;
  config.maxMIMO_layers = *GNBParamList.paramarray[0][GNB_MAXMIMOLAYERS_IDX].iptr;
  config.disable_harq = *GNBParamList.paramarray[0][GNB_DISABLE_HARQ_IDX].iptr;
  config.num_dlharq = *GNBParamList.paramarray[0][GNB_NUM_DL_HARQ_IDX].iptr;
  config.num_ulharq =  *GNBParamList.paramarray[0][GNB_NUM_UL_HARQ_IDX].iptr;
  if (config.disable_harq)
    LOG_W(GNB_APP, "\"disable_harq\" is a REL17 feature and is incompatible with REL15 and REL16 UEs!\n");
  LOG_I(GNB_APP,
        "CSI-RS %d, SRS %d, 256 QAM %s, delta_MCS %s, maxMIMO_Layers %d, HARQ feedback %s, num DLHARQ:%d, num ULHARQ:%d\n",
        config.do_CSIRS,
        config.do_SRS,
        config.force_256qam_off ? "force off" : "may be on",
        config.use_deltaMCS ? "on" : "off",
        config.maxMIMO_layers,
        config.disable_harq ? "disabled" : "enabled",
        config.num_dlharq,
        config.num_ulharq);
  int tot_ant = config.pdsch_AntennaPorts.N1 * config.pdsch_AntennaPorts.N2 * config.pdsch_AntennaPorts.XP;
  AssertFatal(config.maxMIMO_layers != 0 && config.maxMIMO_layers <= tot_ant, "Invalid maxMIMO_layers %d\n", config.maxMIMO_layers);

  paramdef_t Timers_Params[] = GNB_TIMERS_PARAMS_DESC;
  char aprefix[MAX_OPTNAME_SIZE * 2 + 8];
  sprintf(aprefix, "%s.[0].%s", GNB_CONFIG_STRING_GNB_LIST, GNB_CONFIG_STRING_TIMERS_CONFIG);
  config_get(config_get_if(), Timers_Params, sizeofArray(Timers_Params), aprefix);

  config.timer_config.sr_ProhibitTimer = *Timers_Params[GNB_TIMERS_SR_PROHIBIT_TIMER_IDX].iptr;
  config.timer_config.sr_TransMax = *Timers_Params[GNB_TIMERS_SR_TRANS_MAX_IDX].iptr;
  config.timer_config.sr_ProhibitTimer_v1700 = *Timers_Params[GNB_TIMERS_SR_PROHIBIT_TIMER_V1700_IDX].iptr;
  config.timer_config.t300 = *Timers_Params[GNB_TIMERS_T300_IDX].iptr;
  config.timer_config.t301 = *Timers_Params[GNB_TIMERS_T301_IDX].iptr;
  config.timer_config.t310 = *Timers_Params[GNB_TIMERS_T310_IDX].iptr;
  config.timer_config.n310 = *Timers_Params[GNB_TIMERS_N310_IDX].iptr;
  config.timer_config.t311 = *Timers_Params[GNB_TIMERS_T311_IDX].iptr;
  config.timer_config.n311 = *Timers_Params[GNB_TIMERS_N311_IDX].iptr;
  config.timer_config.t319 = *Timers_Params[GNB_TIMERS_T319_IDX].iptr;
  LOG_I(GNB_APP,
        "sr_ProhibitTimer %d, sr_TransMax %d, sr_ProhibitTimer_v1700 %d, "
        "t300 %d, t301 %d, t310 %d, n310 %d, t311 %d, n311 %d, t319 %d\n",
        config.timer_config.sr_ProhibitTimer,
        config.timer_config.sr_TransMax,
        config.timer_config.sr_ProhibitTimer_v1700,
        config.timer_config.t300,
        config.timer_config.t301,
        config.timer_config.t310,
        config.timer_config.n310,
        config.timer_config.t311,
        config.timer_config.n311,
        config.timer_config.t319);

  if (config_isparamset(GNBParamList.paramarray[0], GNB_BEAMWEIGHTS_IDX)) {
    int n = GNBParamList.paramarray[0][GNB_BEAMWEIGHTS_IDX].numelt;
    AssertFatal(n % num_tx == 0, "Error! Number of beam input needs to be multiple of TX antennas\n");
    // each beam is described by a set of weights (one for each antenna)
    // on the other hand in case of analog beamforming an index to the RU beam identifier is provided
    config.nb_bfw[0] = num_tx;  // number of tx antennas
    config.nb_bfw[1] = n / num_tx; // number of beams
    config.bw_list = malloc16_clear(n * sizeof(*config.bw_list));
    for (int b = 0; b < n; b++) {
      config.bw_list[b] = GNBParamList.paramarray[0][GNB_BEAMWEIGHTS_IDX].iptr[b];
    }
  }

  NR_ServingCellConfigCommon_t *scc = get_scc_config(cfg, config.minRXTXTIME);
  //xer_fprint(stdout, &asn_DEF_NR_ServingCellConfigCommon, scc);
  NR_ServingCellConfig_t *scd = get_scd_config(cfg);

  if (MacRLC_ParamList.numelt > 0) {
    RC.nb_nr_macrlc_inst = MacRLC_ParamList.numelt;
    ngran_node_t node_type = get_node_type();
    mac_top_init_gNB(node_type, scc, scd, &config);
    RC.nb_nr_mac_CC = (int *)malloc(RC.nb_nr_macrlc_inst * sizeof(int));

    for (j = 0; j < RC.nb_nr_macrlc_inst; j++) {
      RC.nb_nr_mac_CC[j] = *(MacRLC_ParamList.paramarray[j][MACRLC_CC_IDX].iptr);
      RC.nrmac[j]->pusch_target_snrx10 = *(MacRLC_ParamList.paramarray[j][MACRLC_PUSCHTARGETSNRX10_IDX].iptr);
      RC.nrmac[j]->pucch_target_snrx10 = *(MacRLC_ParamList.paramarray[j][MACRLC_PUCCHTARGETSNRX10_IDX].iptr);
      RC.nrmac[j]->ul_prbblack_SNR_threshold = *(MacRLC_ParamList.paramarray[j][MACRLC_UL_PRBBLACK_SNR_THRESHOLD_IDX].iptr);
      RC.nrmac[j]->pucch_failure_thres = *(MacRLC_ParamList.paramarray[j][MACRLC_PUCCHFAILURETHRES_IDX].iptr);
      RC.nrmac[j]->pusch_failure_thres = *(MacRLC_ParamList.paramarray[j][MACRLC_PUSCHFAILURETHRES_IDX].iptr);

      LOG_I(NR_MAC,
            "PUSCH Target %d, PUCCH Target %d, PUCCH Failure %d, PUSCH Failure %d\n",
            RC.nrmac[j]->pusch_target_snrx10,
            RC.nrmac[j]->pucch_target_snrx10,
            RC.nrmac[j]->pucch_failure_thres,
            RC.nrmac[j]->pusch_failure_thres);
      if (strcmp(*(MacRLC_ParamList.paramarray[j][MACRLC_TRANSPORT_N_PREFERENCE_IDX].strptr), "local_RRC") == 0) {
        // check number of instances is same as RRC/PDCP

      } else if (strcmp(*(MacRLC_ParamList.paramarray[j][MACRLC_TRANSPORT_N_PREFERENCE_IDX].strptr), "f1") == 0
                 || strcmp(*(MacRLC_ParamList.paramarray[j][MACRLC_TRANSPORT_N_PREFERENCE_IDX].strptr), "cudu") == 0) {
        printf("Configuring F1 interfaces for MACRLC\n");
        char **f1caddr = MacRLC_ParamList.paramarray[j][MACRLC_LOCAL_N_ADDRESS_IDX].strptr;
        RC.nrmac[j]->eth_params_n.my_addr = strdup(*f1caddr);
        char **f1uaddr = MacRLC_ParamList.paramarray[j][MACRLC_LOCAL_N_ADDRESS_F1U_IDX].strptr;
        RC.nrmac[j]->f1u_addr = f1uaddr != NULL ? strdup(*f1uaddr) : strdup(*f1caddr);
        RC.nrmac[j]->eth_params_n.remote_addr = strdup(*(MacRLC_ParamList.paramarray[j][MACRLC_REMOTE_N_ADDRESS_IDX].strptr));
        RC.nrmac[j]->eth_params_n.my_portc = *(MacRLC_ParamList.paramarray[j][MACRLC_LOCAL_N_PORTC_IDX].iptr);
        RC.nrmac[j]->eth_params_n.remote_portc = *(MacRLC_ParamList.paramarray[j][MACRLC_REMOTE_N_PORTC_IDX].iptr);
        RC.nrmac[j]->eth_params_n.my_portd = *(MacRLC_ParamList.paramarray[j][MACRLC_LOCAL_N_PORTD_IDX].iptr);
        RC.nrmac[j]->eth_params_n.remote_portd = *(MacRLC_ParamList.paramarray[j][MACRLC_REMOTE_N_PORTD_IDX].iptr);
        RC.nrmac[j]->eth_params_n.transp_preference = ETH_UDP_MODE;
      } else { // other midhaul
        AssertFatal(1 == 0, "MACRLC %d: %s unknown northbound midhaul\n", j, *(MacRLC_ParamList.paramarray[j][MACRLC_TRANSPORT_N_PREFERENCE_IDX].strptr));
      }

      if (strcmp(*(MacRLC_ParamList.paramarray[j][MACRLC_TRANSPORT_S_PREFERENCE_IDX].strptr), "local_L1") == 0) {
      } else if (strcmp(*(MacRLC_ParamList.paramarray[j][MACRLC_TRANSPORT_S_PREFERENCE_IDX].strptr), "nfapi") == 0) {
        RC.nrmac[j]->eth_params_s.my_addr = strdup(*(MacRLC_ParamList.paramarray[j][MACRLC_LOCAL_S_ADDRESS_IDX].strptr));
        RC.nrmac[j]->eth_params_s.remote_addr = strdup(*(MacRLC_ParamList.paramarray[j][MACRLC_REMOTE_S_ADDRESS_IDX].strptr));
        RC.nrmac[j]->eth_params_s.my_portc = *(MacRLC_ParamList.paramarray[j][MACRLC_LOCAL_S_PORTC_IDX].iptr);
        RC.nrmac[j]->eth_params_s.remote_portc = *(MacRLC_ParamList.paramarray[j][MACRLC_REMOTE_S_PORTC_IDX].iptr);
        RC.nrmac[j]->eth_params_s.my_portd = *(MacRLC_ParamList.paramarray[j][MACRLC_LOCAL_S_PORTD_IDX].iptr);
        RC.nrmac[j]->eth_params_s.remote_portd = *(MacRLC_ParamList.paramarray[j][MACRLC_REMOTE_S_PORTD_IDX].iptr);
        RC.nrmac[j]->eth_params_s.transp_preference = ETH_UDP_MODE;

        printf("**************** vnf_port:%d\n", RC.nrmac[j]->eth_params_s.my_portc);
        configure_nr_nfapi_vnf(
            RC.nrmac[j]->eth_params_s.my_addr, RC.nrmac[j]->eth_params_s.my_portc, RC.nrmac[j]->eth_params_s.remote_addr, RC.nrmac[j]->eth_params_s.remote_portd, RC.nrmac[j]->eth_params_s.my_portd);
        printf("**************** RETURNED FROM configure_nfapi_vnf() vnf_port:%d\n", RC.nrmac[j]->eth_params_s.my_portc);
      } else if(strcmp(*(MacRLC_ParamList.paramarray[j][MACRLC_TRANSPORT_S_PREFERENCE_IDX].strptr), "aerial") == 0){
#ifdef ENABLE_AERIAL
        RC.nrmac[j]->nvipc_params_s.nvipc_shm_prefix =
            strdup(*(MacRLC_ParamList.paramarray[j][MACRLC_TRANSPORT_S_SHM_PREFIX].strptr));
        RC.nrmac[j]->nvipc_params_s.nvipc_poll_core = *(MacRLC_ParamList.paramarray[j][MACRLC_TRANSPORT_S_POLL_CORE].i8ptr);
        printf("Configuring VNF for Aerial connection with prefix %s\n", RC.nrmac[j]->eth_params_s.local_if_name);
        aerial_configure_nr_fapi_vnf();
#endif
      } else { // other midhaul
        AssertFatal(1 == 0, "MACRLC %d: %s unknown southbound midhaul\n", j, *(MacRLC_ParamList.paramarray[j][MACRLC_TRANSPORT_S_PREFERENCE_IDX].strptr));
      }
      RC.nrmac[j]->ulsch_max_frame_inactivity = *(MacRLC_ParamList.paramarray[j][MACRLC_ULSCH_MAX_FRAME_INACTIVITY].uptr);
      NR_bler_options_t *dl_bler_options = &RC.nrmac[j]->dl_bler;
      dl_bler_options->upper = *(MacRLC_ParamList.paramarray[j][MACRLC_DL_BLER_TARGET_UPPER_IDX].dblptr);
      dl_bler_options->lower = *(MacRLC_ParamList.paramarray[j][MACRLC_DL_BLER_TARGET_LOWER_IDX].dblptr);
      dl_bler_options->max_mcs = *(MacRLC_ParamList.paramarray[j][MACRLC_DL_MAX_MCS_IDX].u8ptr);
      if (config.disable_harq)
        dl_bler_options->harq_round_max = 1;
      else
        dl_bler_options->harq_round_max = *(MacRLC_ParamList.paramarray[j][MACRLC_DL_HARQ_ROUND_MAX_IDX].u8ptr);
      NR_bler_options_t *ul_bler_options = &RC.nrmac[j]->ul_bler;
      ul_bler_options->upper = *(MacRLC_ParamList.paramarray[j][MACRLC_UL_BLER_TARGET_UPPER_IDX].dblptr);
      ul_bler_options->lower = *(MacRLC_ParamList.paramarray[j][MACRLC_UL_BLER_TARGET_LOWER_IDX].dblptr);
      ul_bler_options->max_mcs = *(MacRLC_ParamList.paramarray[j][MACRLC_UL_MAX_MCS_IDX].u8ptr);
      if (config.disable_harq)
        ul_bler_options->harq_round_max = 1;
      else
        ul_bler_options->harq_round_max = *(MacRLC_ParamList.paramarray[j][MACRLC_UL_HARQ_ROUND_MAX_IDX].u8ptr);
      RC.nrmac[j]->min_grant_prb = *(MacRLC_ParamList.paramarray[j][MACRLC_MIN_GRANT_PRB_IDX].u8ptr);
      RC.nrmac[j]->min_grant_mcs = *(MacRLC_ParamList.paramarray[j][MACRLC_MIN_GRANT_MCS_IDX].u8ptr);
      RC.nrmac[j]->identity_pm = *(MacRLC_ParamList.paramarray[j][MACRLC_IDENTITY_PM_IDX].u8ptr);
      RC.nrmac[j]->num_ulprbbl = num_prbbl;
      memcpy(RC.nrmac[j]->ulprbbl, prbbl, 275 * sizeof(prbbl[0]));
      bool ab = *MacRLC_ParamList.paramarray[j][MACRLC_ANALOG_BEAMFORMING_IDX].u8ptr;
      if (ab) {
        NR_beam_info_t *beam_info = &RC.nrmac[j]->beam_info;
        int beams_per_period = *MacRLC_ParamList.paramarray[j][MACRLC_ANALOG_BEAMS_PERIOD_IDX].u8ptr;
        beam_info->beam_allocation = malloc16(beams_per_period * sizeof(int *));
        beam_info->beam_duration = *MacRLC_ParamList.paramarray[j][MACRLC_ANALOG_BEAM_DURATION_IDX].u8ptr;
        beam_info->beams_per_period = beams_per_period;
        beam_info->beam_allocation_size = -1; // to be initialized once we have information on frame configuration
      }
      // triggers also PHY initialization in case we have L1 via FAPI
      nr_mac_config_scc(RC.nrmac[j], scc, &config);
    } //  for (j=0;j<RC.nb_nr_macrlc_inst;j++)

    uint64_t gnb_du_id = 0;
    uint32_t gnb_id = 0;
    char *name = NULL;
    f1ap_served_cell_info_t info;
    read_du_cell_info(cfg, NODE_IS_DU(node_type), &gnb_id, &gnb_du_id, &name, &info, 1);

    if (get_softmodem_params()->sa) {
      nr_mac_configure_sib1(RC.nrmac[0], &info.plmn, info.nr_cellid, *info.tac);
      if (scc->ext2 && scc->ext2->ntn_Config_r17)
        nr_mac_configure_sib19(RC.nrmac[0]);
    }
    
    // read F1 Setup information from config and generated MIB/SIB1
    // and store it at MAC for sending later
    NR_BCCH_BCH_Message_t *mib = RC.nrmac[0]->common_channels[0].mib;
    const NR_BCCH_DL_SCH_Message_t *sib1 = RC.nrmac[0]->common_channels[0].sib1;
    f1ap_setup_req_t *req = RC_read_F1Setup(gnb_du_id, name, &info, scc, mib, sib1);
    AssertFatal(req != NULL, "could not read F1 Setup information\n");
    RC.nrmac[0]->f1_config.setup_req = req;
    RC.nrmac[0]->f1_config.gnb_id = gnb_id;

    free(name); /* read_du_cell_info() allocated memory */

  } else { // MacRLC_ParamList.numelt > 0
    LOG_E(PHY, "No %s configuration found\n", CONFIG_STRING_MACRLC_LIST);
    // AssertFatal (0,"No " CONFIG_STRING_MACRLC_LIST " configuration found");
  }
}

void config_security(gNB_RRC_INST *rrc)
{
  paramdef_t sec_params[] = SECURITY_GLOBALPARAMS_DESC;
  int ret = config_get(config_get_if(), sec_params, sizeofArray(sec_params), CONFIG_STRING_SECURITY);
  int i;

  if (ret < 0) {
    LOG_W(RRC, "configuration file does not contain a \"security\" section, applying default parameters (nia2 nea0, integrity disabled for DRBs)\n");
    rrc->security.ciphering_algorithms[0]    = 0;  /* nea0 = no ciphering */
    rrc->security.ciphering_algorithms_count = 1;
    rrc->security.integrity_algorithms[0]    = 2;  /* nia2 */
    rrc->security.integrity_algorithms[1]    = 0;  /* nia0 = no integrity, as a fallback (but nia2 should be supported by all UEs) */
    rrc->security.integrity_algorithms_count = 2;
    rrc->security.do_drb_ciphering           = 1;  /* even if nea0 let's activate so that we don't generate cipheringDisabled in pdcp_Config */
    rrc->security.do_drb_integrity           = 0;
    return;
  }

  if (sec_params[SECURITY_CONFIG_CIPHERING_IDX].numelt > 4) {
    LOG_E(RRC, "too much ciphering algorithms in section \"security\" of the configuration file, maximum is 4\n");
    exit(1);
  }
  if (sec_params[SECURITY_CONFIG_INTEGRITY_IDX].numelt > 4) {
    LOG_E(RRC, "too much integrity algorithms in section \"security\" of the configuration file, maximum is 4\n");
    exit(1);
  }

  /* get ciphering algorithms */
  rrc->security.ciphering_algorithms_count = 0;
  for (i = 0; i < sec_params[SECURITY_CONFIG_CIPHERING_IDX].numelt; i++) {
    if (!strcmp(sec_params[SECURITY_CONFIG_CIPHERING_IDX].strlistptr[i], "nea0")) {
      rrc->security.ciphering_algorithms[rrc->security.ciphering_algorithms_count] = 0;
      rrc->security.ciphering_algorithms_count++;
      continue;
    }
    if (!strcmp(sec_params[SECURITY_CONFIG_CIPHERING_IDX].strlistptr[i], "nea1")) {
      rrc->security.ciphering_algorithms[rrc->security.ciphering_algorithms_count] = 1;
      rrc->security.ciphering_algorithms_count++;
      continue;
    }
    if (!strcmp(sec_params[SECURITY_CONFIG_CIPHERING_IDX].strlistptr[i], "nea2")) {
      rrc->security.ciphering_algorithms[rrc->security.ciphering_algorithms_count] = 2;
      rrc->security.ciphering_algorithms_count++;
      continue;
    }
    if (!strcmp(sec_params[SECURITY_CONFIG_CIPHERING_IDX].strlistptr[i], "nea3")) {
      rrc->security.ciphering_algorithms[rrc->security.ciphering_algorithms_count] = 3;
      rrc->security.ciphering_algorithms_count++;
      continue;
    }
    LOG_E(RRC, "unknown ciphering algorithm \"%s\" in section \"security\" of the configuration file\n",
          sec_params[SECURITY_CONFIG_CIPHERING_IDX].strlistptr[i]);
    exit(1);
  }

  /* get integrity algorithms */
  rrc->security.integrity_algorithms_count = 0;
  for (i = 0; i < sec_params[SECURITY_CONFIG_INTEGRITY_IDX].numelt; i++) {
    if (!strcmp(sec_params[SECURITY_CONFIG_INTEGRITY_IDX].strlistptr[i], "nia0")) {
      rrc->security.integrity_algorithms[rrc->security.integrity_algorithms_count] = 0;
      rrc->security.integrity_algorithms_count++;
      continue;
    }
    if (!strcmp(sec_params[SECURITY_CONFIG_INTEGRITY_IDX].strlistptr[i], "nia1")) {
      rrc->security.integrity_algorithms[rrc->security.integrity_algorithms_count] = 1;
      rrc->security.integrity_algorithms_count++;
      continue;
    }
    if (!strcmp(sec_params[SECURITY_CONFIG_INTEGRITY_IDX].strlistptr[i], "nia2")) {
      rrc->security.integrity_algorithms[rrc->security.integrity_algorithms_count] = 2;
      rrc->security.integrity_algorithms_count++;
      continue;
    }
    if (!strcmp(sec_params[SECURITY_CONFIG_INTEGRITY_IDX].strlistptr[i], "nia3")) {
      rrc->security.integrity_algorithms[rrc->security.integrity_algorithms_count] = 3;
      rrc->security.integrity_algorithms_count++;
      continue;
    }
    LOG_E(RRC, "unknown integrity algorithm \"%s\" in section \"security\" of the configuration file\n",
          sec_params[SECURITY_CONFIG_INTEGRITY_IDX].strlistptr[i]);
    exit(1);
  }

  if (rrc->security.ciphering_algorithms_count == 0) {
    LOG_W(RRC, "no preferred ciphering algorithm set in configuration file, applying default parameters (no security)\n");
    rrc->security.ciphering_algorithms[0]    = 0;  /* nea0 = no ciphering */
    rrc->security.ciphering_algorithms_count = 1;
  }

  if (rrc->security.integrity_algorithms_count == 0) {
    LOG_W(RRC, "no preferred integrity algorithm set in configuration file, applying default parameters (nia2)\n");
    rrc->security.integrity_algorithms[0]    = 2;  /* nia2 */
    rrc->security.integrity_algorithms[1]    = 0;  /* nia0 = no integrity */
    rrc->security.integrity_algorithms_count = 2;
  }

  if (!strcmp(*sec_params[SECURITY_CONFIG_DO_DRB_CIPHERING_IDX].strptr, "yes")) {
    rrc->security.do_drb_ciphering = 1;
  } else if (!strcmp(*sec_params[SECURITY_CONFIG_DO_DRB_CIPHERING_IDX].strptr, "no")) {
    rrc->security.do_drb_ciphering = 0;
  } else {
    LOG_E(RRC, "in configuration file, bad drb_ciphering value '%s', only 'yes' and 'no' allowed\n",
          *sec_params[SECURITY_CONFIG_DO_DRB_CIPHERING_IDX].strptr);
    exit(1);
  }

  if (!strcmp(*sec_params[SECURITY_CONFIG_DO_DRB_INTEGRITY_IDX].strptr, "yes")) {
    rrc->security.do_drb_integrity = 1;
  } else if (!strcmp(*sec_params[SECURITY_CONFIG_DO_DRB_INTEGRITY_IDX].strptr, "no")) {
    rrc->security.do_drb_integrity = 0;
  } else {
    LOG_E(RRC, "in configuration file, bad drb_integrity value '%s', only 'yes' and 'no' allowed\n",
          *sec_params[SECURITY_CONFIG_DO_DRB_INTEGRITY_IDX].strptr);
    exit(1);
  }
}

static int sort_neighbour_cell_config_by_cell_id(const void *a, const void *b)
{
  const neighbour_cell_configuration_t *config_a = (const neighbour_cell_configuration_t *)a;
  const neighbour_cell_configuration_t *config_b = (const neighbour_cell_configuration_t *)b;

  if (config_a->nr_cell_id < config_b->nr_cell_id) {
    return -1;
  } else if (config_a->nr_cell_id > config_b->nr_cell_id) {
    return 1;
  }

  return 0;
}

static void fill_neighbour_cell_configuration(uint8_t gnb_idx, gNB_RRC_INST *rrc)
{
  char gnbpath[MAX_OPTNAME_SIZE + 8];
  sprintf(gnbpath, "%s.[%i]", GNB_CONFIG_STRING_GNB_LIST, gnb_idx);

  paramdef_t neighbour_list_params[] = GNB_NEIGHBOUR_LIST_PARAM_LIST;
  paramlist_def_t neighbour_list_param_list = {GNB_CONFIG_STRING_NEIGHBOUR_LIST, NULL, 0};

  config_getlist(config_get_if(), &neighbour_list_param_list, neighbour_list_params, sizeofArray(neighbour_list_params), gnbpath);
  if (neighbour_list_param_list.numelt < 1)
    return;

  rrc->neighbour_cell_configuration = malloc(sizeof(seq_arr_t));
  seq_arr_init(rrc->neighbour_cell_configuration, sizeof(neighbour_cell_configuration_t));

  for (int elm = 0; elm < neighbour_list_param_list.numelt; ++elm) {
    neighbour_cell_configuration_t *cell = calloc(1, sizeof(neighbour_cell_configuration_t));
    AssertFatal(cell != NULL, "out of memory\n");
    cell->nr_cell_id = (uint64_t)*neighbour_list_param_list.paramarray[elm][0].u64ptr;

    char neighbourpath[MAX_OPTNAME_SIZE + 8];
    sprintf(neighbourpath, "%s.[%i].%s.[%i]", GNB_CONFIG_STRING_GNB_LIST, gnb_idx, GNB_CONFIG_STRING_NEIGHBOUR_LIST, elm);
    paramdef_t NeighbourCellParams[] = GNBNEIGHBOURCELLPARAMS_DESC;
    paramlist_def_t NeighbourCellParamList = {GNB_CONFIG_STRING_NEIGHBOUR_CELL_LIST, NULL, 0};
    config_getlist(config_get_if(), &NeighbourCellParamList, NeighbourCellParams, sizeofArray(NeighbourCellParams), neighbourpath);
    LOG_D(GNB_APP, "HO LOG: For the Cell: %ld Neighbour Cell ELM NUM: %d\n", cell->nr_cell_id, NeighbourCellParamList.numelt);
    if (NeighbourCellParamList.numelt < 1)
      continue;

    cell->neighbour_cells = malloc(sizeof(seq_arr_t));
    AssertFatal(cell->neighbour_cells != NULL, "Memory exhausted!!!");
    seq_arr_init(cell->neighbour_cells, sizeof(nr_neighbour_gnb_configuration_t));
    for (int l = 0; l < NeighbourCellParamList.numelt; ++l) {
      nr_neighbour_gnb_configuration_t *neighbourCell = calloc(1, sizeof(nr_neighbour_gnb_configuration_t));
      AssertFatal(neighbourCell != NULL, "out of memory\n");
      neighbourCell->gNB_ID = *(NeighbourCellParamList.paramarray[l][GNB_CONFIG_N_CELL_GNB_ID_IDX].uptr);
      neighbourCell->nrcell_id = (uint64_t) * (NeighbourCellParamList.paramarray[l][GNB_CONFIG_N_CELL_NR_CELLID_IDX].u64ptr);
      neighbourCell->physicalCellId = *NeighbourCellParamList.paramarray[l][GNB_CONFIG_N_CELL_PHYSICAL_ID_IDX].uptr;
      neighbourCell->subcarrierSpacing = *NeighbourCellParamList.paramarray[l][GNB_CONFIG_N_CELL_SCS_IDX].uptr;
      neighbourCell->absoluteFrequencySSB = *NeighbourCellParamList.paramarray[l][GNB_CONFIG_N_CELL_ABS_FREQ_SSB_IDX].i64ptr;
      neighbourCell->tac = *NeighbourCellParamList.paramarray[l][GNB_CONFIG_N_CELL_TAC_IDX].uptr;

      char neighbour_plmn_path[CONFIG_MAXOPTLENGTH];
      sprintf(neighbour_plmn_path,
              "%s.%s.[%i].%s",
              neighbourpath,
              GNB_CONFIG_STRING_NEIGHBOUR_CELL_LIST,
              l,
              GNB_CONFIG_STRING_NEIGHBOUR_PLMN);

      paramdef_t NeighbourPlmn[] = GNBPLMNPARAMS_DESC;
      config_get(config_get_if(), NeighbourPlmn, sizeofArray(NeighbourPlmn), neighbour_plmn_path);

      neighbourCell->plmn.mcc = *NeighbourPlmn[GNB_MOBILE_COUNTRY_CODE_IDX].uptr;
      neighbourCell->plmn.mnc = *NeighbourPlmn[GNB_MOBILE_NETWORK_CODE_IDX].uptr;
      neighbourCell->plmn.mnc_digit_length = *NeighbourPlmn[GNB_MNC_DIGIT_LENGTH].uptr;
      seq_arr_push_back(cell->neighbour_cells, neighbourCell, sizeof(nr_neighbour_gnb_configuration_t));
    }

    seq_arr_push_back(rrc->neighbour_cell_configuration, cell, sizeof(neighbour_cell_configuration_t));
  }
  void *base = seq_arr_front(rrc->neighbour_cell_configuration);
  size_t nmemb = seq_arr_size(rrc->neighbour_cell_configuration);
  size_t element_size = sizeof(neighbour_cell_configuration_t);

  qsort(base, nmemb, element_size, sort_neighbour_cell_config_by_cell_id);
}

static void fill_measurement_configuration(uint8_t gnb_idx, gNB_RRC_INST *rrc)
{
  char measurement_path[MAX_OPTNAME_SIZE + 8];
  sprintf(measurement_path, "%s.[%i].%s", GNB_CONFIG_STRING_GNB_LIST, gnb_idx, GNB_CONFIG_STRING_MEASUREMENT_CONFIGURATION);

  nr_measurement_configuration_t *measurementConfig = &rrc->measurementConfiguration;
  // Periodical Event Configuration
  char periodic_event_path[MAX_OPTNAME_SIZE + 8];
  sprintf(periodic_event_path,
          "%s.[%i].%s.%s",
          GNB_CONFIG_STRING_GNB_LIST,
          gnb_idx,
          GNB_CONFIG_STRING_MEASUREMENT_CONFIGURATION,
          MEASUREMENT_EVENTS_PERIODICAL);
  paramdef_t Periodical_EventParams[] = MEASUREMENT_PERIODICAL_GLOBALPARAMS_DESC;
  config_get(config_get_if(), Periodical_EventParams, sizeofArray(Periodical_EventParams), periodic_event_path);
  if (*Periodical_EventParams[MEASUREMENT_EVENTS_ENABLE_IDX].i64ptr) {
    nr_per_event_t *periodic_event = (nr_per_event_t *)calloc(1, sizeof(nr_per_event_t));
    periodic_event->includeBeamMeasurements = *Periodical_EventParams[MEASUREMENT_EVENTS_INCLUDE_BEAM_MEAS_IDX].i64ptr;
    periodic_event->maxReportCells = *Periodical_EventParams[MEASUREMENT_EVENTS_MAX_RS_INDEX_TO_REPORT].i64ptr;

    measurementConfig->per_event = periodic_event;
  }

  // A2 Event Configuration
  char a2_path[MAX_OPTNAME_SIZE + 8];
  sprintf(a2_path,
          "%s.[%i].%s.%s",
          GNB_CONFIG_STRING_GNB_LIST,
          gnb_idx,
          GNB_CONFIG_STRING_MEASUREMENT_CONFIGURATION,
          MEASUREMENT_EVENTS_A2);
  paramdef_t A2_EventParams[] = MEASUREMENT_A2_GLOBALPARAMS_DESC;
  config_get(config_get_if(), A2_EventParams, sizeofArray(A2_EventParams), a2_path);
  if (*A2_EventParams[MEASUREMENT_EVENTS_ENABLE_IDX].i64ptr) {
    nr_a2_event_t *a2_event = (nr_a2_event_t *)calloc(1, sizeof(nr_a2_event_t));
    a2_event->threshold_RSRP = *A2_EventParams[MEASUREMENT_EVENTS_A2_THRESHOLD_IDX].i64ptr;
    a2_event->timeToTrigger = *A2_EventParams[MEASUREMENT_EVENTS_TIMETOTRIGGER_IDX].i64ptr;

    measurementConfig->a2_event = a2_event;
  }
  // A3 Event Configuration
  paramlist_def_t A3_EventList = {MEASUREMENT_EVENTS_A3, NULL, 0};
  paramdef_t A3_EventParams[] = MEASUREMENT_A3_GLOBALPARAMS_DESC;
  config_getlist(config_get_if(), &A3_EventList, A3_EventParams, sizeofArray(A3_EventParams), measurement_path);
  LOG_D(GNB_APP, "HO LOG: A3 Configuration Exists: %d\n", A3_EventList.numelt);

  if (A3_EventList.numelt < 1)
    return;

  measurementConfig->a3_event_list = malloc(sizeof(seq_arr_t));
  seq_arr_init(measurementConfig->a3_event_list, sizeof(nr_a3_event_t));
  for (int i = 0; i < A3_EventList.numelt; i++) {
    nr_a3_event_t *a3_event = (nr_a3_event_t *)calloc(1, sizeof(nr_a3_event_t));
    AssertFatal(a3_event != NULL, "out of memory\n");
    a3_event->pci = *A3_EventList.paramarray[i][MEASUREMENT_EVENTS_PCI_ID_IDX].i64ptr;
    AssertFatal(a3_event->pci >= -1 && a3_event->pci < 1024,
                "entry %s.%s must be -1<=PCI<1024, but is %d\n",
                measurement_path,
                MEASUREMENT_EVENTS_PCI_ID,
                a3_event->pci);
    a3_event->timeToTrigger = *A3_EventList.paramarray[i][MEASUREMENT_EVENTS_TIMETOTRIGGER_IDX].i64ptr;
    a3_event->a3_offset = *A3_EventList.paramarray[i][MEASUREMENT_EVENTS_OFFSET_IDX].i64ptr;
    a3_event->hysteresis = *A3_EventList.paramarray[i][MEASUREMENT_EVENTS_HYSTERESIS_IDX].i64ptr;

    if (a3_event->pci == -1)
      measurementConfig->is_default_a3_configuration_exists = true;

    seq_arr_push_back(measurementConfig->a3_event_list, a3_event, sizeof(nr_a3_event_t));
  }
}

void RCconfig_NRRRC(gNB_RRC_INST *rrc)
{

  int num_gnbs = 0;
  char aprefix[MAX_OPTNAME_SIZE*2 + 8];
  int32_t gnb_id = 0;
  int k = 0;
  int i = 0;

  paramdef_t GNBSParams[] = GNBSPARAMS_DESC;
  ////////// Identification parameters
  paramdef_t GNBParams[]  = GNBPARAMS_DESC;
  paramlist_def_t GNBParamList = {GNB_CONFIG_STRING_GNB_LIST,NULL,0};

  ////////// Physical parameters

  /* get global parameters, defined outside any section in the config file */

  config_get(config_get_if(), GNBSParams, sizeofArray(GNBSParams), NULL);
  num_gnbs = GNBSParams[GNB_ACTIVE_GNBS_IDX].numelt;
  AssertFatal (i < num_gnbs,"Failed to parse config file no %dth element in %s \n", i, GNB_CONFIG_STRING_ACTIVE_GNBS);
  AssertFatal(num_gnbs == 1, "required section \"gNBs\" not in config!\n");

  if (num_gnbs > 0) {

    // Output a list of all gNBs. ////////// Identification parameters
    config_getlist(config_get_if(), &GNBParamList, GNBParams, sizeofArray(GNBParams), NULL);
    if (GNBParamList.paramarray[i][GNB_GNB_ID_IDX].uptr == NULL) {
    // Calculate a default gNB ID
      if (get_softmodem_params()->sa) { 
        uint32_t hash;
        hash = ngap_generate_gNB_id ();
        gnb_id = i + (hash & 0xFFFFFF8);
      } else {
        gnb_id = i;
      }
    } else {
      gnb_id = *(GNBParamList.paramarray[i][GNB_GNB_ID_IDX].uptr);
    }

    sprintf(aprefix, "%s.[%i]", GNB_CONFIG_STRING_GNB_LIST, 0);

    printf("NRRRC %d: Southbound Transport %s\n", i, *(GNBParamList.paramarray[i][GNB_TRANSPORT_S_PREFERENCE_IDX].strptr));

    rrc->node_type = get_node_type();
    rrc->node_id        = gnb_id;
    if (NODE_IS_CU(rrc->node_type)) {
      paramdef_t SCTPParams[]  = GNBSCTPPARAMS_DESC;
      char aprefix[MAX_OPTNAME_SIZE*2 + 8];
      sprintf(aprefix,"%s.[%d].%s", GNB_CONFIG_STRING_GNB_LIST, i, GNB_CONFIG_STRING_SCTP_CONFIG);
      config_get(config_get_if(), SCTPParams, sizeofArray(SCTPParams), aprefix);
      LOG_I(GNB_APP,"F1AP: gNB_CU_id[%d] %d\n", k, rrc->node_id);
      rrc->node_name = strdup(*(GNBParamList.paramarray[0][GNB_GNB_NAME_IDX].strptr));
      LOG_I(GNB_APP,"F1AP: gNB_CU_name[%d] %s\n", k, rrc->node_name);
      rrc->eth_params_s.my_addr                  = strdup(*(GNBParamList.paramarray[i][GNB_LOCAL_S_ADDRESS_IDX].strptr));
      rrc->eth_params_s.remote_addr              = strdup(*(GNBParamList.paramarray[i][GNB_REMOTE_S_ADDRESS_IDX].strptr));
      rrc->eth_params_s.my_portc                 = *(GNBParamList.paramarray[i][GNB_LOCAL_S_PORTC_IDX].uptr);
      rrc->eth_params_s.remote_portc             = *(GNBParamList.paramarray[i][GNB_REMOTE_S_PORTC_IDX].uptr);
      rrc->eth_params_s.my_portd                 = *(GNBParamList.paramarray[i][GNB_LOCAL_S_PORTD_IDX].uptr);
      rrc->eth_params_s.remote_portd             = *(GNBParamList.paramarray[i][GNB_REMOTE_S_PORTD_IDX].uptr);
      rrc->eth_params_s.transp_preference        = ETH_UDP_MODE;
    }

   

    rrc->nr_cellid        = (uint64_t)*(GNBParamList.paramarray[i][GNB_NRCELLID_IDX].u64ptr);

    if (strcmp(*(GNBParamList.paramarray[i][GNB_TRANSPORT_S_PREFERENCE_IDX].strptr), "local_mac") == 0) {
      
    } else if (strcmp(*(GNBParamList.paramarray[i][GNB_TRANSPORT_S_PREFERENCE_IDX].strptr), "cudu") == 0) {
      rrc->eth_params_s.my_addr                  = strdup(*(GNBParamList.paramarray[i][GNB_LOCAL_S_ADDRESS_IDX].strptr));
      rrc->eth_params_s.remote_addr              = strdup(*(GNBParamList.paramarray[i][GNB_REMOTE_S_ADDRESS_IDX].strptr));
      rrc->eth_params_s.my_portc                 = *(GNBParamList.paramarray[i][GNB_LOCAL_S_PORTC_IDX].uptr);
      rrc->eth_params_s.remote_portc             = *(GNBParamList.paramarray[i][GNB_REMOTE_S_PORTC_IDX].uptr);
      rrc->eth_params_s.my_portd                 = *(GNBParamList.paramarray[i][GNB_LOCAL_S_PORTD_IDX].uptr);
      rrc->eth_params_s.remote_portd             = *(GNBParamList.paramarray[i][GNB_REMOTE_S_PORTD_IDX].uptr);
      rrc->eth_params_s.transp_preference        = ETH_UDP_MODE;
    } else { // other midhaul
    }       
    
    // search if in active list
    
    gNB_RrcConfigurationReq nrrrc_config = {0};
    for (k=0; k <num_gnbs ; k++) {
      if (strcmp(GNBSParams[GNB_ACTIVE_GNBS_IDX].strlistptr[k], *(GNBParamList.paramarray[i][GNB_GNB_NAME_IDX].strptr) )== 0) {
	
        char gnbpath[MAX_OPTNAME_SIZE + 8];
        sprintf(gnbpath,"%s.[%i]",GNB_CONFIG_STRING_GNB_LIST,k);

        fill_neighbour_cell_configuration(k, rrc);

        fill_measurement_configuration(k, rrc);

        paramdef_t PLMNParams[] = GNBPLMNPARAMS_DESC;

        paramlist_def_t PLMNParamList = {GNB_CONFIG_STRING_PLMN_LIST, NULL, 0};
        /* map parameter checking array instances to parameter definition array instances */
        checkedparam_t config_check_PLMNParams [] = PLMNPARAMS_CHECK;

        for (int I = 0; I < sizeofArray(PLMNParams); ++I)
          PLMNParams[I].chkPptr = &(config_check_PLMNParams[I]);

        nrrrc_config.tac               = *GNBParamList.paramarray[i][GNB_TRACKING_AREA_CODE_IDX].uptr;
        AssertFatal(!GNBParamList.paramarray[i][GNB_MOBILE_COUNTRY_CODE_IDX_OLD].strptr
                    && !GNBParamList.paramarray[i][GNB_MOBILE_NETWORK_CODE_IDX_OLD].strptr,
                    "It seems that you use an old configuration file. Please change the existing\n"
                    "    tracking_area_code  =  \"1\";\n"
                    "    mobile_country_code =  \"208\";\n"
                    "    mobile_network_code =  \"93\";\n"
                    "to\n"
                    "    tracking_area_code  =  1; // no string!!\n"
                    "    plmn_list = ( { mcc = 208; mnc = 93; mnc_length = 2; } )\n");
        config_getlist(config_get_if(), &PLMNParamList, PLMNParams, sizeofArray(PLMNParams), gnbpath);

        if (PLMNParamList.numelt < 1 || PLMNParamList.numelt > 6)
          AssertFatal(0, "The number of PLMN IDs must be in [1,6], but is %d\n",
                      PLMNParamList.numelt);

        nrrrc_config.num_plmn = PLMNParamList.numelt;

        for (int l = 0; l < PLMNParamList.numelt; ++l) {
	
	  nrrrc_config.mcc[l]               = *PLMNParamList.paramarray[l][GNB_MOBILE_COUNTRY_CODE_IDX].uptr;
	  nrrrc_config.mnc[l]               = *PLMNParamList.paramarray[l][GNB_MOBILE_NETWORK_CODE_IDX].uptr;
	  nrrrc_config.mnc_digit_length[l]  = *PLMNParamList.paramarray[l][GNB_MNC_DIGIT_LENGTH].u8ptr;
	  AssertFatal((nrrrc_config.mnc_digit_length[l] == 2) ||
		      (nrrrc_config.mnc_digit_length[l] == 3),"BAD MNC DIGIT LENGTH %d",
		      nrrrc_config.mnc_digit_length[l]);
        }
        nrrrc_config.enable_sdap = *GNBParamList.paramarray[i][GNB_ENABLE_SDAP_IDX].iptr;
        LOG_I(GNB_APP, "SDAP layer is %s\n", nrrrc_config.enable_sdap ? "enabled" : "disabled");
        nrrrc_config.drbs = *GNBParamList.paramarray[i][GNB_DRBS].iptr;
        nrrrc_config.um_on_default_drb = *(GNBParamList.paramarray[i][GNB_UMONDEFAULTDRB_IDX].uptr);
        LOG_I(GNB_APP, "Data Radio Bearer count %d\n", nrrrc_config.drbs);

      }//
    }//End for (k=0; k <num_gnbs ; k++)
    openair_rrc_gNB_configuration(rrc, &nrrrc_config);
  }//End if (num_gnbs>0)

  config_security(rrc);
}//End RCconfig_NRRRC function

int RCconfig_NR_NG(MessageDef *msg_p, uint32_t i) {

  int               j,k = 0;
  int               gnb_id;
  int32_t           my_int;
  const char*       active_gnb[MAX_GNB];
  char             *address                       = NULL;
  char             *cidr                          = NULL;

  // for no gcc warnings 

  (void)  my_int;

  memset((char*)active_gnb,0,MAX_GNB* sizeof(char*));
  char*             gnb_ipv4_address_for_NGU      = NULL;
  uint32_t          gnb_port_for_NGU              = 0;
  char*             gnb_ipv4_address_for_S1U      = NULL;
  uint32_t          gnb_port_for_S1U              = 0;

  paramdef_t GNBSParams[] = GNBSPARAMS_DESC;
  paramdef_t GNBParams[]  = GNBPARAMS_DESC;
  paramlist_def_t GNBParamList = {GNB_CONFIG_STRING_GNB_LIST,NULL,0};

  /* get global parameters, defined outside any section in the config file */
  config_get(config_get_if(), GNBSParams, sizeofArray(GNBSParams), NULL);

  AssertFatal (i<GNBSParams[GNB_ACTIVE_GNBS_IDX].numelt,
     "Failed to parse config file %s, %uth attribute %s \n",
     RC.config_file_name, i, GNB_CONFIG_STRING_ACTIVE_GNBS);
    
  
  if (GNBSParams[GNB_ACTIVE_GNBS_IDX].numelt>0) {
    // Output a list of all gNBs.
    config_getlist(config_get_if(), &GNBParamList, GNBParams, sizeofArray(GNBParams), NULL);
    if (GNBParamList.numelt > 0) {
      for (k = 0; k < GNBParamList.numelt; k++) {
        if (GNBParamList.paramarray[k][GNB_GNB_ID_IDX].uptr == NULL) {
          // Calculate a default gNB ID
          if (get_softmodem_params()->sa) {
            uint32_t hash;
          
          hash = ngap_generate_gNB_id ();
          gnb_id = k + (hash & 0xFFFFFF8);
          } else {
            gnb_id = k;
          }
        } else {
          gnb_id = *(GNBParamList.paramarray[k][GNB_GNB_ID_IDX].uptr);
        }
  
  
        // search if in active list
        for (j=0; j < GNBSParams[GNB_ACTIVE_GNBS_IDX].numelt; j++) {
          if (strcmp(GNBSParams[GNB_ACTIVE_GNBS_IDX].strlistptr[j], *(GNBParamList.paramarray[k][GNB_GNB_NAME_IDX].strptr)) == 0) {
            paramdef_t PLMNParams[] = GNBPLMNPARAMS_DESC;
            paramlist_def_t PLMNParamList = {GNB_CONFIG_STRING_PLMN_LIST, NULL, 0};
            paramdef_t SNSSAIParams[] = GNBSNSSAIPARAMS_DESC;
            paramlist_def_t SNSSAIParamList = {GNB_CONFIG_STRING_SNSSAI_LIST, NULL, 0};
            /* map parameter checking array instances to parameter definition array instances */
            checkedparam_t config_check_PLMNParams [] = PLMNPARAMS_CHECK;
            checkedparam_t config_check_SNSSAIParams [] = SNSSAIPARAMS_CHECK;

            for (int I = 0; I < sizeofArray(PLMNParams); ++I)
              PLMNParams[I].chkPptr = &(config_check_PLMNParams[I]);
            for (int J = 0; J < sizeofArray(SNSSAIParams); ++J)
              SNSSAIParams[J].chkPptr = &(config_check_SNSSAIParams[J]);

            paramdef_t NGParams[]  = GNBNGPARAMS_DESC;
            paramlist_def_t NGParamList = {GNB_CONFIG_STRING_AMF_IP_ADDRESS,NULL,0};
            
            paramdef_t SCTPParams[]  = GNBSCTPPARAMS_DESC;
            paramdef_t NETParams[]  =  GNBNETPARAMS_DESC;
            char aprefix[MAX_OPTNAME_SIZE*2 + 8];
            sprintf(aprefix, "%s.[%i]", GNB_CONFIG_STRING_GNB_LIST, k);
            
            NGAP_REGISTER_GNB_REQ (msg_p).gNB_id = gnb_id;
            
            if (strcmp(*(GNBParamList.paramarray[k][GNB_CELL_TYPE_IDX].strptr), "CELL_MACRO_GNB") == 0) {
              NGAP_REGISTER_GNB_REQ (msg_p).cell_type = CELL_MACRO_GNB;
            } else  if (strcmp(*(GNBParamList.paramarray[k][GNB_CELL_TYPE_IDX].strptr), "CELL_HOME_GNB") == 0) {
              NGAP_REGISTER_GNB_REQ (msg_p).cell_type = CELL_HOME_ENB;
            } else {
              AssertFatal (0,
              "Failed to parse gNB configuration file %s, gnb %u unknown value \"%s\" for cell_type choice: CELL_MACRO_GNB or CELL_HOME_GNB !\n",
              RC.config_file_name, i, *(GNBParamList.paramarray[k][GNB_CELL_TYPE_IDX].strptr));
            }
            
            NGAP_REGISTER_GNB_REQ (msg_p).gNB_name         = strdup(*(GNBParamList.paramarray[k][GNB_GNB_NAME_IDX].strptr));
            NGAP_REGISTER_GNB_REQ (msg_p).tac              = *GNBParamList.paramarray[k][GNB_TRACKING_AREA_CODE_IDX].uptr;
            AssertFatal(!GNBParamList.paramarray[k][GNB_MOBILE_COUNTRY_CODE_IDX_OLD].strptr
                        && !GNBParamList.paramarray[k][GNB_MOBILE_NETWORK_CODE_IDX_OLD].strptr,
                        "It seems that you use an old configuration file. Please change the existing\n"
                        "    tracking_area_code  =  \"1\";\n"
                        "    mobile_country_code =  \"208\";\n"
                        "    mobile_network_code =  \"93\";\n"
                        "to\n"
                        "    tracking_area_code  =  1; // no string!!\n"
                        "    plmn_list = ( { mcc = 208; mnc = 93; mnc_length = 2; } )\n");
            config_getlist(config_get_if(), &PLMNParamList, PLMNParams, sizeofArray(PLMNParams), aprefix);

            if (PLMNParamList.numelt < 1 || PLMNParamList.numelt > 6)
              AssertFatal(0, "The number of PLMN IDs must be in [1,6], but is %d\n",
                          PLMNParamList.numelt);

            NGAP_REGISTER_GNB_REQ(msg_p).num_plmn = PLMNParamList.numelt;

            for (int l = 0; l < PLMNParamList.numelt; ++l) {
              char snssaistr[MAX_OPTNAME_SIZE*2 + 8];
              sprintf(snssaistr, "%s.[%i].%s.[%i]", GNB_CONFIG_STRING_GNB_LIST, k, GNB_CONFIG_STRING_PLMN_LIST, l);
              config_getlist(config_get_if(), &SNSSAIParamList, SNSSAIParams, sizeofArray(SNSSAIParams), snssaistr);

              NGAP_REGISTER_GNB_REQ(msg_p).plmn[l].mcc = *PLMNParamList.paramarray[l][GNB_MOBILE_COUNTRY_CODE_IDX].uptr;
              NGAP_REGISTER_GNB_REQ(msg_p).plmn[l].mnc = *PLMNParamList.paramarray[l][GNB_MOBILE_NETWORK_CODE_IDX].uptr;
              NGAP_REGISTER_GNB_REQ(msg_p).plmn[l].mnc_digit_length = *PLMNParamList.paramarray[l][GNB_MNC_DIGIT_LENGTH].u8ptr;
              NGAP_REGISTER_GNB_REQ (msg_p).default_drx      = 0;
              AssertFatal((NGAP_REGISTER_GNB_REQ(msg_p).plmn[l].mnc_digit_length == 2)
                              || (NGAP_REGISTER_GNB_REQ(msg_p).plmn[l].mnc_digit_length == 3),
                          "BAD MNC DIGIT LENGTH %d",
                          NGAP_REGISTER_GNB_REQ(msg_p).plmn[l].mnc_digit_length);

              NGAP_REGISTER_GNB_REQ(msg_p).plmn[l].num_nssai = SNSSAIParamList.numelt;
              for (int s = 0; s < SNSSAIParamList.numelt; ++s) {
                NGAP_REGISTER_GNB_REQ(msg_p).plmn[l].s_nssai[s].sst =
                    *SNSSAIParamList.paramarray[s][GNB_SLICE_SERVICE_TYPE_IDX].uptr;
                // SD is optional
                // 0xffffff is "no SD", see 23.003 Sec 28.4.2
                NGAP_REGISTER_GNB_REQ(msg_p).plmn[l].s_nssai[s].sd =
                    (*SNSSAIParamList.paramarray[s][GNB_SLICE_DIFFERENTIATOR_IDX].uptr & 0xffffff);
              }
            }
            sprintf(aprefix,"%s.[%i]",GNB_CONFIG_STRING_GNB_LIST,k);
            config_getlist(config_get_if(), &NGParamList, NGParams, sizeofArray(NGParams), aprefix);

            NGAP_REGISTER_GNB_REQ (msg_p).nb_amf = 0;
            
            for (int l = 0; l < NGParamList.numelt; l++) {
              
              NGAP_REGISTER_GNB_REQ (msg_p).nb_amf += 1;
              strcpy(NGAP_REGISTER_GNB_REQ (msg_p).amf_ip_address[l].ipv4_address,*(NGParamList.paramarray[l][GNB_AMF_IPV4_ADDRESS_IDX].strptr));
              NGAP_REGISTER_GNB_REQ (msg_p).amf_ip_address[j].ipv4 = 1;
              NGAP_REGISTER_GNB_REQ (msg_p).amf_ip_address[j].ipv6 = 0;

              /* not in configuration yet ...
              if (NGParamList.paramarray[l][GNB_AMF_BROADCAST_PLMN_INDEX].iptr)
                NGAP_REGISTER_GNB_REQ(msg_p).broadcast_plmn_num[l] = NGParamList.paramarray[l][GNB_AMF_BROADCAST_PLMN_INDEX].numelt;
              else
                NGAP_REGISTER_GNB_REQ(msg_p).broadcast_plmn_num[l] = 0;

              AssertFatal(NGAP_REGISTER_GNB_REQ(msg_p).broadcast_plmn_num[l] <= NGAP_REGISTER_GNB_REQ(msg_p).num_plmn,
                          "List of broadcast PLMN to be sent to AMF can not be longer than actual "
                          "PLMN list (max %d, but is %d)\n",
                          NGAP_REGISTER_GNB_REQ(msg_p).num_plmn,
                          NGAP_REGISTER_GNB_REQ(msg_p).broadcast_plmn_num[l]);

              for (int el = 0; el < NGAP_REGISTER_GNB_REQ(msg_p).broadcast_plmn_num[l]; ++el) {
                // UINTARRAY gets mapped to int, see config_libconfig.c:223
                NGAP_REGISTER_GNB_REQ(msg_p).broadcast_plmn_index[l][el] = NGParamList.paramarray[l][GNB_AMF_BROADCAST_PLMN_INDEX].iptr[el];
                AssertFatal(NGAP_REGISTER_GNB_REQ(msg_p).broadcast_plmn_index[l][el] >= 0
                            && NGAP_REGISTER_GNB_REQ(msg_p).broadcast_plmn_index[l][el] < NGAP_REGISTER_GNB_REQ(msg_p).num_plmn,
                            "index for AMF's MCC/MNC (%d) is an invalid index for the registered PLMN IDs (%d)\n",
                            NGAP_REGISTER_GNB_REQ(msg_p).broadcast_plmn_index[l][el],
                            NGAP_REGISTER_GNB_REQ(msg_p).num_plmn);
              }
              */

              /* if no broadcasst_plmn array is defined, fill default values */
              if (NGAP_REGISTER_GNB_REQ(msg_p).broadcast_plmn_num[l] == 0) {
                NGAP_REGISTER_GNB_REQ(msg_p).broadcast_plmn_num[l] = NGAP_REGISTER_GNB_REQ(msg_p).num_plmn;

                for (int el = 0; el < NGAP_REGISTER_GNB_REQ(msg_p).num_plmn; ++el)
                  NGAP_REGISTER_GNB_REQ(msg_p).broadcast_plmn_index[l][el] = el;
              }
              
            }

          
            // SCTP SETTING
            NGAP_REGISTER_GNB_REQ (msg_p).sctp_out_streams = SCTP_OUT_STREAMS;
            NGAP_REGISTER_GNB_REQ (msg_p).sctp_in_streams  = SCTP_IN_STREAMS;
            if (get_softmodem_params()->sa) {
              sprintf(aprefix,"%s.[%i].%s",GNB_CONFIG_STRING_GNB_LIST,k,GNB_CONFIG_STRING_SCTP_CONFIG);
              config_get(config_get_if(), SCTPParams, sizeofArray(SCTPParams), aprefix);
              NGAP_REGISTER_GNB_REQ (msg_p).sctp_in_streams = (uint16_t)*(SCTPParams[GNB_SCTP_INSTREAMS_IDX].uptr);
              NGAP_REGISTER_GNB_REQ (msg_p).sctp_out_streams = (uint16_t)*(SCTPParams[GNB_SCTP_OUTSTREAMS_IDX].uptr);
            }

            sprintf(aprefix,"%s.[%i].%s",GNB_CONFIG_STRING_GNB_LIST,k,GNB_CONFIG_STRING_NETWORK_INTERFACES_CONFIG);
            // NETWORK_INTERFACES
            config_get(config_get_if(), NETParams, sizeofArray(NETParams), aprefix);

            //    NGAP_REGISTER_GNB_REQ (msg_p).enb_interface_name_for_NGU = strdup(enb_interface_name_for_NGU);
            cidr = *(NETParams[GNB_IPV4_ADDRESS_FOR_NG_AMF_IDX].strptr);
            char *save = NULL;
            address = strtok_r(cidr, "/", &save);
            
            NGAP_REGISTER_GNB_REQ (msg_p).gnb_ip_address.ipv6 = 0;
            NGAP_REGISTER_GNB_REQ (msg_p).gnb_ip_address.ipv4 = 1;
            
            strcpy(NGAP_REGISTER_GNB_REQ (msg_p).gnb_ip_address.ipv4_address, address);
            
            break;
          }
        }
      }
    }
  }
  return 0;
}

void NRRCConfig(void) {

  paramlist_def_t MACRLCParamList = {CONFIG_STRING_MACRLC_LIST,NULL,0};
  paramlist_def_t L1ParamList     = {CONFIG_STRING_L1_LIST,NULL,0};
  paramlist_def_t RUParamList     = {CONFIG_STRING_RU_LIST,NULL,0};
  paramdef_t GNBSParams[]         = GNBSPARAMS_DESC;
  
/* get global parameters, defined outside any section in the config file */

  LOG_I(GNB_APP, "Getting GNBSParams\n");

  config_get(config_get_if(), GNBSParams, sizeofArray(GNBSParams), NULL);
  RC.nb_nr_inst = GNBSParams[GNB_ACTIVE_GNBS_IDX].numelt;

  // Get num MACRLC instances
  config_getlist(config_get_if(), &MACRLCParamList, NULL, 0, NULL);
  RC.nb_nr_macrlc_inst  = MACRLCParamList.numelt;
  // Get num L1 instances
  config_getlist(config_get_if(), &L1ParamList, NULL, 0, NULL);
  RC.nb_nr_L1_inst = L1ParamList.numelt;
  
  // Get num RU instances
  config_getlist(config_get_if(), &RUParamList, NULL, 0, NULL);
  RC.nb_RU     = RUParamList.numelt; 
}


int RCconfig_NR_X2(MessageDef *msg_p, uint32_t i) {
  int   J, l;
  char *address = NULL;
  char *cidr    = NULL;
  //int                    num_gnbs                                                      = 0;
  //int                    num_component_carriers                                        = 0;
  int                    j,k                                                           = 0;
  int32_t                gnb_id                                                        = 0;

  paramdef_t GNBSParams[] = GNBSPARAMS_DESC;
  ////////// Identification parameters
  paramdef_t GNBParams[]  = GNBPARAMS_DESC;
  paramlist_def_t GNBParamList = {GNB_CONFIG_STRING_GNB_LIST,NULL,0};
  /* get global parameters, defined outside any section in the config file */
  config_get(config_get_if(), GNBSParams, sizeofArray(GNBSParams), NULL);
  NR_ServingCellConfigCommon_t *scc = calloc_or_fail(1, sizeof(*scc));
  uint64_t ssb_bitmap=0xff;
  memset((void*)scc,0,sizeof(NR_ServingCellConfigCommon_t));
  prepare_scc(scc);
  paramdef_t SCCsParams[] = SCCPARAMS_DESC(scc);
  paramlist_def_t SCCsParamList = {GNB_CONFIG_STRING_SERVINGCELLCONFIGCOMMON, NULL, 0};

  AssertFatal(i < GNBSParams[GNB_ACTIVE_GNBS_IDX].numelt,
              "Failed to parse config file %s, %uth attribute %s \n",
              RC.config_file_name, i, GNB_CONFIG_STRING_ACTIVE_GNBS);

  if (GNBSParams[GNB_ACTIVE_GNBS_IDX].numelt > 0) {
    // Output a list of all gNBs.
    config_getlist(config_get_if(), &GNBParamList, GNBParams, sizeofArray(GNBParams), NULL);

    if (GNBParamList.numelt > 0) {
      for (k = 0; k < GNBParamList.numelt; k++) {
        if (GNBParamList.paramarray[k][GNB_GNB_ID_IDX].uptr == NULL) {
          // Calculate a default eNB ID
          if (get_softmodem_params()->sa) {
            uint32_t hash;
            hash = ngap_generate_gNB_id ();
            gnb_id = k + (hash & 0xFFFFFF8);
          } else {
            gnb_id = k;
          }
        } else {
          gnb_id = *(GNBParamList.paramarray[k][GNB_GNB_ID_IDX].uptr);
        }

        // search if in active list
        for (j = 0; j < GNBSParams[GNB_ACTIVE_GNBS_IDX].numelt; j++) {
          if (strcmp(GNBSParams[GNB_ACTIVE_GNBS_IDX].strlistptr[j], *(GNBParamList.paramarray[k][GNB_GNB_NAME_IDX].strptr)) == 0) {
            paramdef_t PLMNParams[] = GNBPLMNPARAMS_DESC;
            paramlist_def_t PLMNParamList = {GNB_CONFIG_STRING_PLMN_LIST, NULL, 0};
            /* map parameter checking array instances to parameter definition array instances */
            checkedparam_t config_check_PLMNParams [] = PLMNPARAMS_CHECK;

            for (int I = 0; I < sizeofArray(PLMNParams); ++I)
              PLMNParams[I].chkPptr = &(config_check_PLMNParams[I]);

            paramdef_t X2Params[]  = X2PARAMS_DESC;
            paramlist_def_t X2ParamList = {ENB_CONFIG_STRING_TARGET_ENB_X2_IP_ADDRESS,NULL,0};
            paramdef_t SCTPParams[]  = GNBSCTPPARAMS_DESC;
	    char*             gnb_ipv4_address_for_NGU      = NULL;
	    uint32_t          gnb_port_for_NGU              = 0;
	    char*             gnb_ipv4_address_for_S1U      = NULL;
	    uint32_t          gnb_port_for_S1U              = 0;

            paramdef_t NETParams[]  =  GNBNETPARAMS_DESC;
            /* TODO: fix the size - if set lower we have a crash (MAX_OPTNAME_SIZE was 64 when this code was written) */
            /* this is most probably a problem with the config module */
            char aprefix[MAX_OPTNAME_SIZE*80 + 8];
            sprintf(aprefix,"%s.[%i]",GNB_CONFIG_STRING_GNB_LIST,k);
            /* Some default/random parameters */
            X2AP_REGISTER_ENB_REQ (msg_p).eNB_id = gnb_id;

            if (strcmp(*(GNBParamList.paramarray[k][GNB_CELL_TYPE_IDX].strptr), "CELL_MACRO_GNB") == 0) {
              X2AP_REGISTER_ENB_REQ (msg_p).cell_type = CELL_MACRO_GNB;
            }else {
              AssertFatal (0,
                           "Failed to parse eNB configuration file %s, enb %u unknown value \"%s\" for cell_type choice: CELL_MACRO_ENB or CELL_HOME_ENB !\n",
                           RC.config_file_name, i, *(GNBParamList.paramarray[k][GNB_CELL_TYPE_IDX].strptr));
            }

            X2AP_REGISTER_ENB_REQ (msg_p).eNB_name         = strdup(*(GNBParamList.paramarray[k][GNB_GNB_NAME_IDX].strptr));
            X2AP_REGISTER_ENB_REQ (msg_p).tac              = *GNBParamList.paramarray[k][GNB_TRACKING_AREA_CODE_IDX].uptr;
            config_getlist(config_get_if(), &PLMNParamList, PLMNParams, sizeofArray(PLMNParams), aprefix);

            if (PLMNParamList.numelt < 1 || PLMNParamList.numelt > 6)
              AssertFatal(0, "The number of PLMN IDs must be in [1,6], but is %d\n",
                          PLMNParamList.numelt);

            if (PLMNParamList.numelt > 1)
              LOG_W(X2AP, "X2AP currently handles only one PLMN, ignoring the others!\n");

            X2AP_REGISTER_ENB_REQ (msg_p).mcc = *PLMNParamList.paramarray[0][GNB_MOBILE_COUNTRY_CODE_IDX].uptr;
            X2AP_REGISTER_ENB_REQ (msg_p).mnc = *PLMNParamList.paramarray[0][GNB_MOBILE_NETWORK_CODE_IDX].uptr;
            X2AP_REGISTER_ENB_REQ (msg_p).mnc_digit_length = *PLMNParamList.paramarray[0][GNB_MNC_DIGIT_LENGTH].u8ptr;
            AssertFatal(X2AP_REGISTER_ENB_REQ(msg_p).mnc_digit_length == 3
                        || X2AP_REGISTER_ENB_REQ(msg_p).mnc < 100,
                        "MNC %d cannot be encoded in two digits as requested (change mnc_digit_length to 3)\n",
                        X2AP_REGISTER_ENB_REQ(msg_p).mnc);

            sprintf(aprefix, "%s.[%i]", GNB_CONFIG_STRING_GNB_LIST, 0);

            config_getlist(config_get_if(), &SCCsParamList, NULL, 0, aprefix);
            if (SCCsParamList.numelt > 0) {
              sprintf(aprefix, "%s.[%i].%s.[%i]", GNB_CONFIG_STRING_GNB_LIST,0,GNB_CONFIG_STRING_SERVINGCELLCONFIGCOMMON, 0);
              config_get(config_get_if(), SCCsParams, sizeofArray(SCCsParams), aprefix);
              fix_scc(scc,ssb_bitmap);
            }
            X2AP_REGISTER_ENB_REQ (msg_p).num_cc = SCCsParamList.numelt;
            for (J = 0; J < SCCsParamList.numelt ; J++) {
              struct NR_FrequencyInfoDL *frequencyInfoDL = scc->downlinkConfigCommon->frequencyInfoDL;
              X2AP_REGISTER_ENB_REQ(msg_p).nr_band[J] = *frequencyInfoDL->frequencyBandList.list.array[0]; // nr_band; //78
              X2AP_REGISTER_ENB_REQ(msg_p).nrARFCN[J] = frequencyInfoDL->absoluteFrequencyPointA;
              X2AP_REGISTER_ENB_REQ (msg_p).uplink_frequency_offset[J] = scc->uplinkConfigCommon->frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->offsetToCarrier; //0
              X2AP_REGISTER_ENB_REQ (msg_p).Nid_cell[J]= *scc->physCellId; //0
              X2AP_REGISTER_ENB_REQ(msg_p).N_RB_DL[J] =
                  frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth; // 106
              X2AP_REGISTER_ENB_REQ (msg_p).frame_type[J] = TDD;
              LOG_I(X2AP, "gNB configuration parameters: nr_band: %d, nr_ARFCN: %d, DL_RBs: %d, num_cc: %d \n",
                  X2AP_REGISTER_ENB_REQ (msg_p).nr_band[J],
                  X2AP_REGISTER_ENB_REQ (msg_p).nrARFCN[J],
                  X2AP_REGISTER_ENB_REQ (msg_p).N_RB_DL[J],
                  X2AP_REGISTER_ENB_REQ (msg_p).num_cc);
            }

            sprintf(aprefix,"%s.[%i]",GNB_CONFIG_STRING_GNB_LIST,k);
            config_getlist(config_get_if(), &X2ParamList, X2Params, sizeofArray(X2Params), aprefix);
            AssertFatal(X2ParamList.numelt <= X2AP_MAX_NB_ENB_IP_ADDRESS,
                        "value of X2ParamList.numelt %d must be lower than X2AP_MAX_NB_ENB_IP_ADDRESS %d value: reconsider to increase X2AP_MAX_NB_ENB_IP_ADDRESS\n",
                        X2ParamList.numelt,X2AP_MAX_NB_ENB_IP_ADDRESS);
            X2AP_REGISTER_ENB_REQ (msg_p).nb_x2 = 0;

            for (l = 0; l < X2ParamList.numelt; l++) {
              X2AP_REGISTER_ENB_REQ (msg_p).nb_x2 += 1;
              strcpy(X2AP_REGISTER_ENB_REQ (msg_p).target_enb_x2_ip_address[l].ipv4_address,*(X2ParamList.paramarray[l][ENB_X2_IPV4_ADDRESS_IDX].strptr));
              X2AP_REGISTER_ENB_REQ (msg_p).target_enb_x2_ip_address[l].ipv4 = 1;
              X2AP_REGISTER_ENB_REQ (msg_p).target_enb_x2_ip_address[l].ipv6 = 0;
            }

            // timers
            {
              int t_reloc_prep = 0;
              int tx2_reloc_overall = 0;
              int t_dc_prep = 0;
              int t_dc_overall = 0;
              paramdef_t p[] = {
                { "t_reloc_prep", "t_reloc_prep", 0, .iptr=&t_reloc_prep, .defintval=0, TYPE_INT, 0 },
                { "tx2_reloc_overall", "tx2_reloc_overall", 0, .iptr=&tx2_reloc_overall, .defintval=0, TYPE_INT, 0 },
                { "t_dc_prep", "t_dc_prep", 0, .iptr=&t_dc_prep, .defintval=0, TYPE_INT, 0 },
                { "t_dc_overall", "t_dc_overall", 0, .iptr=&t_dc_overall, .defintval=0, TYPE_INT, 0 }
              };
              config_get(config_get_if(), p, sizeofArray(p), aprefix);

              if (t_reloc_prep <= 0 || t_reloc_prep > 10000 ||
                  tx2_reloc_overall <= 0 || tx2_reloc_overall > 20000 ||
                  t_dc_prep <= 0 || t_dc_prep > 10000 ||
                  t_dc_overall <= 0 || t_dc_overall > 20000) {
                LOG_E(X2AP, "timers in configuration file have wrong values. We must have [0 < t_reloc_prep <= 10000] and [0 < tx2_reloc_overall <= 20000] and [0 < t_dc_prep <= 10000] and [0 < t_dc_overall <= 20000]\n");
                exit(1);
              }

              X2AP_REGISTER_ENB_REQ (msg_p).t_reloc_prep = t_reloc_prep;
              X2AP_REGISTER_ENB_REQ (msg_p).tx2_reloc_overall = tx2_reloc_overall;
              X2AP_REGISTER_ENB_REQ (msg_p).t_dc_prep = t_dc_prep;
              X2AP_REGISTER_ENB_REQ (msg_p).t_dc_overall = t_dc_overall;
            }
            // SCTP SETTING
            X2AP_REGISTER_ENB_REQ (msg_p).sctp_out_streams = SCTP_OUT_STREAMS;
            X2AP_REGISTER_ENB_REQ (msg_p).sctp_in_streams  = SCTP_IN_STREAMS;

            if (get_softmodem_params()->sa) {
              sprintf(aprefix,"%s.[%i].%s",GNB_CONFIG_STRING_GNB_LIST,k,GNB_CONFIG_STRING_SCTP_CONFIG);
              config_get(config_get_if(), SCTPParams, sizeofArray(SCTPParams), aprefix);
              X2AP_REGISTER_ENB_REQ (msg_p).sctp_in_streams = (uint16_t)*(SCTPParams[GNB_SCTP_INSTREAMS_IDX].uptr);
              X2AP_REGISTER_ENB_REQ (msg_p).sctp_out_streams = (uint16_t)*(SCTPParams[GNB_SCTP_OUTSTREAMS_IDX].uptr);
            }

            sprintf(aprefix,"%s.[%i].%s",GNB_CONFIG_STRING_GNB_LIST,k,GNB_CONFIG_STRING_NETWORK_INTERFACES_CONFIG);
            // NETWORK_INTERFACES
            config_get(config_get_if(), NETParams, sizeofArray(NETParams), aprefix);
            X2AP_REGISTER_ENB_REQ (msg_p).enb_port_for_X2C = (uint32_t)*(NETParams[GNB_PORT_FOR_X2C_IDX].uptr);

            //temp out
            if ((NETParams[GNB_IPV4_ADDR_FOR_X2C_IDX].strptr == NULL) || (X2AP_REGISTER_ENB_REQ (msg_p).enb_port_for_X2C == 0)) {
              LOG_E(RRC,"Add eNB IPv4 address and/or port for X2C in the CONF file!\n");
              exit(1);
            }

            cidr = *(NETParams[ENB_IPV4_ADDR_FOR_X2C_IDX].strptr);
            char *save = NULL;
            address = strtok_r(cidr, "/", &save);
            X2AP_REGISTER_ENB_REQ (msg_p).enb_x2_ip_address.ipv6 = 0;
            X2AP_REGISTER_ENB_REQ (msg_p).enb_x2_ip_address.ipv4 = 1;
            strcpy(X2AP_REGISTER_ENB_REQ (msg_p).enb_x2_ip_address.ipv4_address, address);
          }
        }
      }
    }
  }

  return 0;
}

void wait_f1_setup_response(void)
{
  gNB_MAC_INST *mac = RC.nrmac[0];
  NR_SCHED_LOCK(&mac->sched_lock);
  if (mac->f1_config.setup_resp != NULL) {
    NR_SCHED_UNLOCK(&mac->sched_lock);
    return;
  }

  LOG_W(GNB_APP, "waiting for F1 Setup Response before activating radio\n");

  /* for the moment, we keep it simple and just sleep to periodically check.
   * The actual check is protected by a mutex */
  while (mac->f1_config.setup_resp == NULL) {
    NR_SCHED_UNLOCK(&mac->sched_lock);
    sleep(1);
    NR_SCHED_LOCK(&mac->sched_lock);
  }
  NR_SCHED_UNLOCK(&mac->sched_lock);
}
static bool check_plmn_identity(const f1ap_plmn_t *check_plmn, const f1ap_plmn_t *plmn)
{
  return plmn->mcc == check_plmn->mcc && plmn->mnc_digit_length == check_plmn->mnc_digit_length && plmn->mnc == check_plmn->mnc;
}

int gNB_app_handle_f1ap_gnb_cu_configuration_update(f1ap_gnb_cu_configuration_update_t *gnb_cu_cfg_update) {
  int i, j, ret=0;
  LOG_I(GNB_APP, "cells_to_activate %d, RRC instances %d\n",
        gnb_cu_cfg_update->num_cells_to_activate, RC.nb_nr_inst);

  AssertFatal(gnb_cu_cfg_update->num_cells_to_activate == 1, "only one cell supported at the moment\n");
  AssertFatal(RC.nb_nr_inst == 1, "expected one instance\n");
  gNB_MAC_INST *mac = RC.nrmac[0];
  NR_SCHED_LOCK(&mac->sched_lock);
  for (j = 0; j < gnb_cu_cfg_update->num_cells_to_activate; j++) {
    for (i = 0; i < RC.nb_nr_inst; i++) {
      f1ap_setup_req_t *setup_req = RC.nrmac[i]->f1_config.setup_req;
      // identify local index of cell j by nr_cellid, plmn identity and physical cell ID

      if (setup_req->cell[0].info.nr_cellid == gnb_cu_cfg_update->cells_to_activate[j].nr_cellid
          && check_plmn_identity(&setup_req->cell[0].info.plmn, &gnb_cu_cfg_update->cells_to_activate[j].plmn) > 0
          && setup_req->cell[0].info.nr_pci == gnb_cu_cfg_update->cells_to_activate[j].nrpci) {
        // copy system information and decode it
        AssertFatal(gnb_cu_cfg_update->cells_to_activate[j].num_SI == 0,
                    "gNB-CU Configuration Update: handling of additional SIs not implemend\n");
        ret++;
        mac->f1_config.setup_resp = malloc(sizeof(*mac->f1_config.setup_resp));
        AssertFatal(mac->f1_config.setup_resp != NULL, "out of memory\n");
        mac->f1_config.setup_resp->num_cells_to_activate = gnb_cu_cfg_update->num_cells_to_activate;
        mac->f1_config.setup_resp->cells_to_activate[0] = gnb_cu_cfg_update->cells_to_activate[0];
      } else {
        LOG_E(GNB_APP, "GNB_CU_CONFIGURATION_UPDATE not matching\n");
      }
    }
  }
  NR_SCHED_UNLOCK(&mac->sched_lock);
  /* Free F1AP struct after use */
  free_f1ap_cu_configuration_update(gnb_cu_cfg_update);
  MessageDef *msg_ack_p = NULL;
  if (ret > 0) {
    // generate gNB_CU_CONFIGURATION_UPDATE_ACKNOWLEDGE
    msg_ack_p = itti_alloc_new_message (TASK_GNB_APP, 0, F1AP_GNB_CU_CONFIGURATION_UPDATE_ACKNOWLEDGE);
    F1AP_GNB_CU_CONFIGURATION_UPDATE_ACKNOWLEDGE(msg_ack_p).num_cells_failed_to_be_activated = 0;
    F1AP_GNB_CU_CONFIGURATION_UPDATE_ACKNOWLEDGE(msg_ack_p).have_criticality = 0; 
    F1AP_GNB_CU_CONFIGURATION_UPDATE_ACKNOWLEDGE(msg_ack_p).noofTNLAssociations_to_setup =0;
    F1AP_GNB_CU_CONFIGURATION_UPDATE_ACKNOWLEDGE(msg_ack_p).noofTNLAssociations_failed = 0;
    F1AP_GNB_CU_CONFIGURATION_UPDATE_ACKNOWLEDGE(msg_ack_p).noofDedicatedSIDeliveryNeededUEs = 0;
    F1AP_GNB_CU_CONFIGURATION_UPDATE_ACKNOWLEDGE(msg_ack_p).transaction_id = F1AP_get_next_transaction_identifier(0, 0);
    itti_send_msg_to_task (TASK_DU_F1, INSTANCE_DEFAULT, msg_ack_p);

  }
  else {
    // generate gNB_CU_CONFIGURATION_UPDATE_FAILURE
    msg_ack_p = itti_alloc_new_message (TASK_GNB_APP, 0, F1AP_GNB_CU_CONFIGURATION_UPDATE_FAILURE);
    F1AP_GNB_CU_CONFIGURATION_UPDATE_FAILURE(msg_ack_p).cause = F1AP_CauseRadioNetwork_cell_not_available;

    itti_send_msg_to_task (TASK_DU_F1, INSTANCE_DEFAULT, msg_ack_p);

  }

  return(ret);
}

ngran_node_t get_node_type(void)
{
  paramdef_t        MacRLC_Params[] = MACRLCPARAMS_DESC;
  paramlist_def_t   MacRLC_ParamList = {CONFIG_STRING_MACRLC_LIST,NULL,0};
  paramdef_t        GNBParams[]  = GNBPARAMS_DESC;
  paramlist_def_t   GNBParamList = {GNB_CONFIG_STRING_GNB_LIST,NULL,0};
  paramdef_t        GNBE1Params[] = GNBE1PARAMS_DESC;
  paramlist_def_t   GNBE1ParamList = {GNB_CONFIG_STRING_E1_PARAMETERS, NULL, 0};

  config_getlist(config_get_if(), &GNBParamList, GNBParams, sizeofArray(GNBParams), NULL);

  if (GNBParamList.numelt == 0) // We have no valid configuration, let's return a default 
    return ngran_gNB;

  config_getlist(config_get_if(), &MacRLC_ParamList, MacRLC_Params, sizeofArray(MacRLC_Params), NULL);
  char aprefix[MAX_OPTNAME_SIZE*2 + 8];
  sprintf(aprefix, "%s.[%i]", GNB_CONFIG_STRING_GNB_LIST, 0);
  config_getlist(config_get_if(), &GNBE1ParamList, GNBE1Params, sizeofArray(GNBE1Params), aprefix);
  if (MacRLC_ParamList.numelt > 0) {
    RC.nb_nr_macrlc_inst = MacRLC_ParamList.numelt; 
    for (int j = 0; j < RC.nb_nr_macrlc_inst; j++) {
      if (strcmp(*(MacRLC_ParamList.paramarray[j][MACRLC_TRANSPORT_N_PREFERENCE_IDX].strptr), "f1") == 0) {
        return ngran_gNB_DU; // MACRLCs present in config: it must be a DU
      }
    }
  }

  if (strcmp(*(GNBParamList.paramarray[0][GNB_TRANSPORT_S_PREFERENCE_IDX].strptr), "f1") == 0) {
    if ( GNBE1ParamList.paramarray == NULL || GNBE1ParamList.numelt == 0 )
      return ngran_gNB_CU;
    else if (strcmp(*(GNBE1ParamList.paramarray[0][GNB_CONFIG_E1_CU_TYPE_IDX].strptr), "cp") == 0)
      return ngran_gNB_CUCP;
    else if (strcmp(*(GNBE1ParamList.paramarray[0][GNB_CONFIG_E1_CU_TYPE_IDX].strptr), "up") == 0)
      return ngran_gNB_CUUP;
    else
      return ngran_gNB_CU;
  } else {
    return ngran_gNB;
  }
}
