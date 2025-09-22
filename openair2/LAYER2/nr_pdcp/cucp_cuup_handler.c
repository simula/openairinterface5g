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

#include "cucp_cuup_handler.h"
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "NR_DRB-ToAddMod.h"
#include "NR_DRB-ToAddModList.h"
#include "NR_PDCP-Config.h"
#include "NR_QFI.h"
#include "NR_SDAP-Config.h"
#include "RRC/NR/nr_rrc_common.h"
#include "SDAP/nr_sdap/nr_sdap_entity.h"
#include "asn_internal.h"
#include "assertions.h"
#include "common/utils/T/T.h"
#include "common/utils/oai_asn1.h"
#include "constr_TYPE.h"
#include "cuup_cucp_if.h"
#include "gtpv1_u_messages_types.h"
#include "nr_pdcp/nr_pdcp_entity.h"
#include "nr_pdcp_oai_api.h"
#include "openair2/COMMON/e1ap_messages_types.h"
#include "openair2/E1AP/e1ap_common.h"
#include "openair2/F1AP/f1ap_common.h"
#include "openair2/F1AP/f1ap_ids.h"
#include "openair2/SDAP/nr_sdap/nr_sdap.h"
#include "openair3/ocp-gtpu/gtp_itf.h"

static void fill_DRB_configList_e1(NR_DRB_ToAddModList_t *DRB_configList, const pdu_session_to_setup_t *pdu)
{
  for (int i = 0; i < pdu->numDRB2Setup; i++) {
    const DRB_nGRAN_to_setup_t *drb = pdu->DRBnGRanList + i;
    asn1cSequenceAdd(DRB_configList->list, struct NR_DRB_ToAddMod, ie);
    ie->drb_Identity = drb->id;
    ie->cnAssociation = CALLOC(1, sizeof(*ie->cnAssociation));
    ie->cnAssociation->present = NR_DRB_ToAddMod__cnAssociation_PR_sdap_Config;

    // sdap_Config
    asn1cCalloc(ie->cnAssociation->choice.sdap_Config, sdap_config);
    sdap_config->pdu_Session = pdu->sessionId;
    /* SDAP */
    sdap_config->sdap_HeaderDL = drb->sdap_config.sDAP_Header_DL;
    sdap_config->sdap_HeaderUL = drb->sdap_config.sDAP_Header_UL;
    sdap_config->defaultDRB    = drb->sdap_config.defaultDRB;
    asn1cCalloc(sdap_config->mappedQoS_FlowsToAdd, FlowsToAdd);
    for (int j = 0; j < drb->numQosFlow2Setup; j++) {
      asn1cSequenceAdd(FlowsToAdd->list, NR_QFI_t, qfi);
      *qfi = drb->qosFlows[j].qfi;
    }
    sdap_config->mappedQoS_FlowsToRelease = NULL;

    // pdcp_Config
    ie->reestablishPDCP = NULL;
    ie->recoverPDCP = NULL;
    asn1cCalloc(ie->pdcp_Config, pdcp_config);
    asn1cCalloc(pdcp_config->drb, drbCfg);
    asn1cCallocOne(drbCfg->discardTimer, drb->pdcp_config.discardTimer);
    asn1cCallocOne(drbCfg->pdcp_SN_SizeUL, drb->pdcp_config.pDCP_SN_Size_UL);
    asn1cCallocOne(drbCfg->pdcp_SN_SizeDL, drb->pdcp_config.pDCP_SN_Size_DL);
    drbCfg->headerCompression.present = NR_PDCP_Config__drb__headerCompression_PR_notUsed;
    drbCfg->headerCompression.choice.notUsed = 0;
    drbCfg->integrityProtection = NULL;
    drbCfg->statusReportRequired = NULL;
    drbCfg->outOfOrderDelivery = NULL;
    pdcp_config->moreThanOneRLC = NULL;
    pdcp_config->t_Reordering = calloc(1, sizeof(*pdcp_config->t_Reordering));
    *pdcp_config->t_Reordering = drb->pdcp_config.reorderingTimer;
    pdcp_config->ext1 = NULL;
    const security_indication_t *sec = &pdu->securityIndication;
    if (sec->integrityProtectionIndication == SECURITY_REQUIRED ||
        sec->integrityProtectionIndication == SECURITY_PREFERRED) {
      asn1cCallocOne(drbCfg->integrityProtection, NR_PDCP_Config__drb__integrityProtection_enabled);
    }
    if (sec->confidentialityProtectionIndication == SECURITY_NOT_NEEDED) {
      asn1cCalloc(pdcp_config->ext1, ext1);
      asn1cCallocOne(ext1->cipheringDisabled, NR_PDCP_Config__ext1__cipheringDisabled_true);
    }
  }
}

static int drb_gtpu_create(instance_t instance,
                           uint32_t ue_id,
                           int incoming_id,
                           int outgoing_id,
                           int qfi,
                           in_addr_t tlAddress, // only IPv4 now
                           teid_t outgoing_teid,
                           gtpCallback callBack,
                           gtpCallbackSDAP callBackSDAP,
                           gtpv1u_gnb_create_tunnel_resp_t *create_tunnel_resp)
{
  gtpv1u_gnb_create_tunnel_req_t create_tunnel_req = {0};
  create_tunnel_req.incoming_rb_id[0] = incoming_id;
  create_tunnel_req.pdusession_id[0] = outgoing_id;
  memcpy(&create_tunnel_req.dst_addr[0].buffer, &tlAddress, sizeof(uint8_t) * 4);
  create_tunnel_req.dst_addr[0].length = 32;
  create_tunnel_req.outgoing_teid[0] = outgoing_teid;
  create_tunnel_req.outgoing_qfi[0] = qfi;
  create_tunnel_req.num_tunnels = 1;
  create_tunnel_req.ue_id = ue_id;

  // we use gtpv1u_create_ngu_tunnel because it returns the interface
  // address and port of the interface; apart from that, we also might call
  // newGtpuCreateTunnel() directly
  return gtpv1u_create_ngu_tunnel(instance, &create_tunnel_req, create_tunnel_resp, callBack, callBackSDAP);
}

static instance_t get_n3_gtp_instance(void)
{
  const e1ap_upcp_inst_t *inst = getCxtE1(0);
  AssertFatal(inst, "need to have E1 instance\n");
  return inst->gtpInstN3;
}

static instance_t get_f1_gtp_instance(void)
{
  const f1ap_cudu_inst_t *inst = getCxt(0);
  if (!inst)
    return -1; // means no F1
  return inst->gtpInst;
}

void e1_bearer_context_setup(const e1ap_bearer_setup_req_t *req)
{
  bool need_ue_id_mgmt = e1_used();

  /* mirror the CU-CP UE ID for CU-UP */
  uint32_t cu_up_ue_id = req->gNB_cu_cp_ue_id;
  f1_ue_data_t ued = {.secondary_ue = req->gNB_cu_cp_ue_id};
  if (need_ue_id_mgmt && !cu_exists_f1_ue_data(cu_up_ue_id)) {
    bool success = cu_add_f1_ue_data(cu_up_ue_id, &ued);
    DevAssert(success);
    LOG_I(E1AP, "adding UE with CU-CP UE ID %d and CU-UP UE ID %d\n", req->gNB_cu_cp_ue_id, cu_up_ue_id);
  }

  instance_t n3inst = get_n3_gtp_instance();
  instance_t f1inst = get_f1_gtp_instance();

  e1ap_bearer_setup_resp_t resp = {
    .gNB_cu_cp_ue_id = req->gNB_cu_cp_ue_id,
    .gNB_cu_up_ue_id = cu_up_ue_id,
  };
  resp.numPDUSessions = req->numPDUSessions;
  for (int i = 0; i < resp.numPDUSessions; ++i) {
    const pdu_session_to_setup_t *req_pdu = req->pduSession + i;
    LOG_I(E1AP, "UE %d: add PDU session ID %ld (%d bearers)\n", cu_up_ue_id, req_pdu->sessionId, req_pdu->numDRB2Setup);

    pdu_session_setup_t *resp_pdu = resp.pduSession + i;
    resp_pdu->id = req_pdu->sessionId;

    AssertFatal(req_pdu->numDRB2Setup == 1, "can only handle one DRB per PDU session\n");
    resp_pdu->numDRBSetup = req_pdu->numDRB2Setup;
    const DRB_nGRAN_to_setup_t *req_drb = &req_pdu->DRBnGRanList[0];
    AssertFatal(req_drb->numQosFlow2Setup == 1, "can only handle one QoS Flow per DRB\n");
    DRB_nGRAN_setup_t *resp_drb = &resp_pdu->DRBnGRanList[0];
    resp_drb->id = req_drb->id;
    resp_drb->numQosFlowSetup = req_drb->numQosFlow2Setup;
    for (int k = 0; k < resp_drb->numQosFlowSetup; k++) {
      const qos_flow_to_setup_t *qosflow2Setup = &req_drb->qosFlows[k];
      qos_flow_list_t *qosflowSetup = &resp_drb->qosFlows[k];
      qosflowSetup->qfi = qosflow2Setup->qfi;
    }

    // GTP tunnel for N3/to core
    gtpv1u_gnb_create_tunnel_resp_t resp_n3 = {0};
    int qfi = req_drb->qosFlows[0].qfi;
    int ret = drb_gtpu_create(n3inst,
                              cu_up_ue_id,
                              req_drb->id,
                              req_pdu->sessionId,
                              qfi,
                              req_pdu->UP_TL_information.tlAddress,
                              req_pdu->UP_TL_information.teId,
                              nr_pdcp_data_req_drb,
                              sdap_data_req,
                              &resp_n3);
    AssertFatal(ret >= 0, "Unable to create GTP Tunnel for NG-U\n");
    AssertFatal(resp_n3.num_tunnels == req_pdu->numDRB2Setup, "could not create all tunnels\n");
    resp_pdu->tl_info.teId = resp_n3.gnb_NGu_teid[0];
    memcpy(&resp_pdu->tl_info.tlAddress, &resp_n3.gnb_addr.buffer, 4);

    // create PDCP bearers. This will also create SDAP bearers
    NR_DRB_ToAddModList_t DRB_configList = {0};
    fill_DRB_configList_e1(&DRB_configList, req_pdu);
    nr_pdcp_entity_security_keys_and_algos_t security_parameters;
    security_parameters.ciphering_algorithm = req->secInfo.cipheringAlgorithm;
    security_parameters.integrity_algorithm = req->secInfo.integrityProtectionAlgorithm;
    memcpy(security_parameters.ciphering_key, req->secInfo.encryptionKey, NR_K_KEY_SIZE);
    memcpy(security_parameters.integrity_key, req->secInfo.integrityProtectionKey, NR_K_KEY_SIZE);
    nr_pdcp_add_drbs(true, // set this to notify PDCP that his not UE
                     cu_up_ue_id,
                     &DRB_configList,
                     &security_parameters);
    ASN_STRUCT_RESET(asn_DEF_NR_DRB_ToAddModList, &DRB_configList.list);
    if (f1inst >= 0) { /* we have F1(-U) */
      teid_t dummy_teid = 0xffff; // we will update later with answer from DU
      in_addr_t dummy_address = {0}; // IPv4, updated later with answer from DU
      gtpv1u_gnb_create_tunnel_resp_t resp_f1 = {0};
      int qfi = -1; // don't put PDU session marker in GTP
      int ret = drb_gtpu_create(f1inst,
                                cu_up_ue_id,
                                req_drb->id,
                                req_drb->id,
                                qfi,
                                dummy_address,
                                dummy_teid,
                                cu_f1u_data_req,
                                NULL,
                                &resp_f1);
      resp_drb->numUpParam = 1;
      AssertFatal(ret >= 0, "Unable to create GTP Tunnel for F1-U\n");
      memcpy(&resp_drb->UpParamList[0].tl_info.tlAddress, &resp_f1.gnb_addr.buffer, 4);
      resp_drb->UpParamList[0].tl_info.teId = resp_f1.gnb_NGu_teid[0];
    }

    // We assume all DRBs to setup have been setup successfully, so we always
    // send successful outcome in response and no failed DRBs
    resp_pdu->numDRBFailed = 0;
  }

  get_e1_if()->bearer_setup_response(&resp);
}

/**
 * @brief Fill Bearer Context Modification Response and send to callback
 */
void e1_bearer_context_modif(const e1ap_bearer_mod_req_t *req)
{
  AssertFatal(req->numPDUSessionsMod > 0, "need at least one PDU session to modify\n");

  e1ap_bearer_modif_resp_t modif = {
      .gNB_cu_cp_ue_id = req->gNB_cu_cp_ue_id,
      .gNB_cu_up_ue_id = req->gNB_cu_up_ue_id,
      .numPDUSessionsMod = req->numPDUSessionsMod,
  };

  instance_t f1inst = get_f1_gtp_instance();

  /* PDU Session Resource To Modify List (see 9.3.3.11 of TS 38.463) */
  for (int i = 0; i < req->numPDUSessionsMod; i++) {
    DevAssert(req->pduSessionMod[i].sessionId > 0);
    LOG_I(E1AP,
          "UE %d: updating PDU session ID %ld (%ld bearers)\n",
          req->gNB_cu_up_ue_id,
          req->pduSessionMod[i].sessionId,
          req->pduSessionMod[i].numDRB2Modify);
    modif.pduSessionMod[i].id = req->pduSessionMod[i].sessionId;
    modif.pduSessionMod[i].numDRBModified = req->pduSessionMod[i].numDRB2Modify;
    /* DRBs to modify */
    for (int j = 0; j < req->pduSessionMod[i].numDRB2Modify; j++) {
      const DRB_nGRAN_to_mod_t *to_modif = &req->pduSessionMod[i].DRBnGRanModList[j];
      DRB_nGRAN_modified_t *modified = &modif.pduSessionMod[i].DRBnGRanModList[j];
      modified->id = to_modif->id;

      if (to_modif->pdcp_config && to_modif->pdcp_config->pDCP_Reestablishment) {
        nr_pdcp_entity_security_keys_and_algos_t security_parameters;
        if (req->secInfo) {
          security_parameters.ciphering_algorithm = req->secInfo->cipheringAlgorithm;
          security_parameters.integrity_algorithm = req->secInfo->integrityProtectionAlgorithm;
          memcpy(security_parameters.ciphering_key, req->secInfo->encryptionKey, NR_K_KEY_SIZE);
          memcpy(security_parameters.integrity_key, req->secInfo->integrityProtectionKey, NR_K_KEY_SIZE);
        } else {
          /* don't change security settings if not present in the Bearer Context Modification Request */
          security_parameters.ciphering_algorithm = -1;
          security_parameters.integrity_algorithm = -1;
        }
        nr_pdcp_reestablishment(req->gNB_cu_up_ue_id,
                                to_modif->id,
                                false,
                                &security_parameters);
      }

      // PDCP Status Information requested
      if (to_modif->pdcp_sn_status_requested) {
        modified->pdcp_status = malloc_or_fail(sizeof(*modified->pdcp_status));
        nr_pdcp_count_t dl_count = {0};
        nr_pdcp_count_t ul_count = {0};
        nr_pdcp_get_drb_count_values(req->gNB_cu_cp_ue_id, to_modif->id, &ul_count, &dl_count);
        modified->pdcp_status->dl_count.hfn = dl_count.hfn;
        modified->pdcp_status->dl_count.sn = dl_count.sn;
        modified->pdcp_status->ul_count.hfn = ul_count.hfn;
        modified->pdcp_status->ul_count.sn = ul_count.sn;
      }

      // PDCP Status received (from source CU-UP)
      if (to_modif->pdcp_status) {
        DevAssert(to_modif->pdcp_config);
        DevAssert(to_modif->pdcp_config->pDCP_SN_Size_DL == to_modif->pdcp_config->pDCP_SN_Size_UL);
        bool is_sn_len_18 = to_modif->pdcp_config->pDCP_SN_Size_DL == NR_PDCP_Config__drb__pdcp_SN_SizeDL_len18bits;
        nr_pdcp_count_t dl_count = { .hfn = to_modif->pdcp_status->dl_count.hfn, .sn = to_modif->pdcp_status->dl_count.sn};
        nr_pdcp_count_t ul_count = { .hfn = to_modif->pdcp_status->ul_count.hfn, .sn = to_modif->pdcp_status->ul_count.sn};
        nr_pdcp_count_update(req->gNB_cu_cp_ue_id,
                             to_modif->id,
                             dl_count,
                             ul_count,
                             is_sn_len_18 ? LONG_SN_SIZE : SHORT_SN_SIZE);
      }

      if (f1inst < 0) // no F1-U?
        continue; // nothing to do

      /* Loop through DL UP Transport Layer params list
       * and update GTP tunnel outgoing addr and TEID */
      for (int k = 0; k < to_modif->numDlUpParam; k++) {
        in_addr_t addr = to_modif->DlUpParamList[k].tl_info.tlAddress;
        GtpuUpdateTunnelOutgoingAddressAndTeid(f1inst, req->gNB_cu_cp_ue_id, to_modif->id, addr, to_modif->DlUpParamList[k].tl_info.teId);
      }

    }
  }

  get_e1_if()->bearer_modif_response(&modif);
}

static void remove_ue_e1(const uint32_t ue_id)
{
  bool need_ue_id_mgmt = e1_used();

  instance_t n3inst = get_n3_gtp_instance();
  instance_t f1inst = get_f1_gtp_instance();

  newGtpuDeleteAllTunnels(n3inst, ue_id);
  if (f1inst >= 0)  // is there F1-U?
    newGtpuDeleteAllTunnels(f1inst, ue_id);
  if (need_ue_id_mgmt) {
    // see issue #706: in monolithic, gNB will free PDCP of UE
    nr_pdcp_remove_UE(ue_id);
    cu_remove_f1_ue_data(ue_id);
  }
  nr_sdap_delete_ue_entities(ue_id);
}

void e1_bearer_release_cmd(const e1ap_bearer_release_cmd_t *cmd)
{
  LOG_I(E1AP, "releasing UE %d\n", cmd->gNB_cu_up_ue_id);
  remove_ue_e1(cmd->gNB_cu_up_ue_id);

  e1ap_bearer_release_cplt_t cplt = {
    .gNB_cu_cp_ue_id = cmd->gNB_cu_cp_ue_id,
    .gNB_cu_up_ue_id = cmd->gNB_cu_up_ue_id,
  };

  get_e1_if()->bearer_release_complete(&cplt);
}

void e1_reset(void)
{
  /* we get the list of all UEs from the PDCP, which maintains a list */
  ue_id_t ue_ids[MAX_MOBILES_PER_GNB];
  int num = nr_pdcp_get_num_ues(ue_ids, MAX_MOBILES_PER_GNB);
  for (uint32_t i = 0; i < num; ++i) {
    ue_id_t ue_id = ue_ids[i];
    LOG_W(E1AP, "releasing UE %ld\n", ue_id);
    remove_ue_e1(ue_id);
  }
}
