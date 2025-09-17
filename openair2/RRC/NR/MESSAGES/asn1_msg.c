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

/*! \file asn1_msg.c
* \brief primitives to build the asn1 messages
* \author Raymond Knopp and Navid Nikaein, WEI-TAI CHEN
* \date 2011, 2018
* \version 1.0
* \company Eurecom, NTUST
* \email: {raymond.knopp, navid.nikaein}@eurecom.fr and kroempa@gmail.com
*/

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h> /* for atoi(3) */
#include <unistd.h> /* for getopt(3) */
#include <string.h> /* for strerror(3) */
#include <sysexits.h> /* for EX_* exit codes */
#include <errno.h>  /* for errno */
#include "common/utils/LOG/log.h"
#include "oai_asn1.h"
#include <asn_application.h>
#include <per_encoder.h>
#include <nr/nr_common.h>
#include <softmodem-common.h>

#include "executables/softmodem-common.h"
#include "LAYER2/nr_rlc/nr_rlc_oai_api.h"
#include "asn1_msg.h"
#include "../nr_rrc_proto.h"
#include "LAYER2/nr_pdcp/nr_pdcp_asn1_utils.h"

#include "openair3/SECU/key_nas_deriver.h"

#include "NR_DL-CCCH-Message.h"
#include "NR_UL-CCCH-Message.h"
#include "NR_DL-DCCH-Message.h"
#include "NR_RRCReject.h"
#include "NR_RejectWaitTime.h"
#include "NR_RRCSetup.h"
#include "NR_RRCSetup-IEs.h"
#include "NR_SRB-ToAddModList.h"
#include "NR_CellGroupConfig.h"
#include "NR_RLC-BearerConfig.h"
#include "NR_RLC-Config.h"
#include "NR_LogicalChannelConfig.h"
#include "NR_PDCP-Config.h"
#include "NR_MAC-CellGroupConfig.h"
#include "NR_SecurityModeCommand.h"
#include "NR_CipheringAlgorithm.h"
#include "NR_RRCReconfiguration-IEs.h"
#include "NR_DRB-ToAddMod.h"
#include "NR_DRB-ToAddModList.h"
#include "NR_SecurityConfig.h"
#include "NR_RRCReconfiguration-v1530-IEs.h"
#include "NR_UL-DCCH-Message.h"
#include "NR_SDAP-Config.h"
#include "NR_RRCReconfigurationComplete.h"
#include "NR_RRCReconfigurationComplete-IEs.h"
#include "NR_DLInformationTransfer.h"
#include "NR_RRCReestablishmentRequest.h"
#include "NR_PCCH-Message.h"
#include "NR_PagingRecord.h"
#include "NR_UE-CapabilityRequestFilterNR.h"
#include "NR_HandoverPreparationInformation.h"
#include "NR_HandoverPreparationInformation-IEs.h"
#include "common/utils/nr/nr_common.h"
#if defined(NR_Rel16)
  #include "NR_SCS-SpecificCarrier.h"
  #include "NR_TDD-UL-DL-ConfigCommon.h"
  #include "NR_FrequencyInfoUL.h"
  #include "NR_FrequencyInfoDL.h"
  #include "NR_RACH-ConfigGeneric.h"
  #include "NR_RACH-ConfigCommon.h"
  #include "NR_PUSCH-TimeDomainResourceAllocation.h"
  #include "NR_PUSCH-ConfigCommon.h"
  #include "NR_PUCCH-ConfigCommon.h"
  #include "NR_PDSCH-TimeDomainResourceAllocation.h"
  #include "NR_PDSCH-ConfigCommon.h"
  #include "NR_RateMatchPattern.h"
  #include "NR_RateMatchPatternLTE-CRS.h"
  #include "NR_SearchSpace.h"
  #include "NR_ControlResourceSet.h"
  #include "NR_EUTRA-MBSFN-SubframeConfig.h"
  #include "NR_BWP-DownlinkCommon.h"
  #include "NR_BWP-DownlinkDedicated.h"
  #include "NR_UplinkConfigCommon.h"
  #include "NR_SetupRelease.h"
  #include "NR_PDCCH-ConfigCommon.h"
  #include "NR_BWP-UplinkCommon.h"

  #include "assertions.h"
  //#include "RRCConnectionRequest.h"
  //#include "UL-CCCH-Message.h"
  #include "NR_UL-DCCH-Message.h"
  //#include "DL-CCCH-Message.h"
  #include "NR_DL-DCCH-Message.h"
  //#include "EstablishmentCause.h"
  //#include "RRCConnectionSetup.h"
  #include "NR_SRB-ToAddModList.h"
  #include "NR_DRB-ToAddModList.h"
  //#include "MCCH-Message.h"
  //#define MRB1 1

  //#include "RRCConnectionSetupComplete.h"
  //#include "RRCConnectionReconfigurationComplete.h"
  //#include "RRCConnectionReconfiguration.h"
  #include "NR_MIB.h"
  //#include "SystemInformation.h"

  #include "NR_SIB1.h"
  #include "NR_ServingCellConfigCommon.h"
  //#include "SIB-Type.h"

  //#include "BCCH-DL-SCH-Message.h"

  //#include "PHY/defs.h"

  #include "NR_MeasObjectToAddModList.h"
  #include "NR_ReportConfigToAddModList.h"
  #include "NR_MeasIdToAddModList.h"
  #include "gnb_config.h"
#endif

#include "intertask_interface.h"

#include "common/ran_context.h"
#include "conversions.h"
#include "ds/byte_array.h"

//#define XER_PRINT

typedef struct xer_sprint_string_s {
  char *string;
  size_t string_size;
  size_t string_index;
} xer_sprint_string_t;

/*
 * This is a helper function for xer_sprint, which directs all incoming data
 * into the provided string.
 */
static int xer__nr_print2s(const void *buffer, size_t size, void *app_key)
{
  xer_sprint_string_t *string_buffer = (xer_sprint_string_t *) app_key;
  size_t string_remaining = string_buffer->string_size - string_buffer->string_index;

  if (string_remaining > 0) {
    if (size > string_remaining) {
      size = string_remaining;
    }

    memcpy(&string_buffer->string[string_buffer->string_index], buffer, size);
    string_buffer->string_index += size;
  }

  return 0;
}

int xer_nr_sprint(char *string, size_t string_size, asn_TYPE_descriptor_t *td, void *sptr)
{
  asn_enc_rval_t er;
  xer_sprint_string_t string_buffer;
  string_buffer.string = string;
  string_buffer.string_size = string_size;
  string_buffer.string_index = 0;
  er = xer_encode(td, sptr, XER_F_BASIC, xer__nr_print2s, &string_buffer);

  if (er.encoded < 0) {
    LOG_E(RRC, "xer_sprint encoding error (%zd)!", er.encoded);
    er.encoded = string_buffer.string_size;
  } else {
    if (er.encoded > string_buffer.string_size) {
      LOG_E(RRC, "xer_sprint string buffer too small, got %zd need %zd!", string_buffer.string_size, er.encoded);
      er.encoded = string_buffer.string_size;
    }
  }

  return er.encoded;
}

//------------------------------------------------------------------------------

int do_SIB2_NR(uint8_t **msg_SIB2, NR_SSB_MTC_t *ssbmtc)
{
  NR_SIB2_t *sib2 = calloc(1, sizeof(*sib2));
  sib2->cellReselectionInfoCommon.q_Hyst = NR_SIB2__cellReselectionInfoCommon__q_Hyst_dB0;
  struct NR_SIB2__cellReselectionInfoCommon__speedStateReselectionPars *speed = calloc(1, sizeof(*speed));
  NR_MobilityStateParameters_t mobilityStateParameters = {0};
  mobilityStateParameters.t_Evaluation = NR_MobilityStateParameters__t_Evaluation_s30;
  mobilityStateParameters.t_HystNormal = NR_MobilityStateParameters__t_HystNormal_s30;
  mobilityStateParameters.n_CellChangeMedium = 1;
  mobilityStateParameters.n_CellChangeHigh = 2;
  speed->mobilityStateParameters = mobilityStateParameters;
  struct NR_SIB2__cellReselectionInfoCommon__speedStateReselectionPars__q_HystSF qhyst = {0};
  qhyst.sf_Medium = NR_SIB2__cellReselectionInfoCommon__speedStateReselectionPars__q_HystSF__sf_Medium_dB_4;
  qhyst.sf_High = NR_SIB2__cellReselectionInfoCommon__speedStateReselectionPars__q_HystSF__sf_High_dB_6;
  speed->q_HystSF = qhyst;
  sib2->cellReselectionInfoCommon.speedStateReselectionPars = speed;
  sib2->cellReselectionServingFreqInfo.cellReselectionPriority = 0; // INTEGER (0..7)
  sib2->cellReselectionServingFreqInfo.threshServingLowP = 0;
  NR_ReselectionThresholdQ_t *threshServingLowQ = calloc(1, sizeof(*threshServingLowQ));
  *threshServingLowQ = 0;
  sib2->cellReselectionServingFreqInfo.threshServingLowQ = threshServingLowQ;
  sib2->intraFreqCellReselectionInfo.q_RxLevMin = -56; // INTEGER (-70..-22)
  sib2->intraFreqCellReselectionInfo.s_IntraSearchP = 22; // INTEGER (0..31)
  sib2->intraFreqCellReselectionInfo.t_ReselectionNR = 1; // INTEGER (0..7)
  sib2->intraFreqCellReselectionInfo.deriveSSB_IndexFromCell = true;
  sib2->intraFreqCellReselectionInfo.smtc = ssbmtc;

  ssize_t size = uper_encode_to_new_buffer(&asn_DEF_NR_SIB2, NULL, (void *)sib2, (void **)msg_SIB2);
  AssertFatal (size > 0, "ASN1 message encoding failed (encoded %lu bytes)!\n", size);
  ASN_STRUCT_FREE(asn_DEF_NR_SIB2, sib2);
  return size;
}

int do_RRCReject(uint8_t *const buffer)
{
    asn_enc_rval_t                                   enc_rval;
    NR_DL_CCCH_Message_t                             dl_ccch_msg;
    NR_RRCReject_t                                   *rrcReject;

    memset((void *)&dl_ccch_msg, 0, sizeof(NR_DL_CCCH_Message_t));
    dl_ccch_msg.message.present = NR_DL_CCCH_MessageType_PR_c1;
    dl_ccch_msg.message.choice.c1          = CALLOC(1, sizeof(struct NR_DL_CCCH_MessageType__c1));
    dl_ccch_msg.message.choice.c1->present = NR_DL_CCCH_MessageType__c1_PR_rrcReject;

    dl_ccch_msg.message.choice.c1->choice.rrcReject = CALLOC(1,sizeof(NR_RRCReject_t));
    rrcReject = dl_ccch_msg.message.choice.c1->choice.rrcReject;

    rrcReject->criticalExtensions.choice.rrcReject           = CALLOC(1, sizeof(struct NR_RRCReject_IEs));
    rrcReject->criticalExtensions.choice.rrcReject->waitTime = CALLOC(1, sizeof(NR_RejectWaitTime_t));
    *rrcReject->criticalExtensions.choice.rrcReject->waitTime = 10;

    rrcReject->criticalExtensions.present = NR_RRCReject__criticalExtensions_PR_rrcReject;

    if ( LOG_DEBUGFLAG(DEBUG_ASN1) ) {
        xer_fprint(stdout, &asn_DEF_NR_DL_CCCH_Message, (void *)&dl_ccch_msg);
    }

    enc_rval = uper_encode_to_buffer(&asn_DEF_NR_DL_CCCH_Message,
                                    NULL,
                                    (void *)&dl_ccch_msg,
                                    buffer,
                                    100);

    AssertFatal(enc_rval.encoded > 0, "ASN1 message encoding failed (%s, %lu)!\n",
                enc_rval.failed_type->name, enc_rval.encoded);
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NR_DL_CCCH_Message, &dl_ccch_msg);

    LOG_D(NR_RRC,"RRCReject Encoded %zd bits (%zd bytes)\n",
            enc_rval.encoded,(enc_rval.encoded+7)/8);
    return (enc_rval.encoded + 7) / 8;
}

/* returns a default radio bearer config suitable for NSA etc */
NR_RadioBearerConfig_t *get_default_rbconfig(int eps_bearer_id,
                                             int rb_id,
                                             e_NR_CipheringAlgorithm ciphering_algorithm,
                                             e_NR_SecurityConfig__keyToUse key_to_use,
                                             const nr_pdcp_configuration_t *pdcp_config)
{
  NR_RadioBearerConfig_t *rbconfig = calloc(1, sizeof(*rbconfig));
  rbconfig->srb_ToAddModList = NULL;
  rbconfig->srb3_ToRelease = NULL;
  rbconfig->drb_ToAddModList = calloc(1,sizeof(*rbconfig->drb_ToAddModList));
  NR_DRB_ToAddMod_t *drb_ToAddMod = calloc(1,sizeof(*drb_ToAddMod));
  drb_ToAddMod->cnAssociation = calloc(1,sizeof(*drb_ToAddMod->cnAssociation));
  drb_ToAddMod->cnAssociation->present = NR_DRB_ToAddMod__cnAssociation_PR_eps_BearerIdentity;
  drb_ToAddMod->cnAssociation->choice.eps_BearerIdentity= eps_bearer_id;
  drb_ToAddMod->drb_Identity = rb_id;
  drb_ToAddMod->reestablishPDCP = NULL;
  drb_ToAddMod->recoverPDCP = NULL;
  drb_ToAddMod->pdcp_Config = calloc(1,sizeof(*drb_ToAddMod->pdcp_Config));
  asn1cCalloc(drb_ToAddMod->pdcp_Config->drb, drb);
  asn1cCallocOne(drb->discardTimer, encode_discard_timer(pdcp_config->drb.discard_timer));
  asn1cCallocOne(drb->pdcp_SN_SizeUL, encode_sn_size_ul(pdcp_config->drb.sn_size));
  asn1cCallocOne(drb->pdcp_SN_SizeDL, encode_sn_size_dl(pdcp_config->drb.sn_size));
  drb->headerCompression.present = NR_PDCP_Config__drb__headerCompression_PR_notUsed;
  drb->headerCompression.choice.notUsed = 0;
  drb->integrityProtection = NULL;
  drb->statusReportRequired = NULL;
  drb->outOfOrderDelivery = NULL;

  drb_ToAddMod->pdcp_Config->moreThanOneRLC = NULL;
  asn1cCallocOne(drb_ToAddMod->pdcp_Config->t_Reordering, encode_t_reordering(pdcp_config->drb.t_reordering));
  drb_ToAddMod->pdcp_Config->ext1 = NULL;

  asn1cSeqAdd(&rbconfig->drb_ToAddModList->list,drb_ToAddMod);

  rbconfig->drb_ToReleaseList = NULL;

  asn1cCalloc(rbconfig->securityConfig, secConf);
  asn1cCalloc(secConf->securityAlgorithmConfig, secConfAlgo);
  secConfAlgo->cipheringAlgorithm = ciphering_algorithm;
  secConfAlgo->integrityProtAlgorithm = NULL;
  asn1cCallocOne(secConf->keyToUse, key_to_use);
  return rbconfig;
}

//------------------------------------------------------------------------------
int do_RRCSetup(uint8_t *const buffer,
                size_t buffer_size,
                const uint8_t transaction_id,
                const uint8_t *masterCellGroup,
                int masterCellGroup_len,
                const gNB_RrcConfigurationReq *configuration,
                NR_SRB_ToAddModList_t *SRBs)
//------------------------------------------------------------------------------
{
  NR_DL_CCCH_Message_t dl_ccch_msg = {0};
  dl_ccch_msg.message.present = NR_DL_CCCH_MessageType_PR_c1;
  asn1cCalloc(dl_ccch_msg.message.choice.c1, dl_msg);
  dl_msg->present = NR_DL_CCCH_MessageType__c1_PR_rrcSetup;
  asn1cCalloc(dl_msg->choice.rrcSetup, rrcSetup);
  rrcSetup->criticalExtensions.present = NR_RRCSetup__criticalExtensions_PR_rrcSetup;
  rrcSetup->rrc_TransactionIdentifier = transaction_id;
  rrcSetup->criticalExtensions.choice.rrcSetup = calloc(1, sizeof(NR_RRCSetup_IEs_t));
  NR_RRCSetup_IEs_t *ie = rrcSetup->criticalExtensions.choice.rrcSetup;

  /****************************** radioBearerConfig ******************************/
  ie->radioBearerConfig.srb_ToAddModList = SRBs;
  ie->radioBearerConfig.srb3_ToRelease = NULL;
  ie->radioBearerConfig.drb_ToAddModList = NULL;
  ie->radioBearerConfig.drb_ToReleaseList = NULL;
  ie->radioBearerConfig.securityConfig = NULL;

  /****************************** masterCellGroup ******************************/
  DevAssert(masterCellGroup && masterCellGroup_len > 0);
  ie->masterCellGroup.buf = malloc(masterCellGroup_len);
  AssertFatal(ie->masterCellGroup.buf != NULL, "could not allocate memory for masterCellGroup\n");
  memcpy(ie->masterCellGroup.buf, masterCellGroup, masterCellGroup_len);
  ie->masterCellGroup.size = masterCellGroup_len;

  if (LOG_DEBUGFLAG(DEBUG_ASN1))
    xer_fprint(stdout, &asn_DEF_NR_DL_CCCH_Message, (void *)&dl_ccch_msg);

  asn_enc_rval_t enc_rval = uper_encode_to_buffer(&asn_DEF_NR_DL_CCCH_Message, NULL, (void *)&dl_ccch_msg, buffer, buffer_size);

  AssertFatal(enc_rval.encoded > 0, "ASN1 message encoding failed (%s, %lu)!\n", enc_rval.failed_type->name, enc_rval.encoded);
  // free what we did not allocate ourselves
  ie->radioBearerConfig.srb_ToAddModList = NULL;
  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NR_DL_CCCH_Message, &dl_ccch_msg);

  LOG_D(NR_RRC, "RRCSetup Encoded %zd bits (%zd bytes)\n", enc_rval.encoded, (enc_rval.encoded + 7) / 8);
  return ((enc_rval.encoded + 7) / 8);
}

int do_NR_SecurityModeCommand(uint8_t *const buffer,
                              const uint8_t Transaction_id,
                              const uint8_t cipheringAlgorithm,
                              NR_IntegrityProtAlgorithm_t integrityProtAlgorithm)
//------------------------------------------------------------------------------
{
  NR_DL_DCCH_Message_t dl_dcch_msg={0};
  asn_enc_rval_t enc_rval;
  dl_dcch_msg.message.present           = NR_DL_DCCH_MessageType_PR_c1;
  asn1cCalloc(dl_dcch_msg.message.choice.c1, c1);
  c1->present = NR_DL_DCCH_MessageType__c1_PR_securityModeCommand;
  asn1cCalloc(c1->choice.securityModeCommand,scm);
  scm->rrc_TransactionIdentifier = Transaction_id;
  scm->criticalExtensions.present = NR_SecurityModeCommand__criticalExtensions_PR_securityModeCommand;

  asn1cCalloc(scm->criticalExtensions.choice.securityModeCommand,scmIE);
  // the two following information could be based on the mod_id
  scmIE->securityConfigSMC.securityAlgorithmConfig.cipheringAlgorithm
    = (NR_CipheringAlgorithm_t)cipheringAlgorithm;
  asn1cCallocOne(scmIE->securityConfigSMC.securityAlgorithmConfig.integrityProtAlgorithm, integrityProtAlgorithm);

  if ( LOG_DEBUGFLAG(DEBUG_ASN1) ) {
    xer_fprint(stdout, &asn_DEF_NR_DL_DCCH_Message, (void *)&dl_dcch_msg);
  }

  enc_rval = uper_encode_to_buffer(&asn_DEF_NR_DL_DCCH_Message,
                                   NULL,
                                   (void *)&dl_dcch_msg,
                                   buffer,
                                   100);

  AssertFatal(enc_rval.encoded >0 , "ASN1 message encoding failed (%s, %lu)!\n",
              enc_rval.failed_type->name, enc_rval.encoded);
  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NR_DL_DCCH_Message,&dl_dcch_msg);

  //  rrc_ue_process_ueCapabilityEnquiry(0,1000,&dl_dcch_msg.message.choice.c1.choice.ueCapabilityEnquiry,0);
  //  exit(-1);
  return((enc_rval.encoded+7)/8);
}

int do_NR_SA_UECapabilityEnquiry(uint8_t *const buffer, const uint8_t Transaction_id)
{
  NR_UE_CapabilityRequestFilterNR_t *sa_band_filter;
  NR_FreqBandList_t *sa_band_list;
  NR_FreqBandInformation_t *sa_band_info;
  NR_FreqBandInformationNR_t *sa_band_infoNR;

  NR_DL_DCCH_Message_t dl_dcch_msg;
  NR_UE_CapabilityRAT_Request_t *ue_capabilityrat_request;

  memset(&dl_dcch_msg,0,sizeof(NR_DL_DCCH_Message_t));
  dl_dcch_msg.message.present           = NR_DL_DCCH_MessageType_PR_c1;
  dl_dcch_msg.message.choice.c1 = CALLOC(1,sizeof(struct NR_DL_DCCH_MessageType__c1));
  dl_dcch_msg.message.choice.c1->present = NR_DL_DCCH_MessageType__c1_PR_ueCapabilityEnquiry;
  dl_dcch_msg.message.choice.c1->choice.ueCapabilityEnquiry = CALLOC(1,sizeof(struct NR_UECapabilityEnquiry));
  dl_dcch_msg.message.choice.c1->choice.ueCapabilityEnquiry->rrc_TransactionIdentifier = Transaction_id;
  dl_dcch_msg.message.choice.c1->choice.ueCapabilityEnquiry->criticalExtensions.present = NR_UECapabilityEnquiry__criticalExtensions_PR_ueCapabilityEnquiry;
  dl_dcch_msg.message.choice.c1->choice.ueCapabilityEnquiry->criticalExtensions.choice.ueCapabilityEnquiry = CALLOC(1,sizeof(struct NR_UECapabilityEnquiry_IEs));
  ue_capabilityrat_request =  CALLOC(1,sizeof(NR_UE_CapabilityRAT_Request_t));
  memset(ue_capabilityrat_request,0,sizeof(NR_UE_CapabilityRAT_Request_t));
  ue_capabilityrat_request->rat_Type = NR_RAT_Type_nr;

  sa_band_infoNR = (NR_FreqBandInformationNR_t*)calloc(1,sizeof(NR_FreqBandInformationNR_t));
  sa_band_infoNR->bandNR = 78;
  sa_band_info = (NR_FreqBandInformation_t*)calloc(1,sizeof(NR_FreqBandInformation_t));
  sa_band_info->present = NR_FreqBandInformation_PR_bandInformationNR;
  sa_band_info->choice.bandInformationNR = sa_band_infoNR;
  
  sa_band_list = (NR_FreqBandList_t *)calloc(1, sizeof(NR_FreqBandList_t));
  asn1cSeqAdd(&sa_band_list->list, sa_band_info);

  sa_band_filter = (NR_UE_CapabilityRequestFilterNR_t*)calloc(1,sizeof(NR_UE_CapabilityRequestFilterNR_t));
  sa_band_filter->frequencyBandListFilter = sa_band_list;

  OCTET_STRING_t *req_freq = calloc(1, sizeof(*req_freq));
  AssertFatal(req_freq != NULL, "out of memory\n");
  req_freq->size = uper_encode_to_new_buffer(&asn_DEF_NR_UE_CapabilityRequestFilterNR, NULL, sa_band_filter, (void **)&req_freq->buf);
  AssertFatal(req_freq->size > 0, "ASN1 message encoding failed (encoded %lu bytes)!\n", req_freq->size);

  if ( LOG_DEBUGFLAG(DEBUG_ASN1) ) {
    xer_fprint(stdout, &asn_DEF_NR_UE_CapabilityRequestFilterNR, (void *)sa_band_filter);
  }
  ASN_STRUCT_FREE(asn_DEF_NR_UE_CapabilityRequestFilterNR, sa_band_filter);

  ue_capabilityrat_request->capabilityRequestFilter = req_freq;

  asn1cSeqAdd(&dl_dcch_msg.message.choice.c1->choice.ueCapabilityEnquiry->criticalExtensions.choice.ueCapabilityEnquiry->ue_CapabilityRAT_RequestList.list,
                   ue_capabilityrat_request);


  if ( LOG_DEBUGFLAG(DEBUG_ASN1) ) {
    xer_fprint(stdout, &asn_DEF_NR_DL_DCCH_Message, (void *)&dl_dcch_msg);
  }

  asn_enc_rval_t enc_rval = uper_encode_to_buffer(&asn_DEF_NR_DL_DCCH_Message, NULL, (void *)&dl_dcch_msg, buffer, 100);

  AssertFatal(enc_rval.encoded >0, "ASN1 message encoding failed (%s, %lu)!\n",
              enc_rval.failed_type->name, enc_rval.encoded);
  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NR_DL_DCCH_Message, &dl_dcch_msg);

  LOG_D(NR_RRC, "NR UECapabilityRequestEncoded %zd bits (%zd bytes)\n", enc_rval.encoded, (enc_rval.encoded + 7) / 8);

  return((enc_rval.encoded+7)/8);
}

int do_NR_RRCRelease(uint8_t *buffer, size_t buffer_size, uint8_t Transaction_id)
{
  asn_enc_rval_t enc_rval;
  NR_DL_DCCH_Message_t dl_dcch_msg;
  NR_RRCRelease_t *rrcConnectionRelease;
  memset(&dl_dcch_msg,0,sizeof(NR_DL_DCCH_Message_t));
  dl_dcch_msg.message.present           = NR_DL_DCCH_MessageType_PR_c1;
  dl_dcch_msg.message.choice.c1=CALLOC(1,sizeof(struct NR_DL_DCCH_MessageType__c1));
  dl_dcch_msg.message.choice.c1->present = NR_DL_DCCH_MessageType__c1_PR_rrcRelease;
  dl_dcch_msg.message.choice.c1->choice.rrcRelease = CALLOC(1, sizeof(NR_RRCRelease_t));
  rrcConnectionRelease = dl_dcch_msg.message.choice.c1->choice.rrcRelease;
  // RRCConnectionRelease
  rrcConnectionRelease->rrc_TransactionIdentifier = Transaction_id;
  rrcConnectionRelease->criticalExtensions.present = NR_RRCRelease__criticalExtensions_PR_rrcRelease;
  rrcConnectionRelease->criticalExtensions.choice.rrcRelease = CALLOC(1, sizeof(NR_RRCRelease_IEs_t));
  rrcConnectionRelease->criticalExtensions.choice.rrcRelease->deprioritisationReq =
      CALLOC(1, sizeof(struct NR_RRCRelease_IEs__deprioritisationReq));
  rrcConnectionRelease->criticalExtensions.choice.rrcRelease->deprioritisationReq->deprioritisationType =
      NR_RRCRelease_IEs__deprioritisationReq__deprioritisationType_nr;
  rrcConnectionRelease->criticalExtensions.choice.rrcRelease->deprioritisationReq->deprioritisationTimer =
      NR_RRCRelease_IEs__deprioritisationReq__deprioritisationTimer_min10;

  enc_rval = uper_encode_to_buffer(&asn_DEF_NR_DL_DCCH_Message,
                                   NULL,
                                   (void *)&dl_dcch_msg,
                                   buffer,
                                   buffer_size);
  AssertFatal(enc_rval.encoded > 0, "ASN1 message encoding failed (%s, %lu)!\n",
              enc_rval.failed_type->name, enc_rval.encoded);
  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NR_DL_DCCH_Message, &dl_dcch_msg);
  return((enc_rval.encoded+7)/8);
}

/** @brief Build RRCReconfiguration message (3GPP TS 38.331) */
static NR_RRCReconfiguration_IEs_t *build_RRCReconfiguration_IEs(const nr_rrc_reconfig_param_t *params)
{
  NR_RRCReconfiguration_IEs_t *ie = calloc_or_fail(1, sizeof(*ie));

  // radioBearerConfig
  if ((params->srb_config_list && params->srb_config_list->list.size)
      || (params->drb_config_list && params->drb_config_list->list.size)) {
    ie->radioBearerConfig = calloc_or_fail(1, sizeof(*ie->radioBearerConfig));
    struct NR_RadioBearerConfig *cfg = ie->radioBearerConfig;
    cfg->srb_ToAddModList = params->srb_config_list;
    cfg->drb_ToAddModList = params->drb_config_list;
    cfg->securityConfig = params->security_config;
    cfg->srb3_ToRelease = NULL;
    cfg->drb_ToReleaseList = params->drb_release_list;
  }

  /* measConfig */
  ie->measConfig = params->meas_config;

  /* nonCriticalExtension, RRCReconfiguration-v1530-IEs */
  if (params->cell_group_config || params->num_nas_msg) {
    // Allocate memory for extension IE
    ie->nonCriticalExtension = calloc_or_fail(1, sizeof(*ie->nonCriticalExtension));
  }

  // Configure Cell Group Config
  if (ie->nonCriticalExtension) {
    if (params->num_nas_msg) {
      asn1cCalloc(ie->nonCriticalExtension->dedicatedNAS_MessageList, list);
      /* dedicatedNAS-MessageList: The field is absent in case of reconfiguration with sync
        otherwise it is optionally present */
      for (int i = 0; i < params->num_nas_msg; i++) {
        asn1cSequenceAdd(list->list, NR_DedicatedNAS_Message_t, msg);
        OCTET_STRING_fromBuf(msg, (char *)params->dedicated_NAS_msg_list[i].buf, params->dedicated_NAS_msg_list[i].len);
      }
    }

    /* masterCellGroup */
    if (params->cell_group_config) {
      // Encode in extension IE (Master cell group)
      uint8_t *buf = NULL;
      ssize_t len = uper_encode_to_new_buffer(&asn_DEF_NR_CellGroupConfig, NULL, params->cell_group_config, (void **)&buf);
      AssertFatal(len > 0, "ASN1 message encoding failed (%lu)!\n", len);
      if (LOG_DEBUGFLAG(DEBUG_ASN1)) {
        xer_fprint(stdout, &asn_DEF_NR_CellGroupConfig, (const void *)params->cell_group_config);
      }
      ie->nonCriticalExtension->masterCellGroup = calloc_or_fail(1, sizeof(*ie->nonCriticalExtension->masterCellGroup));
      *ie->nonCriticalExtension->masterCellGroup = (OCTET_STRING_t){.buf = buf, .size = len};
    }

    /* masterKeyUpdate */
    if (params->masterKeyUpdate) {
      ie->nonCriticalExtension->masterKeyUpdate = calloc_or_fail(1, sizeof(*ie->nonCriticalExtension->masterKeyUpdate));
      ie->nonCriticalExtension->masterKeyUpdate->keySetChangeIndicator = false;
      ie->nonCriticalExtension->masterKeyUpdate->nextHopChainingCount = params->nextHopChainingCount;
    }
  }

  return ie;
}

byte_array_t do_RRCReconfiguration(const nr_rrc_reconfig_param_t *params)
{
  byte_array_t msg = {.buf = NULL, .len = 0};
  NR_RRCReconfiguration_IEs_t *ie = build_RRCReconfiguration_IEs(params);

  NR_DL_DCCH_Message_t dl_dcch_msg = {0};
  dl_dcch_msg.message.present = NR_DL_DCCH_MessageType_PR_c1;

  asn1cCalloc(dl_dcch_msg.message.choice.c1, c1);
  c1->present = NR_DL_DCCH_MessageType__c1_PR_rrcReconfiguration;
  asn1cCalloc(c1->choice.rrcReconfiguration, rrcReconf);
  rrcReconf->rrc_TransactionIdentifier = params->transaction_id;
  rrcReconf->criticalExtensions.present = NR_RRCReconfiguration__criticalExtensions_PR_rrcReconfiguration;
  rrcReconf->criticalExtensions.choice.rrcReconfiguration = ie;

  if (LOG_DEBUGFLAG(DEBUG_ASN1)) {
    xer_fprint(stdout, &asn_DEF_NR_DL_DCCH_Message, (void *)&dl_dcch_msg);
  }

  int val = uper_encode_to_new_buffer(&asn_DEF_NR_DL_DCCH_Message, NULL, &dl_dcch_msg, (void **)&msg.buf);
  if (val <= 0) {
    LOG_E(NR_RRC, "Failed to encode DL-DCCH message\n");
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NR_DL_DCCH_Message, &dl_dcch_msg);
    return msg;
  }
  msg.len = val;
  LOG_D(NR_RRC, "RRCReconfiguration: Encoded (%ld bytes)\n", msg.len);

  // Do not free what is not allocated in this context
  ie->measConfig = NULL;
  if (ie->radioBearerConfig) {
    ie->radioBearerConfig->securityConfig = NULL;
    ie->radioBearerConfig->drb_ToReleaseList = NULL;
    ie->radioBearerConfig->srb_ToAddModList = NULL;
    ie->radioBearerConfig->drb_ToAddModList = NULL;
  }

  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NR_DL_DCCH_Message, &dl_dcch_msg);

  return msg;
}

int do_RRCSetupRequest(uint8_t *buffer, size_t buffer_size, uint8_t *rv, uint64_t fiveG_S_TMSI)
{
  NR_UL_CCCH_Message_t ul_ccch_msg = {0};
  ul_ccch_msg.message.present           = NR_UL_CCCH_MessageType_PR_c1;
  asn1cCalloc(ul_ccch_msg.message.choice.c1, c1);
  c1->present = NR_UL_CCCH_MessageType__c1_PR_rrcSetupRequest;
  asn1cCalloc(c1->choice.rrcSetupRequest, rrcSetupRequest);

  if (fiveG_S_TMSI == UINT64_MAX) {
    /* set the ue-Identity to a random value */
    rrcSetupRequest->rrcSetupRequest.ue_Identity.present = NR_InitialUE_Identity_PR_randomValue;
    BIT_STRING_t *str = &rrcSetupRequest->rrcSetupRequest.ue_Identity.choice.randomValue;
    str->size = 5;
    str->bits_unused = 1;
    str->buf = calloc_or_fail(str->size, sizeof(str->buf[0]));
    str->buf[0] = rv[0];
    str->buf[1] = rv[1];
    str->buf[2] = rv[2];
    str->buf[3] = rv[3];
    str->buf[4] = rv[4] & 0xfe;
  } else {
    uint64_t fiveG_S_TMSI_part1 = fiveG_S_TMSI & ((1ULL << 39) - 1);
    /** set the ue-Identity to ng-5G-S-TMSI-Part1
     * ng-5G-S-TMSI-Part1: the rightmost 39 bits of 5G-S-TMSI
     * BIT STRING (SIZE (39)) - 3GPP TS 38.331 */
    LOG_D(NR_RRC, "5G-S-TMSI: %lu, set the ue-Identity to ng-5G-S-TMSI-Part1 %lu\n", fiveG_S_TMSI, fiveG_S_TMSI_part1);
    rrcSetupRequest->rrcSetupRequest.ue_Identity.present = NR_InitialUE_Identity_PR_ng_5G_S_TMSI_Part1;
    BIT_STRING_t *str = &rrcSetupRequest->rrcSetupRequest.ue_Identity.choice.ng_5G_S_TMSI_Part1;
    str->size = 5;
    str->bits_unused = 1;
    str->buf = calloc_or_fail(str->size, sizeof(str->buf[0]));
    str->buf[0] = (fiveG_S_TMSI_part1 >> 31) & 0xff;
    str->buf[1] = (fiveG_S_TMSI_part1 >> 23) & 0xff;
    str->buf[2] = (fiveG_S_TMSI_part1 >> 15) & 0xff;
    str->buf[3] = (fiveG_S_TMSI_part1 >> 7) & 0xff;
    str->buf[4] = (fiveG_S_TMSI_part1 << 1) & 0xff;
  }

  rrcSetupRequest->rrcSetupRequest.establishmentCause = NR_EstablishmentCause_mo_Signalling; //EstablishmentCause_mo_Data;
  rrcSetupRequest->rrcSetupRequest.spare.buf = CALLOC(1, 1);
  rrcSetupRequest->rrcSetupRequest.spare.buf[0] = 0; // spare not used
  rrcSetupRequest->rrcSetupRequest.spare.size=1;
  rrcSetupRequest->rrcSetupRequest.spare.bits_unused = 7;

  if ( LOG_DEBUGFLAG(DEBUG_ASN1) ) {
    xer_fprint(stdout, &asn_DEF_NR_UL_CCCH_Message, (void *)&ul_ccch_msg);
  }

  asn_enc_rval_t enc_rval = uper_encode_to_buffer(&asn_DEF_NR_UL_CCCH_Message, NULL, (void *)&ul_ccch_msg, buffer, buffer_size);
  AssertFatal (enc_rval.encoded > 0, "ASN1 message encoding failed (%s, %lu)!\n", enc_rval.failed_type->name, enc_rval.encoded);
  LOG_D(NR_RRC,"[UE] RRCSetupRequest Encoded %zd bits (%zd bytes)\n", enc_rval.encoded, (enc_rval.encoded+7)/8);
  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NR_UL_CCCH_Message, &ul_ccch_msg);
  return((enc_rval.encoded+7)/8);
}

//------------------------------------------------------------------------------
int do_NR_RRCReconfigurationComplete_for_nsa(
  uint8_t *buffer,
  size_t buffer_size,
  NR_RRC_TransactionIdentifier_t Transaction_id)
//------------------------------------------------------------------------------
{
  NR_RRCReconfigurationComplete_t rrc_complete_msg;
  memset(&rrc_complete_msg, 0, sizeof(rrc_complete_msg));
  rrc_complete_msg.rrc_TransactionIdentifier = Transaction_id;
  rrc_complete_msg.criticalExtensions.choice.rrcReconfigurationComplete =
        CALLOC(1, sizeof(*rrc_complete_msg.criticalExtensions.choice.rrcReconfigurationComplete));
  rrc_complete_msg.criticalExtensions.present =
	NR_RRCReconfigurationComplete__criticalExtensions_PR_rrcReconfigurationComplete;
  rrc_complete_msg.criticalExtensions.choice.rrcReconfigurationComplete->nonCriticalExtension = NULL;
  rrc_complete_msg.criticalExtensions.choice.rrcReconfigurationComplete->lateNonCriticalExtension = NULL;
  if (0) {
    xer_fprint(stdout, &asn_DEF_NR_RRCReconfigurationComplete, (void *)&rrc_complete_msg);
  }

  asn_enc_rval_t enc_rval = uper_encode_to_buffer(&asn_DEF_NR_RRCReconfigurationComplete,
                                                  NULL,
                                                  (void *)&rrc_complete_msg,
                                                  buffer,
                                                  buffer_size);
  AssertFatal (enc_rval.encoded > 0, "ASN1 message encoding failed (%s, %lu)!\n",
               enc_rval.failed_type->name, enc_rval.encoded);
  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NR_RRCReconfigurationComplete, &rrc_complete_msg);
  LOG_A(NR_RRC, "rrcReconfigurationComplete Encoded %zd bits (%zd bytes)\n", enc_rval.encoded, (enc_rval.encoded+7)/8);
  return((enc_rval.encoded+7)/8);
}

//------------------------------------------------------------------------------
int do_NR_RRCReconfigurationComplete(uint8_t *buffer, size_t buffer_size, const uint8_t Transaction_id)
//------------------------------------------------------------------------------
{
  NR_UL_DCCH_Message_t ul_dcch_msg = {0};
  ul_dcch_msg.message.present                     = NR_UL_DCCH_MessageType_PR_c1;
  asn1cCalloc(ul_dcch_msg.message.choice.c1, c1);
  c1->present = NR_UL_DCCH_MessageType__c1_PR_rrcReconfigurationComplete;
  asn1cCalloc(c1->choice.rrcReconfigurationComplete, reconfComplete);
  reconfComplete->rrc_TransactionIdentifier = Transaction_id;
  reconfComplete->criticalExtensions.present = NR_RRCReconfigurationComplete__criticalExtensions_PR_rrcReconfigurationComplete;
  asn1cCalloc(reconfComplete->criticalExtensions.choice.rrcReconfigurationComplete, extension);
  extension->nonCriticalExtension = NULL;
  extension->lateNonCriticalExtension = NULL;
  if ( LOG_DEBUGFLAG(DEBUG_ASN1) ) {
    xer_fprint(stdout, &asn_DEF_NR_UL_DCCH_Message, (void *)&ul_dcch_msg);
  }

  asn_enc_rval_t enc_rval = uper_encode_to_buffer(&asn_DEF_NR_UL_DCCH_Message, NULL, (void *)&ul_dcch_msg, buffer, buffer_size);
  AssertFatal (enc_rval.encoded > 0, "ASN1 message encoding failed (%s, %lu)!\n",
               enc_rval.failed_type->name, enc_rval.encoded);
  LOG_I(NR_RRC,"rrcReconfigurationComplete Encoded %zd bits (%zd bytes)\n",enc_rval.encoded,(enc_rval.encoded+7)/8);
  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NR_UL_DCCH_Message, &ul_dcch_msg);
  return((enc_rval.encoded+7)/8);
}

int do_RRCSetupComplete(uint8_t *buffer,
                        size_t buffer_size,
                        const uint8_t Transaction_id,
                        uint8_t sel_plmn_id,
                        bool is_rrc_connection_setup,
                        uint64_t fiveG_s_tmsi,
                        const int dedicatedInfoNASLength,
                        const char *dedicatedInfoNAS)
{
  NR_UL_DCCH_Message_t ul_dcch_msg = {0};
  ul_dcch_msg.message.present = NR_UL_DCCH_MessageType_PR_c1;
  ul_dcch_msg.message.choice.c1 = CALLOC(1,sizeof(struct NR_UL_DCCH_MessageType__c1));
  ul_dcch_msg.message.choice.c1->present = NR_UL_DCCH_MessageType__c1_PR_rrcSetupComplete;
  ul_dcch_msg.message.choice.c1->choice.rrcSetupComplete = CALLOC(1, sizeof(NR_RRCSetupComplete_t));
  NR_RRCSetupComplete_t *RrcSetupComplete = ul_dcch_msg.message.choice.c1->choice.rrcSetupComplete;
  RrcSetupComplete->rrc_TransactionIdentifier = Transaction_id;
  RrcSetupComplete->criticalExtensions.present = NR_RRCSetupComplete__criticalExtensions_PR_rrcSetupComplete;
  RrcSetupComplete->criticalExtensions.choice.rrcSetupComplete = CALLOC(1, sizeof(NR_RRCSetupComplete_IEs_t));
  NR_RRCSetupComplete_IEs_t *ies = RrcSetupComplete->criticalExtensions.choice.rrcSetupComplete;
  ies->selectedPLMN_Identity = sel_plmn_id;
  ies->registeredAMF = NULL;
  /* RRCSetup is received in response to an RRCSetupRequest
   * set the ng-5G-S-TMSI-Value to ng-5G-S-TMSI-Part2
   * i.e. the leftmost 9 bits of 5G-S-TMSI (5.3.3.4 of 3GPP TS 38.331) */
  if (fiveG_s_tmsi != UINT64_MAX) {
    if (is_rrc_connection_setup) {
      ies->ng_5G_S_TMSI_Value = calloc_or_fail(1, sizeof(*ies->ng_5G_S_TMSI_Value));
      ies->ng_5G_S_TMSI_Value->present = NR_RRCSetupComplete_IEs__ng_5G_S_TMSI_Value_PR_ng_5G_S_TMSI_Part2;
      BIT_STRING_t *str = &ies->ng_5G_S_TMSI_Value->choice.ng_5G_S_TMSI_Part2;
      str->size = 2;
      str->bits_unused = 7;
      str->buf = calloc_or_fail(str->size, sizeof(str->buf[0]));
      uint16_t fiveG_s_tmsi_part2 = (fiveG_s_tmsi >> 39) & ((1ULL << 9) - 1);
      str->buf[0] = (fiveG_s_tmsi_part2 >> (8 - str->bits_unused)) & 0xFF;
      str->buf[1] = (fiveG_s_tmsi_part2 << str->bits_unused) & 0xFF;
      LOG_D(NR_RRC, "5G-S-TMSI part 2 %d in RRCSetupComplete (5G-S-TMSI %ld)\n", fiveG_s_tmsi_part2, fiveG_s_tmsi);
    } else {
      ies->ng_5G_S_TMSI_Value = CALLOC(1, sizeof(struct NR_RRCSetupComplete_IEs__ng_5G_S_TMSI_Value));
      ies->ng_5G_S_TMSI_Value->present = NR_RRCSetupComplete_IEs__ng_5G_S_TMSI_Value_PR_ng_5G_S_TMSI;
      FIVEG_S_TMSI_TO_BIT_STRING(fiveG_s_tmsi, &ies->ng_5G_S_TMSI_Value->choice.ng_5G_S_TMSI);
      LOG_D(NR_RRC, "5G-S-TMSI %lu in RRCSetupComplete\n", fiveG_s_tmsi);
    }
  } else {
    LOG_D(NR_RRC, "5G-S-TMSI is not available!\n");
    ies->ng_5G_S_TMSI_Value = NULL;
  }

  memset(&ies->dedicatedNAS_Message,0,sizeof(OCTET_STRING_t));
  OCTET_STRING_fromBuf(&ies->dedicatedNAS_Message, dedicatedInfoNAS, dedicatedInfoNASLength);
  if ( LOG_DEBUGFLAG(DEBUG_ASN1) ) {
    xer_fprint(stdout, &asn_DEF_NR_UL_DCCH_Message, (void *)&ul_dcch_msg);
  }

  asn_enc_rval_t enc_rval = uper_encode_to_buffer(&asn_DEF_NR_UL_DCCH_Message,
                                                  NULL,
                                                  (void *)&ul_dcch_msg,
                                                  buffer,
                                                  buffer_size);
  AssertFatal(enc_rval.encoded > 0,"ASN1 message encoding failed (%s, %lu)!\n",
              enc_rval.failed_type->name,enc_rval.encoded);
  LOG_D(NR_RRC,"RRCSetupComplete Encoded %zd bits (%zd bytes)\n",enc_rval.encoded,(enc_rval.encoded+7)/8);

  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NR_UL_DCCH_Message, &ul_dcch_msg);
  return((enc_rval.encoded+7)/8);
}

int do_NR_DLInformationTransfer(uint8_t *buffer,
                                size_t buffer_len,
                                uint8_t transaction_id,
                                uint32_t pdu_length,
                                uint8_t *pdu_buffer)
{
  NR_DL_DCCH_Message_t dl_dcch_msg = {0};
  dl_dcch_msg.message.present = NR_DL_DCCH_MessageType_PR_c1;
  asn1cCalloc(dl_dcch_msg.message.choice.c1, c1);
  c1->present = NR_DL_DCCH_MessageType__c1_PR_dlInformationTransfer;

  asn1cCalloc(c1->choice.dlInformationTransfer, infoTransfer);
  infoTransfer->rrc_TransactionIdentifier = transaction_id;
  infoTransfer->criticalExtensions.present = NR_DLInformationTransfer__criticalExtensions_PR_dlInformationTransfer;

  asn1cCalloc(infoTransfer->criticalExtensions.choice.dlInformationTransfer, dlInfoTransfer);
  asn1cCalloc(dlInfoTransfer->dedicatedNAS_Message, msg);
  // we will free the caller buffer, that is ok in the present code logic (else it will leak memory) but not natural,
  // comprehensive code design
  msg->buf = pdu_buffer;
  msg->size = pdu_length;

  asn_enc_rval_t r = uper_encode_to_buffer(&asn_DEF_NR_DL_DCCH_Message, NULL, (void *)&dl_dcch_msg, buffer, buffer_len);
  AssertFatal(r.encoded > 0, "ASN1 message encoding failed (%s, %ld)!\n", "DLInformationTransfer", r.encoded);
  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NR_DL_DCCH_Message, &dl_dcch_msg);
  LOG_D(NR_RRC, "DLInformationTransfer Encoded %zd bytes\n", r.encoded);
  // for (int i=0;i<encoded;i++) printf("%02x ",(*buffer)[i]);
  return (r.encoded + 7) / 8;
}

int do_NR_ULInformationTransfer(uint8_t **buffer, uint32_t pdu_length, uint8_t *pdu_buffer)
{
    ssize_t encoded;
    NR_UL_DCCH_Message_t ul_dcch_msg;
    memset(&ul_dcch_msg, 0, sizeof(NR_UL_DCCH_Message_t));
    ul_dcch_msg.message.present           = NR_UL_DCCH_MessageType_PR_c1;
    ul_dcch_msg.message.choice.c1          = CALLOC(1,sizeof(struct NR_UL_DCCH_MessageType__c1));
    ul_dcch_msg.message.choice.c1->present = NR_UL_DCCH_MessageType__c1_PR_ulInformationTransfer;
    ul_dcch_msg.message.choice.c1->choice.ulInformationTransfer = CALLOC(1,sizeof(struct NR_ULInformationTransfer));
    ul_dcch_msg.message.choice.c1->choice.ulInformationTransfer->criticalExtensions.present = NR_ULInformationTransfer__criticalExtensions_PR_ulInformationTransfer;
    ul_dcch_msg.message.choice.c1->choice.ulInformationTransfer->criticalExtensions.choice.ulInformationTransfer = CALLOC(1,sizeof(struct NR_ULInformationTransfer_IEs));
    struct NR_ULInformationTransfer_IEs *ulInformationTransfer = ul_dcch_msg.message.choice.c1->choice.ulInformationTransfer->criticalExtensions.choice.ulInformationTransfer;
    ulInformationTransfer->dedicatedNAS_Message = CALLOC(1,sizeof(NR_DedicatedNAS_Message_t));
    ulInformationTransfer->dedicatedNAS_Message->buf = pdu_buffer;
    ulInformationTransfer->dedicatedNAS_Message->size = pdu_length;
    ulInformationTransfer->lateNonCriticalExtension = NULL;
    encoded = uper_encode_to_new_buffer (&asn_DEF_NR_UL_DCCH_Message, NULL, (void *) &ul_dcch_msg, (void **) buffer);
    AssertFatal(encoded > 0,"ASN1 message encoding failed (%s, %ld)!\n",
                "ULInformationTransfer",encoded);
    ulInformationTransfer->dedicatedNAS_Message->buf = NULL; // Let caller decide when to free it
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NR_UL_DCCH_Message, &ul_dcch_msg);
    LOG_D(NR_RRC,"ULInformationTransfer Encoded %zd bytes\n",encoded);

    return encoded;
}

int do_RRCReestablishmentRequest(uint8_t *buffer,
                                 NR_ReestablishmentCause_t cause,
                                 uint32_t cell_id,
                                 uint16_t c_rnti)
{
  asn_enc_rval_t enc_rval;
  NR_UL_CCCH_Message_t ul_ccch_msg;
  NR_RRCReestablishmentRequest_t *rrcReestablishmentRequest;
  uint8_t buf[2];

  memset((void *)&ul_ccch_msg,0,sizeof(NR_UL_CCCH_Message_t));
  ul_ccch_msg.message.present            = NR_UL_CCCH_MessageType_PR_c1;
  ul_ccch_msg.message.choice.c1          = CALLOC(1, sizeof(struct NR_UL_CCCH_MessageType__c1));
  ul_ccch_msg.message.choice.c1->present = NR_UL_CCCH_MessageType__c1_PR_rrcReestablishmentRequest;
  ul_ccch_msg.message.choice.c1->choice.rrcReestablishmentRequest = CALLOC(1, sizeof(NR_RRCReestablishmentRequest_t));

  rrcReestablishmentRequest = ul_ccch_msg.message.choice.c1->choice.rrcReestablishmentRequest;
  // test
  rrcReestablishmentRequest->rrcReestablishmentRequest.reestablishmentCause = cause;
  rrcReestablishmentRequest->rrcReestablishmentRequest.ue_Identity.c_RNTI = c_rnti;
  rrcReestablishmentRequest->rrcReestablishmentRequest.ue_Identity.physCellId = cell_id;
  // TODO properly setting shortMAC-I (see 5.3.7.4 of 331)
  rrcReestablishmentRequest->rrcReestablishmentRequest.ue_Identity.shortMAC_I.buf = buf;
  rrcReestablishmentRequest->rrcReestablishmentRequest.ue_Identity.shortMAC_I.buf[0] = 0x08;
  rrcReestablishmentRequest->rrcReestablishmentRequest.ue_Identity.shortMAC_I.buf[1] = 0x32;
  rrcReestablishmentRequest->rrcReestablishmentRequest.ue_Identity.shortMAC_I.size = 2;

  if (LOG_DEBUGFLAG(DEBUG_ASN1)) {
    xer_fprint(stdout, &asn_DEF_NR_UL_CCCH_Message, (void *)&ul_ccch_msg);
  }

  enc_rval = uper_encode_to_buffer(&asn_DEF_NR_UL_CCCH_Message,
                                   NULL,
                                   (void *)&ul_ccch_msg,
                                   buffer,
                                   100);
  AssertFatal (enc_rval.encoded > 0, "ASN1 message encoding failed (%s, %lu)!\n", enc_rval.failed_type->name, enc_rval.encoded);
  // shortMAC_I.buf is on the stack, cannot free useing ASN_STRUCT_FREE macro
  rrcReestablishmentRequest->rrcReestablishmentRequest.ue_Identity.shortMAC_I.buf = NULL;
  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NR_UL_CCCH_Message, &ul_ccch_msg);
  LOG_D(NR_RRC,"[UE] RRCReestablishmentRequest Encoded %zd bits (%zd bytes)\n", enc_rval.encoded, (enc_rval.encoded+7)/8);
  return((enc_rval.encoded+7)/8);
}

//------------------------------------------------------------------------------
int do_RRCReestablishment(int8_t nh_ncc, uint8_t *const buffer, size_t buffer_size, const uint8_t Transaction_id)
{
  asn_enc_rval_t enc_rval;
  NR_DL_DCCH_Message_t dl_dcch_msg = {0};
  NR_RRCReestablishment_t *rrcReestablishment = NULL;

  dl_dcch_msg.message.present = NR_DL_DCCH_MessageType_PR_c1;
  dl_dcch_msg.message.choice.c1 = calloc(1, sizeof(struct NR_DL_DCCH_MessageType__c1));
  dl_dcch_msg.message.choice.c1->present = NR_DL_DCCH_MessageType__c1_PR_rrcReestablishment;
  dl_dcch_msg.message.choice.c1->choice.rrcReestablishment = CALLOC(1, sizeof(NR_RRCReestablishment_t));
  rrcReestablishment = dl_dcch_msg.message.choice.c1->choice.rrcReestablishment;

  /****************************** masterCellGroup ******************************/
  rrcReestablishment->rrc_TransactionIdentifier = Transaction_id;
  rrcReestablishment->criticalExtensions.present = NR_RRCReestablishment__criticalExtensions_PR_rrcReestablishment;
  rrcReestablishment->criticalExtensions.choice.rrcReestablishment = CALLOC(1, sizeof(NR_RRCReestablishment_IEs_t));

  // 3GPP TS 33.501 Section 6.11 Security handling for RRC connection re-establishment procedure
  rrcReestablishment->criticalExtensions.choice.rrcReestablishment->nextHopChainingCount = nh_ncc >= 0 ? nh_ncc : 0;
  rrcReestablishment->criticalExtensions.choice.rrcReestablishment->lateNonCriticalExtension = NULL;
  rrcReestablishment->criticalExtensions.choice.rrcReestablishment->nonCriticalExtension = NULL;

  if (LOG_DEBUGFLAG(DEBUG_ASN1)) {
    xer_fprint(stdout, &asn_DEF_NR_DL_DCCH_Message, (void *)&dl_dcch_msg);
  }

  enc_rval = uper_encode_to_buffer(&asn_DEF_NR_DL_DCCH_Message, NULL, (void *)&dl_dcch_msg, buffer, buffer_size);

  AssertFatal(enc_rval.encoded > 0, "ASN1 message encoding failed (%s, %lu)!\n",
              enc_rval.failed_type->name, enc_rval.encoded);
  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NR_DL_DCCH_Message, &dl_dcch_msg);

  LOG_D(NR_RRC, "RRCReestablishment Encoded %u bits (%u bytes)\n", (uint32_t)enc_rval.encoded, (uint32_t)(enc_rval.encoded + 7) / 8);
  return ((enc_rval.encoded + 7) / 8);
}

int do_RRCReestablishmentComplete(uint8_t *buffer, size_t buffer_size, int64_t rrc_TransactionIdentifier)
{
  asn_enc_rval_t enc_rval;
  NR_UL_DCCH_Message_t ul_dcch_msg;
  NR_RRCReestablishmentComplete_t *rrcReestablishmentComplete;

  memset((void *)&ul_dcch_msg,0,sizeof(NR_UL_DCCH_Message_t));
  ul_dcch_msg.message.present            = NR_UL_DCCH_MessageType_PR_c1;
  ul_dcch_msg.message.choice.c1          = CALLOC(1, sizeof(struct NR_UL_DCCH_MessageType__c1));
  ul_dcch_msg.message.choice.c1->present = NR_UL_DCCH_MessageType__c1_PR_rrcReestablishmentComplete;
  ul_dcch_msg.message.choice.c1->choice.rrcReestablishmentComplete = CALLOC(1, sizeof(NR_RRCReestablishmentComplete_t));

  rrcReestablishmentComplete = ul_dcch_msg.message.choice.c1->choice.rrcReestablishmentComplete;
  rrcReestablishmentComplete->rrc_TransactionIdentifier = rrc_TransactionIdentifier;
  rrcReestablishmentComplete->criticalExtensions.present = NR_RRCReestablishmentComplete__criticalExtensions_PR_rrcReestablishmentComplete;
  rrcReestablishmentComplete->criticalExtensions.choice.rrcReestablishmentComplete = CALLOC(1, sizeof(NR_RRCReestablishmentComplete_IEs_t));
  rrcReestablishmentComplete->criticalExtensions.choice.rrcReestablishmentComplete->lateNonCriticalExtension = NULL;
  rrcReestablishmentComplete->criticalExtensions.choice.rrcReestablishmentComplete->nonCriticalExtension = NULL;

  if ( LOG_DEBUGFLAG(DEBUG_ASN1) ) {
    xer_fprint(stdout, &asn_DEF_NR_UL_CCCH_Message, (void *)&ul_dcch_msg);
  }

  enc_rval = uper_encode_to_buffer(&asn_DEF_NR_UL_DCCH_Message,
                                   NULL,
                                   (void *)&ul_dcch_msg,
                                   buffer,
                                   buffer_size);
  AssertFatal (enc_rval.encoded > 0, "ASN1 message encoding failed (%s, %lu)!\n", enc_rval.failed_type->name, enc_rval.encoded);
  LOG_D(NR_RRC,"[UE] RRCReestablishmentComplete Encoded %zd bits (%zd bytes)\n", enc_rval.encoded, (enc_rval.encoded+7)/8);
  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NR_UL_DCCH_Message, &ul_dcch_msg);
  return((enc_rval.encoded+7)/8);
}

static NR_MeasObjectToAddMod_t *get_MeasObject(const struct NR_MeasTiming__frequencyAndTiming *ft,
                                               int band,
                                               NR_ARFCN_ValueNR_t ssbFrequency,
                                               NR_MeasObjectId_t measObjectId)
{
  const NR_SSB_MTC_t *ssb_mtc = &ft->ssb_MeasurementTimingConfiguration;
  NR_MeasObjectToAddMod_t *mo = calloc_or_fail(1, sizeof(*mo));
  mo->measObjectId = measObjectId;
  mo->measObject.present = NR_MeasObjectToAddMod__measObject_PR_measObjectNR;
  NR_MeasObjectNR_t *monr = calloc_or_fail(1, sizeof(*monr));
  asn1cCallocOne(monr->ssbFrequency, ssbFrequency);
  asn1cCallocOne(monr->ssbSubcarrierSpacing, ft->ssbSubcarrierSpacing);
  monr->referenceSignalConfig.ssb_ConfigMobility = calloc_or_fail(1, sizeof(*monr->referenceSignalConfig.ssb_ConfigMobility));
  monr->referenceSignalConfig.ssb_ConfigMobility->deriveSSB_IndexFromCell = true;
  monr->absThreshSS_BlocksConsolidation = calloc_or_fail(1, sizeof(*monr->absThreshSS_BlocksConsolidation));
  asn1cCallocOne(monr->absThreshSS_BlocksConsolidation->thresholdRSRP, 36);
  asn1cCallocOne(monr->nrofSS_BlocksToAverage, 8);
  monr->smtc1 = calloc_or_fail(1, sizeof(*monr->smtc1));
  monr->smtc1->periodicityAndOffset = ssb_mtc->periodicityAndOffset;
  monr->smtc1->duration = ssb_mtc->duration;
  monr->quantityConfigIndex = 1;
  monr->ext1 = calloc_or_fail(1, sizeof(*monr->ext1));
  asn1cCallocOne(monr->ext1->freqBandIndicatorNR, band);
  mo->measObject.choice.measObjectNR = monr;
  return mo;
}

static NR_MeasIdToAddMod_t *get_MeasId(NR_MeasId_t measId, NR_ReportConfigId_t reportConfigId, NR_MeasObjectId_t measObjectId)
{
  NR_MeasIdToAddMod_t *measid = calloc_or_fail(1, sizeof(NR_MeasIdToAddMod_t));
  measid->measId = measId;
  measid->reportConfigId = reportConfigId;
  measid->measObjectId = measObjectId;
  return measid;
}

NR_MeasConfig_t *get_MeasConfig(const NR_MeasTiming_t *mt,
                                int band,
                                int scs,
                                int nr_pci,
                                NR_ReportConfigToAddMod_t *rc_PER,
                                NR_ReportConfigToAddMod_t *rc_A2,
                                seq_arr_t *rc_A3_seq,
                                seq_arr_t *neigh_seq)
{
  DevAssert(mt != NULL && mt->frequencyAndTiming != NULL);
  const struct NR_MeasTiming__frequencyAndTiming *ft = mt->frequencyAndTiming;

  NR_MeasConfig_t *mc = calloc_or_fail(1, sizeof(*mc));
  mc->measObjectToAddModList = calloc_or_fail(1, sizeof(*mc->measObjectToAddModList));
  mc->reportConfigToAddModList = calloc_or_fail(1, sizeof(*mc->reportConfigToAddModList));
  mc->measIdToAddModList = calloc_or_fail(1, sizeof(*mc->measIdToAddModList));

  // Report Configuration: A reporting configuration defines the reporting criteria. The reporting criteria are classified as event
  // triggered reporting, periodic reporting, CGI reporting or SFTD reporting.

  // Periodic report
  if (rc_PER)
    asn1cSeqAdd(&mc->reportConfigToAddModList->list, rc_PER);

  // Event A2
  if (rc_A2)
    asn1cSeqAdd(&mc->reportConfigToAddModList->list, rc_A2);

  // Event A3
  if (rc_A3_seq) {
    for (int i = 0; i < rc_A3_seq->size; i++) {
      NR_ReportConfigToAddMod_t *rc_A3 = (NR_ReportConfigToAddMod_t *)seq_arr_at(rc_A3_seq, i);
      asn1cSeqAdd(&mc->reportConfigToAddModList->list, rc_A3);
    }
  }

  // Measurement Objects: Specifies what is to be measured. For NR and inter-RAT E-UTRA measurements, this may include
  // cell-specific offsets, blacklisted cells to be ignored and whitelisted cells to consider for measurements.

  // Serving cell
  NR_MeasObjectToAddMod_t *mo1 = get_MeasObject(ft, band, ft->carrierFreq, 1);
  NR_MeasObjectNR_t *monr1 = mo1->measObject.choice.measObjectNR;
  monr1->cellsToAddModList = calloc_or_fail(1, sizeof(*monr1->cellsToAddModList));
  NR_CellsToAddMod_t *cell = calloc_or_fail(1, sizeof(*cell));
  cell->physCellId = nr_pci;
  ASN_SEQUENCE_ADD(&monr1->cellsToAddModList->list, cell);
  asn1cSeqAdd(&mc->measObjectToAddModList->list, mo1);

  // Neighbour cells
  if (neigh_seq) {
    int mo_id = 2;
    FOR_EACH_SEQ_ARR(nr_neighbour_cell_t *, neigh_cell, neigh_seq) {
      NR_MeasObjectToAddMod_t *mo_neighbour = get_MeasObject(ft,  neigh_cell->band, neigh_cell->absoluteFrequencySSB, mo_id);
      NR_MeasObjectNR_t *monr = mo_neighbour->measObject.choice.measObjectNR;
      monr->cellsToAddModList = calloc_or_fail(1, sizeof(*monr->cellsToAddModList));
      NR_CellsToAddMod_t *cell = calloc_or_fail(1, sizeof(*cell));
      cell->physCellId = neigh_cell->physicalCellId;
      ASN_SEQUENCE_ADD(&monr->cellsToAddModList->list, cell);
      asn1cSeqAdd(&mc->measObjectToAddModList->list, mo_neighbour);
      mo_id++;
    }
  }

  // Measurement identities: A list of measurement identities where each measurement identity links one measurement object with one
  // reporting configuration. By configuring multiple measurement identities, it is possible to link more than one measurement
  // object to the same reporting configuration, as well as to link more than one reporting configuration to the same measurement
  // object.

  // MeasId for Periodic report
  int meas_idx = 0;
  if (rc_PER) {
    for (; meas_idx < mc->measObjectToAddModList->list.count; meas_idx++) {
      const NR_MeasObjectId_t measObjectId = mc->measObjectToAddModList->list.array[meas_idx]->measObjectId;
      NR_MeasIdToAddMod_t *measid = get_MeasId(meas_idx + 1, rc_PER->reportConfigId, measObjectId);
      asn1cSeqAdd(&mc->measIdToAddModList->list, measid);
    }
  }

  // MeasId for Event A2
  if (rc_A2) {
    NR_MeasIdToAddMod_t *measid_A2 = get_MeasId(meas_idx + 1, rc_A2->reportConfigId, 1);
    meas_idx++;
    asn1cSeqAdd(&mc->measIdToAddModList->list, measid_A2);
  }

  // MeasId for Event A3
  if (neigh_seq) {
    int i = 0;
    FOR_EACH_SEQ_ARR(nr_neighbour_cell_t *, neigh_cell, neigh_seq) {
      NR_ReportConfigId_t reportConfigId = neigh_cell->physicalCellId == -1 ? 3 : i + 4;
      NR_MeasIdToAddMod_t *measid_A3 = get_MeasId(meas_idx + 1, reportConfigId, i + 2);
      meas_idx++;
      asn1cSeqAdd(&mc->measIdToAddModList->list, measid_A3);
      i++;
    }
  }

  // Quantity configurations: The quantity configuration defines the measurement filtering configuration used for measurement event
  // evaluation and related reporting, and for periodical reporting of that measurement.
  mc->quantityConfig = calloc_or_fail(1, sizeof(*mc->quantityConfig));
  mc->quantityConfig->quantityConfigNR_List = calloc_or_fail(1, sizeof(*mc->quantityConfig->quantityConfigNR_List));
  NR_QuantityConfigNR_t *qcnr = calloc_or_fail(1, sizeof(*qcnr));
  asn1cCallocOne(qcnr->quantityConfigCell.ssb_FilterConfig.filterCoefficientRSRP, NR_FilterCoefficient_fc6);
  asn1cCallocOne(qcnr->quantityConfigCell.csi_RS_FilterConfig.filterCoefficientRSRP, NR_FilterCoefficient_fc6);
  asn1cSeqAdd(&mc->quantityConfig->quantityConfigNR_List->list, qcnr);

  return mc;
}

void free_MeasConfig(NR_MeasConfig_t *mc)
{
  ASN_STRUCT_FREE(asn_DEF_NR_MeasConfig, mc);
}

int do_NR_Paging(uint8_t Mod_id, uint8_t *buffer, uint32_t tmsi)
{
  LOG_D(NR_RRC, "[gNB %d] do_NR_Paging start\n", Mod_id);
  NR_PCCH_Message_t pcch_msg = {0};
  pcch_msg.message.present           = NR_PCCH_MessageType_PR_c1;
  asn1cCalloc(pcch_msg.message.choice.c1, c1);
  c1->present = NR_PCCH_MessageType__c1_PR_paging;
  c1->choice.paging = CALLOC(1, sizeof(NR_Paging_t));
  c1->choice.paging->pagingRecordList = CALLOC(
      1, sizeof(*pcch_msg.message.choice.c1->choice.paging->pagingRecordList));
  c1->choice.paging->nonCriticalExtension = NULL;
  asn_set_empty(&c1->choice.paging->pagingRecordList->list);
  c1->choice.paging->pagingRecordList->list.count = 0;

  asn1cSequenceAdd(c1->choice.paging->pagingRecordList->list, NR_PagingRecord_t,
                   paging_record_p);
  /* convert ue_paging_identity_t to PagingUE_Identity_t */
  paging_record_p->ue_Identity.present = NR_PagingUE_Identity_PR_ng_5G_S_TMSI;
  // set ng_5G_S_TMSI
  INT32_TO_BIT_STRING(tmsi, &paging_record_p->ue_Identity.choice.ng_5G_S_TMSI);

  /* add to list */
  LOG_D(NR_RRC, "[gNB %d] do_Paging paging_record: PagingRecordList.count %d\n",
        Mod_id, c1->choice.paging->pagingRecordList->list.count);
  asn_enc_rval_t enc_rval = uper_encode_to_buffer(
      &asn_DEF_NR_PCCH_Message, NULL, (void *)&pcch_msg, buffer, NR_RRC_BUF_SIZE);

  if ( LOG_DEBUGFLAG(DEBUG_ASN1) ) {
    xer_fprint(stdout, &asn_DEF_NR_PCCH_Message, (void *)&pcch_msg);
  }
  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NR_PCCH_Message, &pcch_msg);
  if(enc_rval.encoded == -1) {
    LOG_I(NR_RRC, "[gNB AssertFatal]ASN1 message encoding failed (%s, %lu)!\n",
          enc_rval.failed_type->name, enc_rval.encoded);
    return -1;
  }

  return((enc_rval.encoded+7)/8);
}

/* \brief generate HandoverPreparationInformation to be sent to the DU for
 * handover. Takes uecap_buf in encoded form as (1) this is the form present at
 * the CU already (2) we have to clone this information anyway, so can take it
 * in encoded form which we decode + add to the handoverPreparationInformation */
int do_NR_HandoverPreparationInformation(const uint8_t *uecap_buf, int uecap_buf_size, uint8_t *buf, int buf_size)
{
  NR_HandoverPreparationInformation_t *hpi = calloc_or_fail(1, sizeof(*hpi));
  hpi->criticalExtensions.present = NR_HandoverPreparationInformation__criticalExtensions_PR_c1;
  hpi->criticalExtensions.choice.c1 = calloc_or_fail(1, sizeof(*hpi->criticalExtensions.choice.c1));
  hpi->criticalExtensions.choice.c1->present =
      NR_HandoverPreparationInformation__criticalExtensions__c1_PR_handoverPreparationInformation;
  NR_HandoverPreparationInformation_IEs_t *hpi_ie = calloc_or_fail(1, sizeof(*hpi_ie));
  hpi->criticalExtensions.choice.c1->choice.handoverPreparationInformation = hpi_ie;

  NR_UE_CapabilityRAT_ContainerList_t *list = NULL;
  asn_dec_rval_t dec_rval =
      uper_decode_complete(NULL, &asn_DEF_NR_UE_CapabilityRAT_ContainerList, (void **)&list, uecap_buf, uecap_buf_size);
  if (dec_rval.code == RC_OK) {
    hpi_ie->ue_CapabilityRAT_List = *list;
    free(list); /* list itself is not needed, members below will be freed in ASN_STRUCT_FREE */
  } else {
    /* problem with decoding, don't put a capability */
    ASN_STRUCT_FREE(asn_DEF_NR_UE_CapabilityRAT_ContainerList, list);
    list = NULL;
  }

  if (LOG_DEBUGFLAG(DEBUG_ASN1))
    xer_fprint(stdout, &asn_DEF_NR_HandoverPreparationInformation, hpi);

  asn_enc_rval_t enc_rval = uper_encode_to_buffer(&asn_DEF_NR_HandoverPreparationInformation, NULL, hpi, buf, buf_size);
  AssertFatal(enc_rval.encoded > 0, "ASN1 message encoding failed (%s, %lu)!\n", enc_rval.failed_type->name, enc_rval.encoded);

  ASN_STRUCT_FREE(asn_DEF_NR_HandoverPreparationInformation, hpi);
  return (enc_rval.encoded + 7) / 8;
}

int do_NR_MeasConfig(const NR_MeasConfig_t *measconfig, uint8_t *buf, int buf_size)
{
  asn_enc_rval_t enc_rval = uper_encode_to_buffer(&asn_DEF_NR_MeasConfig, NULL, measconfig, buf, buf_size);
  AssertFatal(enc_rval.encoded > 0, "ASN1 message encoding failed (%s, %lu)!\n", enc_rval.failed_type->name, enc_rval.encoded);
  return (enc_rval.encoded + 7) / 8;
}

int do_NR_MeasurementTimingConfiguration(const NR_MeasurementTimingConfiguration_t *mtc, uint8_t *buf, int buf_size)
{
  asn_enc_rval_t enc_rval = uper_encode_to_buffer(&asn_DEF_NR_MeasurementTimingConfiguration, NULL, mtc, buf, buf_size);
  AssertFatal(enc_rval.encoded > 0, "ASN1 message encoding failed (%s, %lu)!\n", enc_rval.failed_type->name, enc_rval.encoded);
  return (enc_rval.encoded + 7) / 8;
}
