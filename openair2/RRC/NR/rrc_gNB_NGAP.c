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

/*! \file rrc_gNB_NGAP.h
 * \brief rrc NGAP procedures for gNB
 * \author Yoshio INOUE, Masayuki HARADA
 * \date 2020
 * \version 0.1
 * \email: yoshio.inoue@fujitsu.com,masayuki.harada@fujitsu.com
 *         (yoshio.inoue%40fujitsu.com%2cmasayuki.harada%40fujitsu.com) 
 */

#include "rrc_gNB_NGAP.h"
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <openair3/ocp-gtpu/gtp_itf.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "E1AP_ConfidentialityProtectionIndication.h"
#include "E1AP_IntegrityProtectionIndication.h"
#include "NGAP_CauseRadioNetwork.h"
#include "NGAP_Dynamic5QIDescriptor.h"
#include "NGAP_GTPTunnel.h"
#include "NGAP_NonDynamic5QIDescriptor.h"
#include "NGAP_PDUSessionResourceModifyRequestTransfer.h"
#include "NGAP_PDUSessionResourceSetupRequestTransfer.h"
#include "NGAP_QosFlowAddOrModifyRequestItem.h"
#include "NGAP_QosFlowSetupRequestItem.h"
#include "NGAP_asn_constant.h"
#include "NGAP_ProtocolIE-Field.h"
#include "NR_UE-NR-Capability.h"
#include "NR_UERadioAccessCapabilityInformation.h"
#include "MAC/mac.h"
#include "OCTET_STRING.h"
#include "RRC/NR/MESSAGES/asn1_msg.h"
#include "RRC/NR/nr_rrc_common.h"
#include "RRC/NR/nr_rrc_defs.h"
#include "RRC/NR/nr_rrc_proto.h"
#include "RRC/NR/rrc_gNB_UE_context.h"
#include "RRC/NR/rrc_gNB_radio_bearers.h"
#include "openair2/LAYER2/NR_MAC_COMMON/nr_mac.h"
#include "T.h"
#include "aper_decoder.h"
#include "asn_codecs.h"
#include "assertions.h"
#include "common/ngran_types.h"
#include "common/platform_constants.h"
#include "common/ran_context.h"
#include "common/utils/T/T.h"
#include "constr_TYPE.h"
#include "conversions.h"
#include "e1ap_messages_types.h"
#include "f1ap_messages_types.h"
#include "gtpv1_u_messages_types.h"
#include "intertask_interface.h"
#include "nr_pdcp/nr_pdcp_entity.h"
#include "nr_pdcp/nr_pdcp_oai_api.h"
#include "oai_asn1.h"
#include "openair2/F1AP/f1ap_ids.h"
#include "openair3/SECU/key_nas_deriver.h"
#include "rrc_messages_types.h"
#include "s1ap_messages_types.h"
#include "uper_encoder.h"

#ifdef E2_AGENT
#include "openair2/E2AP/RAN_FUNCTION/O-RAN/ran_func_rc_extern.h"
#endif

/* Masks for NGAP Encryption algorithms, NEA0 is always supported (not coded) */
static const uint16_t NGAP_ENCRYPTION_NEA1_MASK = 0x8000;
static const uint16_t NGAP_ENCRYPTION_NEA2_MASK = 0x4000;
static const uint16_t NGAP_ENCRYPTION_NEA3_MASK = 0x2000;

/* Masks for NGAP Integrity algorithms, NIA0 is always supported (not coded) */
static const uint16_t NGAP_INTEGRITY_NIA1_MASK = 0x8000;
static const uint16_t NGAP_INTEGRITY_NIA2_MASK = 0x4000;
static const uint16_t NGAP_INTEGRITY_NIA3_MASK = 0x2000;

#define INTEGRITY_ALGORITHM_NONE NR_IntegrityProtAlgorithm_nia0

static void set_UE_security_algos(const gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, const ngap_security_capabilities_t *cap);

/*!
 *\brief save security key.
 *\param UE              UE context.
 *\param security_key_pP The security key received from NGAP.
 */
static void set_UE_security_key(gNB_RRC_UE_t *UE, uint8_t *security_key_pP)
{
  int i;

  /* Saves the security key */
  memcpy(UE->kgnb, security_key_pP, SECURITY_KEY_LENGTH);
  memset(UE->nh, 0, SECURITY_KEY_LENGTH);
  UE->nh_ncc = -1;

  char ascii_buffer[65];
  for (i = 0; i < 32; i++) {
    sprintf(&ascii_buffer[2 * i], "%02X", UE->kgnb[i]);
  }
  ascii_buffer[2 * 1] = '\0';

  LOG_I(NR_RRC, "[UE %x] Saved security key %s\n", UE->rnti, ascii_buffer);
}

void nr_rrc_pdcp_config_security(gNB_RRC_UE_t *UE, bool enable_ciphering)
{
  static int                          print_keys= 1;

  /* Derive the keys from kgnb */
  nr_pdcp_entity_security_keys_and_algos_t security_parameters;
  /* set ciphering algorithm depending on 'enable_ciphering' */
  security_parameters.ciphering_algorithm = enable_ciphering ? UE->ciphering_algorithm : 0;
  security_parameters.integrity_algorithm = UE->integrity_algorithm;
  /* use current ciphering algorithm, independently of 'enable_ciphering' to derive ciphering key */
  nr_derive_key(RRC_ENC_ALG, UE->ciphering_algorithm, UE->kgnb, security_parameters.ciphering_key);
  nr_derive_key(RRC_INT_ALG, UE->integrity_algorithm, UE->kgnb, security_parameters.integrity_key);

  if ( LOG_DUMPFLAG( DEBUG_SECURITY ) ) {
    if (print_keys == 1 ) {
      print_keys =0;
      LOG_DUMPMSG(NR_RRC, DEBUG_SECURITY, UE->kgnb, 32, "\nKgNB:");
      LOG_DUMPMSG(NR_RRC, DEBUG_SECURITY, security_parameters.ciphering_key, 16,"\nKRRCenc:" );
      LOG_DUMPMSG(NR_RRC, DEBUG_SECURITY, security_parameters.integrity_key, 16,"\nKRRCint:" );
    }
  }

  nr_pdcp_config_set_security(UE->rrc_ue_id, DL_SCH_LCID_DCCH, true, &security_parameters);
}

/** @brief Process AMF Identifier and fill GUAMI struct members */
static nr_guami_t get_guami(const uint32_t amf_Id, const plmn_id_t plmn)
{
  nr_guami_t guami = {0};
  guami.amf_region_id = (amf_Id >> 16) & 0xff;
  guami.amf_set_id = (amf_Id >> 6) & 0x3ff;
  guami.amf_pointer = amf_Id & 0x3f;
  guami.mcc = plmn.mcc;
  guami.mnc = plmn.mnc;
  guami.mnc_len = plmn.mnc_digit_length;
  return guami;
}

/** @brief Copy NGAP PDU Session Resource item to RRC pdusession_t struct */
static void cp_pdusession_resource_item_to_pdusession(pdusession_t *dst, const pdusession_resource_item_t *src)
{
  dst->pdusession_id = src->pdusession_id;
  dst->nas_pdu = src->nas_pdu;
  dst->nb_qos = src->pdusessionTransfer.nb_qos;
  for (uint8_t i = 0; i < src->pdusessionTransfer.nb_qos && i < QOSFLOW_MAX_VALUE; ++i) {
    dst->qos[i] = src->pdusessionTransfer.qos[i];
  }
  dst->pdu_session_type = src->pdusessionTransfer.pdu_session_type;
  dst->n3_incoming = src->pdusessionTransfer.n3_incoming;
  dst->nssai = src->nssai;
}

/**
 * @brief Prepare the Initial UE Message (Uplink NAS) to be forwarded to the AMF over N2
 *        extracts NAS PDU, Selected PLMN and Registered AMF from the RRCSetupComplete
 */
void rrc_gNB_send_NGAP_NAS_FIRST_REQ(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, NR_RRCSetupComplete_IEs_t *rrcSetupComplete)
{
  MessageDef *message_p = itti_alloc_new_message(TASK_RRC_GNB, rrc->module_id, NGAP_NAS_FIRST_REQ);
  ngap_nas_first_req_t *req = &NGAP_NAS_FIRST_REQ(message_p);
  memset(req, 0, sizeof(*req));

  // RAN UE NGAP ID
  req->gNB_ue_ngap_id = UE->rrc_ue_id;

  // RRC Establishment Cause
  /* Assume that cause is coded in the same way in RRC and NGap, just check that the value is in NGap range */
  AssertFatal(UE->establishment_cause < NGAP_RRC_CAUSE_LAST, "Establishment cause invalid (%jd/%d)!", UE->establishment_cause, NGAP_RRC_CAUSE_LAST);
  req->establishment_cause = UE->establishment_cause;

  // NAS-PDU
  req->nas_pdu = create_byte_array(rrcSetupComplete->dedicatedNAS_Message.size, rrcSetupComplete->dedicatedNAS_Message.buf);

  /* Selected PLMN Identity (Optional)
   * selectedPLMN-Identity in RRCSetupComplete: Index of the PLMN selected by the UE from the plmn-IdentityInfoList (SIB1)
   * Selected PLMN Identity in INITIAL UE MESSAGE: Indicates the selected PLMN id for the non-3GPP access.*/
  if (rrcSetupComplete->selectedPLMN_Identity > rrc->configuration.num_plmn) {
    LOG_E(NGAP,
          "Failed to send Initial UE Message: selected PLMN (%ld) identity is out of bounds (%d)\n",
          rrcSetupComplete->selectedPLMN_Identity,
          rrc->configuration.num_plmn);
    return;
  }
  int selected_plmn_identity = rrcSetupComplete->selectedPLMN_Identity - 1; // Convert 1-based PLMN Identity IE to 0-based index
  req->plmn = rrc->configuration.plmn[selected_plmn_identity]; // Select from the stored list
  plmn_id_t *p = &req->plmn;
  LOG_I(NGAP, "Selected PLMN in the NG Initial UE Message: MCC=%03d MNC=%0*d\n", p->mcc, p->mnc_digit_length, p->mnc);

  /* 5G-S-TMSI */
  if (UE->Initialue_identity_5g_s_TMSI.presence) {
    req->ue_identity.presenceMask |= NGAP_UE_IDENTITIES_FiveG_s_tmsi;
    req->ue_identity.s_tmsi.amf_set_id = UE->Initialue_identity_5g_s_TMSI.amf_set_id;
    req->ue_identity.s_tmsi.amf_pointer = UE->Initialue_identity_5g_s_TMSI.amf_pointer;
    req->ue_identity.s_tmsi.m_tmsi = UE->Initialue_identity_5g_s_TMSI.fiveg_tmsi;
  }

  /* Process Registered AMF IE */
  if (rrcSetupComplete->registeredAMF != NULL) {
    /* Fetch the AMF-Identifier from the registeredAMF IE
     * The IE AMF-Identifier (AMFI) comprises of an AMF Region ID (8b),
     * an AMF Set ID (10b) and an AMF Pointer (6b)
     * as specified in TS 23.003 [21], clause 2.10.1. */
    NR_RegisteredAMF_t *r_amf = rrcSetupComplete->registeredAMF;
    req->ue_identity.presenceMask |= NGAP_UE_IDENTITIES_guami;
    uint32_t amf_Id = BIT_STRING_to_uint32(&r_amf->amf_Identifier);
    UE->ue_guami = req->ue_identity.guami = get_guami(amf_Id, req->plmn);
    LOG_I(NGAP,
          "GUAMI in NGAP_NAS_FIRST_REQ (UE %04x): AMF Set ID %u, Region ID %u, Pointer %u\n",
          UE->rnti,
          req->ue_identity.guami.amf_set_id,
          req->ue_identity.guami.amf_region_id,
          req->ue_identity.guami.amf_pointer);
  }

  itti_send_msg_to_task(TASK_NGAP, rrc->module_id, message_p);
}

/**
 * @brief Triggers bearer setup for the specified UE.
 *
 * This function initiates the setup of bearer contexts for a given UE
 * by preparing and sending an E1AP Bearer Setup Request message to the CU-UP.
 *
 * @return True if bearer setup was successfully initiated, false otherwise
 * @retval true Bearer setup was initiated successfully
 * @retval false No CU-UP is associated, so bearer setup could not be initiated
 *
 * @note the return value is expected to be used (as per declaration)
 *
 */
bool trigger_bearer_setup(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, int n, pdusession_t *sessions, uint64_t ueAggMaxBitRateDownlink)
{
  AssertFatal(UE->as_security_active, "logic bug: security should be active when activating DRBs\n");
  e1ap_bearer_setup_req_t bearer_req = {0};

  // Reject bearers setup if there's no CU-UP associated
  if (!is_cuup_associated(rrc)) {
    return false;
  }

  e1ap_nssai_t cuup_nssai = {0};
  for (int i = 0; i < n; i++) {
    rrc_pdu_session_param_t *pduSession = find_pduSession(UE, sessions[i].pdusession_id, true);
    pdusession_t *session = &pduSession->param;
    cp_pdusession(session, &sessions[i]);
    bearer_req.gNB_cu_cp_ue_id = UE->rrc_ue_id;
    security_information_t *secInfo = &bearer_req.secInfo;
    secInfo->cipheringAlgorithm = rrc->security.do_drb_ciphering ? UE->ciphering_algorithm : 0;
    secInfo->integrityProtectionAlgorithm = rrc->security.do_drb_integrity ? UE->integrity_algorithm : 0;
    nr_derive_key(UP_ENC_ALG, secInfo->cipheringAlgorithm, UE->kgnb, (uint8_t *)secInfo->encryptionKey);
    nr_derive_key(UP_INT_ALG, secInfo->integrityProtectionAlgorithm, UE->kgnb, (uint8_t *)secInfo->integrityProtectionKey);
    bearer_req.ueDlAggMaxBitRate = ueAggMaxBitRateDownlink;
    pdu_session_to_setup_t *pdu = bearer_req.pduSession + bearer_req.numPDUSessions;
    bearer_req.numPDUSessions++;
    pdu->sessionId = session->pdusession_id;
    pdu->nssai = sessions[i].nssai;
    if (cuup_nssai.sst == 0)
      cuup_nssai = pdu->nssai; /* for CU-UP selection below */

    security_indication_t *sec = &pdu->securityIndication;
    sec->integrityProtectionIndication = rrc->security.do_drb_integrity ? SECURITY_REQUIRED
                                                                        : SECURITY_NOT_NEEDED;
    sec->confidentialityProtectionIndication = rrc->security.do_drb_ciphering ? SECURITY_REQUIRED
                                                                              : SECURITY_NOT_NEEDED;
    gtpu_tunnel_t *n3_incoming = &session->n3_incoming;
    pdu->UP_TL_information.teId = n3_incoming->teid;
    memcpy(&pdu->UP_TL_information.tlAddress, n3_incoming->addr.buffer, sizeof(in_addr_t));
    char ip_str[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, n3_incoming->addr.buffer, ip_str, sizeof(ip_str));
    LOG_I(NR_RRC, "Bearer Context Setup: PDU Session ID=%d, incoming TEID=0x%08x, Addr=%s\n", session->pdusession_id, n3_incoming->teid, ip_str);

    /* we assume for the moment one DRB per PDU session. Activate the bearer,
     * and configure in RRC. */
    int drb_id = get_next_available_drb_id(UE);
    drb_t *rrc_drb = generateDRB(UE,
                                 drb_id,
                                 pduSession,
                                 rrc->configuration.enable_sdap,
                                 rrc->security.do_drb_integrity,
                                 rrc->security.do_drb_ciphering,
                                 &rrc->pdcp_config);

    pdu->numDRB2Setup = 1; // One DRB per PDU Session. TODO: Remove hardcoding
    for (int j=0; j < pdu->numDRB2Setup; j++) {
      DRB_nGRAN_to_setup_t *drb = pdu->DRBnGRanList + j;

      drb->id = rrc_drb->drb_id;
      /* SDAP */
      struct sdap_config_s *sdap_config = &rrc_drb->cnAssociation.sdap_config;
      drb->sdap_config.defaultDRB = sdap_config->defaultDRB;
      drb->sdap_config.sDAP_Header_UL = sdap_config->sdap_HeaderUL;
      drb->sdap_config.sDAP_Header_DL = sdap_config->sdap_HeaderDL;
      /* PDCP */
      set_bearer_context_pdcp_config(&drb->pdcp_config, rrc_drb, rrc->configuration.um_on_default_drb);

      drb->numCellGroups = 1; // assume one cell group associated with a DRB

      // Set all Cell Group IDs to MCG
      for (int k=0; k < drb->numCellGroups; k++) {
        cell_group_id_t *cellGroup = drb->cellGroupList + k;
        *cellGroup = MCG;
      }

      drb->numQosFlow2Setup = session->nb_qos;
      for (int k=0; k < drb->numQosFlow2Setup; k++) {
        qos_flow_to_setup_t *qos_flow = drb->qosFlows + k;
        pdusession_level_qos_parameter_t *qos_session = session->qos + k;

        qos_characteristics_t *qos_char = &qos_flow->qos_params.qos_characteristics;
        qos_flow->qfi = qos_session->qfi;
        qos_char->qos_type = qos_session->fiveQI_type;
        if (qos_char->qos_type == DYNAMIC) {
          qos_char->dynamic.fiveqi = qos_session->fiveQI;
          qos_char->dynamic.qos_priority_level = qos_session->qos_priority;
        } else {
          qos_char->non_dynamic.fiveqi = qos_session->fiveQI;
          qos_char->non_dynamic.qos_priority_level = qos_session->qos_priority;
        }

        ngran_allocation_retention_priority_t *rent_priority = &qos_flow->qos_params.alloc_reten_priority;
        ngap_allocation_retention_priority_t *rent_priority_in = &qos_session->allocation_retention_priority;
        rent_priority->priority_level = rent_priority_in->priority_level;
        rent_priority->preemption_capability = rent_priority_in->pre_emp_capability;
        rent_priority->preemption_vulnerability = rent_priority_in->pre_emp_vulnerability;
      }
    }
  }
  /* Limitation: we assume one fixed CU-UP per UE. We base the selection on
   * NSSAI, but the UE might have multiple PDU sessions with differing slices,
   * in which we might need to select different CU-UPs. In this case, we would
   * actually need to group the E1 bearer context setup for the different
   * CU-UPs, and send them to the different CU-UPs. */
  sctp_assoc_t assoc_id = get_new_cuup_for_ue(rrc, UE, cuup_nssai.sst, cuup_nssai.sd);
  rrc->cucp_cuup.bearer_context_setup(assoc_id, &bearer_req);
  return true;
}

/**
 * @brief Fill PDU Session Resource Failed to Setup Item of the
 *        PDU Session Resource Failed to Setup List for either:
 *        - NGAP PDU Session Resource Setup Response
 *        - NGAP Initial Context Setup Response
 */
static void fill_pdu_session_resource_failed_to_setup_item(pdusession_failed_t *f, int pdusession_id, ngap_cause_t cause)
{
  f->pdusession_id = pdusession_id;
  f->cause = cause;
}

/**
 * @brief Fill Initial Context Setup Response with a PDU Session Resource Failed to Setup List
 *        and send ITTI message to TASK_NGAP
 */
static void send_ngap_initial_context_setup_resp_fail(instance_t instance,
                                                      ngap_initial_context_setup_req_t *msg,
                                                      ngap_cause_t cause)
{
  MessageDef *msg_p = itti_alloc_new_message(TASK_RRC_GNB, instance, NGAP_INITIAL_CONTEXT_SETUP_RESP);
  ngap_initial_context_setup_resp_t *resp = &NGAP_INITIAL_CONTEXT_SETUP_RESP(msg_p);
  resp->gNB_ue_ngap_id = msg->gNB_ue_ngap_id;

  for (int i = 0; i < msg->nb_of_pdusessions; i++) {
    fill_pdu_session_resource_failed_to_setup_item(&resp->pdusessions_failed[i], msg->pdusession[i].pdusession_id, cause);
  }
  resp->nb_of_pdusessions = 0;
  resp->nb_of_pdusessions_failed = msg->nb_of_pdusessions;
  itti_send_msg_to_task(TASK_NGAP, instance, msg_p);
}

//------------------------------------------------------------------------------
int rrc_gNB_process_NGAP_INITIAL_CONTEXT_SETUP_REQ(MessageDef *msg_p, instance_t instance)
//------------------------------------------------------------------------------
{
  gNB_RRC_INST *rrc = RC.nrrrc[instance];
  ngap_initial_context_setup_req_t *req = &NGAP_INITIAL_CONTEXT_SETUP_REQ(msg_p);

  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(rrc, req->gNB_ue_ngap_id);
  if (ue_context_p == NULL) {
    /* Can not associate this message to an UE index, send a failure to NGAP and discard it! */
    LOG_W(NR_RRC, "[gNB %ld] In NGAP_INITIAL_CONTEXT_SETUP_REQ: unknown UE from NGAP ids (%u)\n", instance, req->gNB_ue_ngap_id);
    ngap_cause_t cause = { .type = NGAP_CAUSE_RADIO_NETWORK, .value = NGAP_CAUSE_RADIO_NETWORK_UNKNOWN_LOCAL_UE_NGAP_ID};
    rrc_gNB_send_NGAP_INITIAL_CONTEXT_SETUP_FAIL(req->gNB_ue_ngap_id,
                                                 NULL,
                                                 cause);
    return (-1);
  }
  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;

  UE->amf_ue_ngap_id = req->amf_ue_ngap_id;

  // Directly copy the entire guami structure
  UE->ue_guami = req->guami;

  /* NAS PDU */
  // this is malloced pointers, we pass it for later free()
  UE->nas_pdu = req->nas_pdu;

  if (req->nb_of_pdusessions > 0) {
    /* if there are PDU sessions to setup, store them to be created once
     * security (and UE capabilities) are received */
    UE->n_initial_pdu = req->nb_of_pdusessions;
    UE->initial_pdus = calloc_or_fail(UE->n_initial_pdu, sizeof(*UE->initial_pdus));
    for (int i = 0; i < UE->n_initial_pdu; ++i)
      cp_pdusession_resource_item_to_pdusession(&UE->initial_pdus[i], &req->pdusession[i]);
  }

  /* security */
  set_UE_security_algos(rrc, UE, &req->security_capabilities);
  set_UE_security_key(UE, req->security_key);

  /* TS 38.413: "store the received Security Key in the UE context and, if the
   * NG-RAN node is required to activate security for the UE, take this
   * security key into use.": I interpret this as "if AS security is already
   * active, don't do anything" */
  if (!UE->as_security_active) {
    /* configure only integrity, ciphering comes after receiving SecurityModeComplete */
    nr_rrc_pdcp_config_security(UE, false);
    rrc_gNB_generate_SecurityModeCommand(rrc, UE);
  } else {
    /* if AS security key is active, we also have the UE capabilities. Then,
     * there are two possibilities: we should set up PDU sessions, and/or
     * forward the NAS message. */
    if (req->nb_of_pdusessions > 0) {
      // do not remove the above allocation which is reused here: this is used
      // in handle_rrcReconfigurationComplete() to know that we need to send a
      // Initial context setup response message
      if (!trigger_bearer_setup(rrc, UE, UE->n_initial_pdu, UE->initial_pdus, 0)) {
        LOG_W(NR_RRC, "UE %d: reject PDU Session Setup in Initial Context Setup Response\n", UE->rrc_ue_id);
        ngap_cause_t cause = {.type = NGAP_CAUSE_RADIO_NETWORK, .value = NGAP_CAUSE_RADIO_NETWORK_RESOURCES_NOT_AVAILABLE_FOR_THE_SLICE};
        send_ngap_initial_context_setup_resp_fail(rrc->module_id, req, cause);
        rrc_forward_ue_nas_message(rrc, UE);
        return -1;
      }
    } else {
      /* no PDU sesion to setup: acknowledge this message, and forward NAS
       * message, if required */
      rrc_gNB_send_NGAP_INITIAL_CONTEXT_SETUP_RESP(rrc, UE);
      rrc_forward_ue_nas_message(rrc, UE);
    }
  }

#ifdef E2_AGENT
  signal_rrc_state_changed_to(UE, RRC_CONNECTED_RRC_STATE_E2SM_RC);
#endif

  return 0;
}

static gtpu_tunnel_t cp_gtp_tunnel(const gtpu_tunnel_t in)
{
  gtpu_tunnel_t out = {0};
  out.teid = in.teid;
  out.addr.length = in.addr.length;
  memcpy(out.addr.buffer, in.addr.buffer, out.addr.length);
  return out;
}

void rrc_gNB_send_NGAP_INITIAL_CONTEXT_SETUP_RESP(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE)
{
  MessageDef *msg_p = NULL;
  int pdu_sessions_done = 0;
  int pdu_sessions_failed = 0;
  msg_p = itti_alloc_new_message (TASK_RRC_ENB, rrc->module_id, NGAP_INITIAL_CONTEXT_SETUP_RESP);
  ngap_initial_context_setup_resp_t *resp = &NGAP_INITIAL_CONTEXT_SETUP_RESP(msg_p);

  resp->gNB_ue_ngap_id = UE->rrc_ue_id;

  for (int pdusession = 0; pdusession < UE->nb_of_pdusessions; pdusession++) {
    rrc_pdu_session_param_t *session = &UE->pduSession[pdusession];
    if (session->status == PDU_SESSION_STATUS_DONE) {
      pdu_sessions_done++;
      resp->pdusessions[pdusession].pdusession_id = session->param.pdusession_id;
      resp->pdusessions[pdusession].n3_outgoing = cp_gtp_tunnel(session->param.n3_outgoing);
      resp->pdusessions[pdusession].nb_of_qos_flow = session->param.nb_qos;
      for (int qos_flow_index = 0; qos_flow_index < session->param.nb_qos; qos_flow_index++) {
        resp->pdusessions[pdusession].associated_qos_flows[qos_flow_index].qfi = session->param.qos[qos_flow_index].qfi;
        resp->pdusessions[pdusession].associated_qos_flows[qos_flow_index].qos_flow_mapping_ind = QOSFLOW_MAPPING_INDICATION_DL;
      }
    } else if (session->status != PDU_SESSION_STATUS_ESTABLISHED) {
      session->status = PDU_SESSION_STATUS_FAILED;
      ngap_cause_t cause = {.type = NGAP_CAUSE_RADIO_NETWORK, .value = NGAP_CAUSE_RADIO_NETWORK_UNKNOWN_PDU_SESSION_ID};
      fill_pdu_session_resource_failed_to_setup_item(&resp->pdusessions_failed[pdu_sessions_failed],
                                                     session->param.pdusession_id,
                                                     cause);
      pdu_sessions_failed++;
    }
  }

  resp->nb_of_pdusessions = pdu_sessions_done;
  resp->nb_of_pdusessions_failed = pdu_sessions_failed;
  itti_send_msg_to_task (TASK_NGAP, rrc->module_id, msg_p);
}

void rrc_gNB_send_NGAP_INITIAL_CONTEXT_SETUP_FAIL(uint32_t gnb,
                                                  const rrc_gNB_ue_context_t *const ue_context_pP,
                                                  const ngap_cause_t causeP)
{
  MessageDef *msg_p = itti_alloc_new_message(TASK_RRC_GNB, 0, NGAP_INITIAL_CONTEXT_SETUP_FAIL);
  ngap_initial_context_setup_fail_t *fail = &NGAP_INITIAL_CONTEXT_SETUP_FAIL(msg_p);
  memset(fail, 0, sizeof(*fail));
  fail->gNB_ue_ngap_id = gnb;
  fail->cause = causeP;
  itti_send_msg_to_task(TASK_NGAP, 0, msg_p);
}

static NR_CipheringAlgorithm_t rrc_gNB_select_ciphering(const gNB_RRC_INST *rrc, uint16_t algorithms)
{
  int i;
  /* preset nea0 as fallback */
  int ret = 0;

  /* Select ciphering algorithm based on gNB configuration file and
   * UE's supported algorithms.
   * We take the first from the list that is supported by the UE.
   * The ordering of the list comes from the configuration file.
   */
  for (i = 0; i < rrc->security.ciphering_algorithms_count; i++) {
    int nea_mask[4] = {
      0,
      NGAP_ENCRYPTION_NEA1_MASK,
      NGAP_ENCRYPTION_NEA2_MASK,
      NGAP_ENCRYPTION_NEA3_MASK
    };
    if (rrc->security.ciphering_algorithms[i] == 0) {
      /* nea0 */
      break;
    }
    if (algorithms & nea_mask[rrc->security.ciphering_algorithms[i]]) {
      ret = rrc->security.ciphering_algorithms[i];
      break;
    }
  }

  LOG_D(RRC, "selecting ciphering algorithm %d\n", ret);

  return ret;
}

static e_NR_IntegrityProtAlgorithm rrc_gNB_select_integrity(const gNB_RRC_INST *rrc, uint16_t algorithms)
{
  int i;
  /* preset nia0 as fallback */
  int ret = 0;

  /* Select integrity algorithm based on gNB configuration file and
   * UE's supported algorithms.
   * We take the first from the list that is supported by the UE.
   * The ordering of the list comes from the configuration file.
   */
  for (i = 0; i < rrc->security.integrity_algorithms_count; i++) {
    int nia_mask[4] = {
      0,
      NGAP_INTEGRITY_NIA1_MASK,
      NGAP_INTEGRITY_NIA2_MASK,
      NGAP_INTEGRITY_NIA3_MASK
    };
    if (rrc->security.integrity_algorithms[i] == 0) {
      /* nia0 */
      break;
    }
    if (algorithms & nia_mask[rrc->security.integrity_algorithms[i]]) {
      ret = rrc->security.integrity_algorithms[i];
      break;
    }
  }

  LOG_D(RRC, "selecting integrity algorithm %d\n", ret);

  return ret;
}

/*
 * \brief set security algorithms
 * \param rrc     pointer to RRC context
 * \param UE      UE context
 * \param cap     security capabilities for this UE
 */
static void set_UE_security_algos(const gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, const ngap_security_capabilities_t *cap)
{
  /* Save security parameters */
  UE->security_capabilities = *cap;

  /* Select relevant algorithms */
  NR_CipheringAlgorithm_t cipheringAlgorithm = rrc_gNB_select_ciphering(rrc, cap->nRencryption_algorithms);
  e_NR_IntegrityProtAlgorithm integrityProtAlgorithm = rrc_gNB_select_integrity(rrc, cap->nRintegrity_algorithms);

  UE->ciphering_algorithm = cipheringAlgorithm;
  UE->integrity_algorithm = integrityProtAlgorithm;

  LOG_UE_EVENT(UE,
               "Selected security algorithms: ciphering %lx, integrity %x\n",
               cipheringAlgorithm,
               integrityProtAlgorithm);
}

//------------------------------------------------------------------------------
int rrc_gNB_process_NGAP_DOWNLINK_NAS(MessageDef *msg_p, instance_t instance, mui_t *rrc_gNB_mui)
//------------------------------------------------------------------------------
{
  ngap_downlink_nas_t *req = &NGAP_DOWNLINK_NAS(msg_p);
  gNB_RRC_INST *rrc = RC.nrrrc[instance];
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(rrc, req->gNB_ue_ngap_id);

  if (ue_context_p == NULL) {
    /* Can not associate this message to an UE index, send a failure to NGAP and discard it! */
    MessageDef *msg_fail_p;
    LOG_W(NR_RRC, "[gNB %ld] In NGAP_DOWNLINK_NAS: unknown UE from NGAP ids (%u)\n", instance, req->gNB_ue_ngap_id);
    msg_fail_p = itti_alloc_new_message(TASK_RRC_GNB, 0, NGAP_NAS_NON_DELIVERY_IND);
    ngap_nas_non_delivery_ind_t *msg = &NGAP_NAS_NON_DELIVERY_IND(msg_fail_p);
    msg->gNB_ue_ngap_id = req->gNB_ue_ngap_id;
    msg->nas_pdu = req->nas_pdu;
    // TODO add failure cause when defined!
    itti_send_msg_to_task(TASK_NGAP, instance, msg_fail_p);
    return (-1);
  }

  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;
  UE->nas_pdu = req->nas_pdu;
  rrc_forward_ue_nas_message(rrc, UE);
  return 0;
}

void rrc_gNB_send_NGAP_UPLINK_NAS(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, const NR_UL_DCCH_Message_t *const ul_dcch_msg)
{
  NR_ULInformationTransfer_t *ulInformationTransfer = ul_dcch_msg->message.choice.c1->choice.ulInformationTransfer;

  NR_ULInformationTransfer__criticalExtensions_PR p = ulInformationTransfer->criticalExtensions.present;
  if (p != NR_ULInformationTransfer__criticalExtensions_PR_ulInformationTransfer) {
    LOG_E(NR_RRC, "UE %d: expected presence of ulInformationTransfer, but message has %d\n", UE->rrc_ue_id, p);
    return;
  }

  NR_DedicatedNAS_Message_t *nas = ulInformationTransfer->criticalExtensions.choice.ulInformationTransfer->dedicatedNAS_Message;
  if (!nas) {
    LOG_E(NR_RRC, "UE %d: expected NAS message in ulInformation, but it is NULL\n", UE->rrc_ue_id);
    return;
  }

  uint8_t *buf = malloc_or_fail(nas->size);
  memcpy(buf, nas->buf, nas->size);
  MessageDef *msg_p = itti_alloc_new_message(TASK_RRC_GNB, rrc->module_id, NGAP_UPLINK_NAS);
  NGAP_UPLINK_NAS(msg_p).gNB_ue_ngap_id = UE->rrc_ue_id;
  NGAP_UPLINK_NAS(msg_p).nas_pdu.len = nas->size;
  NGAP_UPLINK_NAS(msg_p).nas_pdu.buf = buf;
  itti_send_msg_to_task(TASK_NGAP, rrc->module_id, msg_p);
}

void rrc_gNB_send_NGAP_PDUSESSION_SETUP_RESP(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, uint8_t xid)
{
  MessageDef *msg_p;
  int pdu_sessions_done = 0;
  int pdu_sessions_failed = 0;

  msg_p = itti_alloc_new_message (TASK_RRC_GNB, rrc->module_id, NGAP_PDUSESSION_SETUP_RESP);
  ngap_pdusession_setup_resp_t *resp = &NGAP_PDUSESSION_SETUP_RESP(msg_p);
  resp->gNB_ue_ngap_id = UE->rrc_ue_id;

  for (int pdusession = 0; pdusession < UE->nb_of_pdusessions; pdusession++) {
    rrc_pdu_session_param_t *session = &UE->pduSession[pdusession];
    if (session->status == PDU_SESSION_STATUS_DONE) {
      pdusession_setup_t *tmp = &resp->pdusessions[pdu_sessions_done];
      tmp->pdusession_id = session->param.pdusession_id;
      tmp->nb_of_qos_flow = session->param.nb_qos;
      tmp->n3_outgoing = cp_gtp_tunnel(session->param.n3_outgoing);
      tmp->pdu_session_type = session->param.pdu_session_type;

      for (int qos_flow_index = 0; qos_flow_index < tmp->nb_of_qos_flow; qos_flow_index++) {
        tmp->associated_qos_flows[qos_flow_index].qfi = session->param.qos[qos_flow_index].qfi;
        tmp->associated_qos_flows[qos_flow_index].qos_flow_mapping_ind = QOSFLOW_MAPPING_INDICATION_DL;
      }

      session->status = PDU_SESSION_STATUS_ESTABLISHED;
      char ip_str[INET_ADDRSTRLEN] = {0};
      inet_ntop(AF_INET, tmp->n3_outgoing.addr.buffer, ip_str, sizeof(ip_str));
      LOG_I(NR_RRC, "PDU Session Setup Response: ID=%d, outgoing TEID=0x%08x, Addr=%s\n", tmp->pdusession_id, tmp->n3_outgoing.teid, ip_str);
      pdu_sessions_done++;
    } else if (session->status != PDU_SESSION_STATUS_ESTABLISHED) {
      session->status = PDU_SESSION_STATUS_FAILED;
      pdusession_failed_t *fail = &resp->pdusessions_failed[pdu_sessions_failed];
      fail->pdusession_id = session->param.pdusession_id;
      fail->cause.type = NGAP_CAUSE_RADIO_NETWORK;
      fail->cause.value = NGAP_CAUSE_RADIO_NETWORK_UNKNOWN_PDU_SESSION_ID;
      pdu_sessions_failed++;
    }
    resp->nb_of_pdusessions = pdu_sessions_done;
    resp->nb_of_pdusessions_failed = pdu_sessions_failed;
  }

  if ((pdu_sessions_done > 0 || pdu_sessions_failed)) {
    LOG_I(NR_RRC, "NGAP_PDUSESSION_SETUP_RESP: sending the message\n");
    itti_send_msg_to_task(TASK_NGAP, rrc->module_id, msg_p);
  }

  for(int i = 0; i < NB_RB_MAX; i++) {
    UE->pduSession[i].xid = -1;
  }

  return;
}

/* \brief checks if any transaction is ongoing for any xid of this UE */
static bool transaction_ongoing(const gNB_RRC_UE_t *UE)
{
  for (int xid = 0; xid < NR_RRC_TRANSACTION_IDENTIFIER_NUMBER; ++xid) {
    if (UE->xids[xid] != RRC_ACTION_NONE)
      return true;
  }
  return false;
}

/* \brief delays the ongoing transaction (in msg_p) by setting a timer to wait
 * 10ms; upon expiry, delivers to RRC, which sends the message to itself */
static void delay_transaction(MessageDef *msg_p, int wait_us)
{
  MessageDef *new = itti_alloc_new_message(TASK_RRC_GNB, 0, NGAP_PDUSESSION_SETUP_REQ);
  ngap_pdusession_setup_req_t *n = &NGAP_PDUSESSION_SETUP_REQ(new);
  *n = NGAP_PDUSESSION_SETUP_REQ(msg_p);

  int instance = msg_p->ittiMsgHeader.originInstance;
  long timer_id;
  timer_setup(0, wait_us, TASK_RRC_GNB, instance, TIMER_ONE_SHOT, new, &timer_id);
}

/**
 * @brief Fill PDU Session Resource Setup Response with a list of PDU Session Resources Failed to Setup
 *        and send ITTI message to TASK_NGAP
 */
static void send_ngap_pdu_session_setup_resp_fail(instance_t instance, ngap_pdusession_setup_req_t *msg, ngap_cause_t cause)
{
  MessageDef *msg_resp = itti_alloc_new_message(TASK_RRC_GNB, 0, NGAP_PDUSESSION_SETUP_RESP);
  ngap_pdusession_setup_resp_t *resp = &NGAP_PDUSESSION_SETUP_RESP(msg_resp);
  resp->gNB_ue_ngap_id = msg->gNB_ue_ngap_id;
  resp->nb_of_pdusessions_failed = msg->nb_pdusessions_tosetup;
  resp->nb_of_pdusessions = 0;
  for (int i = 0; i < resp->nb_of_pdusessions_failed; ++i) {
    fill_pdu_session_resource_failed_to_setup_item(&resp->pdusessions_failed[i], msg->pdusession[i].pdusession_id, cause);
  }
  itti_send_msg_to_task(TASK_NGAP, instance, msg_resp);
}

void rrc_gNB_process_NGAP_PDUSESSION_SETUP_REQ(MessageDef *msg_p, instance_t instance)
{
  gNB_RRC_INST *rrc = RC.nrrrc[instance];
  ngap_pdusession_setup_req_t* msg=&NGAP_PDUSESSION_SETUP_REQ(msg_p);
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(rrc, msg->gNB_ue_ngap_id);
  // Reject PDU Session Resource setup if no UE context is found
  if (ue_context_p == NULL) {
    LOG_W(NR_RRC,
          "[gNB %ld] In NGAP_PDUSESSION_SETUP_REQ: no UE context found from UE NGAP ID (%u)\n",
          instance,
          msg->gNB_ue_ngap_id);
    ngap_cause_t cause = {.type = NGAP_CAUSE_RADIO_NETWORK, .value = NGAP_CAUSE_RADIO_NETWORK_UNKNOWN_LOCAL_UE_NGAP_ID};
    send_ngap_pdu_session_setup_resp_fail(instance, msg, cause);
    return;
  }

  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;
  LOG_I(NR_RRC, "UE %d: received PDU Session Resource Setup Request\n", UE->rrc_ue_id);

  // Reject PDU Session Resource setup if gNB_ue_ngap_id is not matching
  if (UE->rrc_ue_id != msg->gNB_ue_ngap_id) {
    LOG_W(NR_RRC, "[gNB %ld] In NGAP_PDUSESSION_SETUP_REQ: unknown UE NGAP ID (%u)\n", instance, msg->gNB_ue_ngap_id);
    ngap_cause_t cause = {.type = NGAP_CAUSE_RADIO_NETWORK, .value = NGAP_CAUSE_RADIO_NETWORK_UNKNOWN_LOCAL_UE_NGAP_ID};
    send_ngap_pdu_session_setup_resp_fail(instance, msg, cause);
    rrc_forward_ue_nas_message(rrc, UE);
    return;
  }

  // Reject PDU Session Resource setup if there is no security context active
  if (!UE->as_security_active) {
    LOG_E(NR_RRC, "UE %d: no security context active for UE, rejecting PDU Session Resource Setup Request\n", UE->rrc_ue_id);
    ngap_cause_t cause = {.type = NGAP_CAUSE_PROTOCOL, .value = NGAP_CAUSE_PROTOCOL_MSG_NOT_COMPATIBLE_WITH_RECEIVER_STATE};
    send_ngap_pdu_session_setup_resp_fail(instance, msg, cause);
    rrc_forward_ue_nas_message(rrc, UE);
    return;
  }

  // Reject PDU session if at least one exists already with that ID.
  // At least one because marking one as existing, and setting up another, that
  // might be more work than is worth it. See 8.2.1.4 in 38.413
  for (int i = 0; i < msg->nb_pdusessions_tosetup; ++i) {
    const pdusession_resource_item_t *p = &msg->pdusession[i];
    rrc_pdu_session_param_t *exist = find_pduSession(UE, p->pdusession_id, false /* don't create */);
    if (exist) {
      LOG_E(NR_RRC, "UE %d: already has existing PDU session %d rejecting PDU Session Resource Setup Request\n", UE->rrc_ue_id, p->pdusession_id);
      ngap_cause_t cause = {.type = NGAP_CAUSE_RADIO_NETWORK, .value = NGAP_CAUSE_RADIO_NETWORK_MULTIPLE_PDU_SESSION_ID_INSTANCES};
      send_ngap_pdu_session_setup_resp_fail(instance, msg, cause);
      rrc_forward_ue_nas_message(rrc, UE);
      return;
    }
  }

  UE->amf_ue_ngap_id = msg->amf_ue_ngap_id;

  /* This is a hack. We observed that with some UEs, PDU session requests might
   * come in quick succession, faster than the RRC reconfiguration for the PDU
   * session requests can be carried out (UE is doing reconfig, and second PDU
   * session request arrives). We don't have currently the means to "queue up"
   * these transactions, which would probably involve some rework of the RRC.
   * To still allow these requests to come in and succeed, we below check and delay transactions
   * for 10ms. However, to not accidentally end up in infinite loops, the
   * maximum number is capped on a per-UE basis as indicated in variable
   * max_delays_pdu_session. */
  if (!UE->ongoing_pdusession_setup_request)
    UE->max_delays_pdu_session = 100;

  if (UE->max_delays_pdu_session > 0 && (transaction_ongoing(UE) || UE->ongoing_pdusession_setup_request)) {
    int wait_us = 10000;
    LOG_I(RRC, "UE %d: delay PDU session setup by %d us, pending %d retries\n", UE->rrc_ue_id, wait_us, UE->max_delays_pdu_session);
    delay_transaction(msg_p, wait_us);
    UE->max_delays_pdu_session--;
    return;
  }

  pdusession_t to_setup[NGAP_MAX_PDU_SESSION] = {0};
  for (int i = 0; i < msg->nb_pdusessions_tosetup; ++i)
    cp_pdusession_resource_item_to_pdusession(&to_setup[i], &msg->pdusession[i]);

  uint64_t dl_ambr = msg->has_ue_ambr ? msg->ueAggMaxBitRate.br_dl : 0;

  if (!trigger_bearer_setup(rrc, UE, msg->nb_pdusessions_tosetup, to_setup, dl_ambr)) {
    // Reject PDU Session Resource setup if there's no CU-UP associated
    LOG_W(NR_RRC, "UE %d: reject PDU Session Setup in PDU Session Resource Setup Response\n", UE->rrc_ue_id);
    ngap_cause_t cause = {.type = NGAP_CAUSE_RADIO_NETWORK, .value = NGAP_CAUSE_RADIO_NETWORK_RESOURCES_NOT_AVAILABLE_FOR_THE_SLICE};
    send_ngap_pdu_session_setup_resp_fail(instance, msg, cause);
    rrc_forward_ue_nas_message(rrc, UE);
  } else {
    UE->ongoing_pdusession_setup_request = true;
  }
}

//------------------------------------------------------------------------------
int rrc_gNB_process_NGAP_PDUSESSION_MODIFY_REQ(MessageDef *msg_p, instance_t instance)
//------------------------------------------------------------------------------
{
  rrc_gNB_ue_context_t *ue_context_p = NULL;

  ngap_pdusession_modify_req_t *req = &NGAP_PDUSESSION_MODIFY_REQ(msg_p);

  gNB_RRC_INST *rrc = RC.nrrrc[instance];
  ue_context_p = rrc_gNB_get_ue_context(rrc, req->gNB_ue_ngap_id);
  if (ue_context_p == NULL) {
    LOG_W(NR_RRC, "[gNB %ld] In NGAP_PDUSESSION_MODIFY_REQ: unknown UE from NGAP ids (%u)\n", instance, req->gNB_ue_ngap_id);
    // TO implement return setup failed
    return (-1);
  }
  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;
  bool all_failed = true;
  for (int i = 0; i < req->nb_pdusessions_tomodify; i++) {
    rrc_pdu_session_param_t *sess;
    const pdusession_resource_item_t *sessMod = req->pdusession + i;
    for (sess = UE->pduSession; sess < UE->pduSession + UE->nb_of_pdusessions; sess++)
      if (sess->param.pdusession_id == sessMod->pdusession_id)
        break;
    if (sess == UE->pduSession + UE->nb_of_pdusessions) {
      LOG_W(NR_RRC, "Requested modification of non-existing PDU session, refusing modification\n");
      UE->nb_of_pdusessions++;
      sess->status = PDU_SESSION_STATUS_FAILED;
      sess->param.pdusession_id = sessMod->pdusession_id;
      sess->cause.type = NGAP_CAUSE_RADIO_NETWORK;
      UE->pduSession[i].cause.type = NGAP_CAUSE_RADIO_NETWORK;
      UE->pduSession[i].cause.value = NGAP_CAUSE_RADIO_NETWORK_UNKNOWN_PDU_SESSION_ID;
    } else {
      all_failed = false;
      sess->status = PDU_SESSION_STATUS_NEW;
      sess->param.pdusession_id = sessMod->pdusession_id;
      sess->cause.type = NGAP_CAUSE_RADIO_NETWORK;
      sess->cause.value = NGAP_CAUSE_RADIO_NETWORK_MULTIPLE_PDU_SESSION_ID_INSTANCES;
      sess->status = PDU_SESSION_STATUS_NEW;
      sess->param.pdusession_id = sessMod->pdusession_id;
      sess->cause.type = NGAP_CAUSE_NOTHING;
      if (sessMod->nas_pdu.buf != NULL) {
        UE->pduSession[i].param.nas_pdu = sessMod->nas_pdu;
      }
      for (int i = 0; i < req->nb_pdusessions_tomodify; ++i)
        cp_pdusession_resource_item_to_pdusession(&UE->pduSession[i].param, &req->pdusession[i]);
    }
  }

  if (!all_failed) {
    rrc_gNB_modify_dedicatedRRCReconfiguration(rrc, UE);
  } else {
    LOG_I(NR_RRC,
          "pdu session modify failed, fill NGAP_PDUSESSION_MODIFY_RESP with the pdu session information that failed to modify \n");
    MessageDef *msg_fail_p = itti_alloc_new_message(TASK_RRC_GNB, 0, NGAP_PDUSESSION_MODIFY_RESP);
    if (msg_fail_p == NULL) {
      LOG_E(NR_RRC, "itti_alloc_new_message failed, msg_fail_p is NULL \n");
      return (-1);
    }
    ngap_pdusession_modify_resp_t *msg = &NGAP_PDUSESSION_MODIFY_RESP(msg_fail_p);
    msg->gNB_ue_ngap_id = req->gNB_ue_ngap_id;
    msg->nb_of_pdusessions = 0;

    for (int i = 0; i < UE->nb_of_pdusessions; i++) {
      if (UE->pduSession[i].status == PDU_SESSION_STATUS_FAILED) {
        msg->pdusessions_failed[i].pdusession_id = UE->pduSession[i].param.pdusession_id;
        msg->pdusessions_failed[i].cause.type = UE->pduSession[i].cause.type;
        msg->pdusessions_failed[i].cause.value = UE->pduSession[i].cause.value;
      }
    }
    itti_send_msg_to_task(TASK_NGAP, instance, msg_fail_p);
  }
  return (0);
}

int rrc_gNB_send_NGAP_PDUSESSION_MODIFY_RESP(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, uint8_t xid)
{
  MessageDef *msg_p = NULL;
  uint8_t pdu_sessions_failed = 0;
  uint8_t pdu_sessions_done = 0;

  msg_p = itti_alloc_new_message (TASK_RRC_GNB, rrc->module_id, NGAP_PDUSESSION_MODIFY_RESP);
  if (msg_p == NULL) {
    LOG_E(NR_RRC, "itti_alloc_new_message failed, msg_p is NULL \n");
    return (-1);
  }
  ngap_pdusession_modify_resp_t *resp = &NGAP_PDUSESSION_MODIFY_RESP(msg_p);
  LOG_I(NR_RRC, "send message NGAP_PDUSESSION_MODIFY_RESP \n");

  resp->gNB_ue_ngap_id = UE->rrc_ue_id;

  for (int i = 0; i < UE->nb_of_pdusessions; i++) {
    if (xid != UE->pduSession[i].xid) {
      LOG_W(NR_RRC, "xid does not correspond (context pdu session index %d, status %d, xid %d/%d) \n ", i, UE->pduSession[i].status, xid, UE->pduSession[i].xid);
      continue;
    }
    if (UE->pduSession[i].status == PDU_SESSION_STATUS_DONE) {
      rrc_pdu_session_param_t *pduSession = find_pduSession(UE, UE->pduSession[i].param.pdusession_id, false);
      if (pduSession) {
        LOG_I(NR_RRC, "update pdu session %d \n", pduSession->param.pdusession_id);
        // Update UE->pduSession
        pduSession->status = PDU_SESSION_STATUS_ESTABLISHED;
        pduSession->cause.type = NGAP_CAUSE_NOTHING;
        for (int qos_flow_index = 0; qos_flow_index < UE->pduSession[i].param.nb_qos; qos_flow_index++) {
          pduSession->param.qos[qos_flow_index] = UE->pduSession[i].param.qos[qos_flow_index];
        }
        resp->pdusessions[pdu_sessions_done].pdusession_id = UE->pduSession[i].param.pdusession_id;
        for (int qos_flow_index = 0; qos_flow_index < UE->pduSession[i].param.nb_qos; qos_flow_index++) {
          resp->pdusessions[pdu_sessions_done].qos[qos_flow_index].qfi = UE->pduSession[i].param.qos[qos_flow_index].qfi;
        }
        resp->pdusessions[pdu_sessions_done].pdusession_id = UE->pduSession[i].param.pdusession_id;
        resp->pdusessions[pdu_sessions_done].nb_of_qos_flow = UE->pduSession[i].param.nb_qos;
        LOG_I(NR_RRC,
              "Modify Resp (msg index %d, pdu session index %d, status %d, xid %d): nb_of_pduSessions %d,  pdusession_id %d \n ",
              pdu_sessions_done,
              i,
              UE->pduSession[i].status,
              xid,
              UE->nb_of_pdusessions,
              resp->pdusessions[pdu_sessions_done].pdusession_id);
        pdu_sessions_done++;
      } else {
        LOG_W(NR_RRC, "PDU SESSION modify of a not existing pdu session %d \n", UE->pduSession[i].param.pdusession_id);
        resp->pdusessions_failed[pdu_sessions_failed].pdusession_id = UE->pduSession[i].param.pdusession_id;
        ngap_cause_t cause = {.type = NGAP_CAUSE_RADIO_NETWORK, .value = NGAP_CAUSE_RADIO_NETWORK_UNKNOWN_PDU_SESSION_ID};
        resp->pdusessions_failed[pdu_sessions_failed].cause = cause;
        pdu_sessions_failed++;
      }
    } else if ((UE->pduSession[i].status == PDU_SESSION_STATUS_NEW) || (UE->pduSession[i].status == PDU_SESSION_STATUS_ESTABLISHED)) {
      LOG_D(NR_RRC, "PDU SESSION is NEW or already ESTABLISHED\n");
    } else if (UE->pduSession[i].status == PDU_SESSION_STATUS_FAILED) {
      resp->pdusessions_failed[pdu_sessions_failed].pdusession_id = UE->pduSession[i].param.pdusession_id;
      resp->pdusessions_failed[pdu_sessions_failed].cause.type = UE->pduSession[i].cause.type;
      resp->pdusessions_failed[pdu_sessions_failed].cause.value = UE->pduSession[i].cause.value;
      pdu_sessions_failed++;
    }
    else
      LOG_W(NR_RRC,
            "Modify pdu session %d, unknown state %d \n ",
            UE->pduSession[i].param.pdusession_id,
            UE->pduSession[i].status);
  }

  resp->nb_of_pdusessions = pdu_sessions_done;
  resp->nb_of_pdusessions_failed = pdu_sessions_failed;

  if (pdu_sessions_done > 0 || pdu_sessions_failed > 0) {
    LOG_D(NR_RRC, "NGAP_PDUSESSION_MODIFY_RESP: sending the message (total pdu session %d)\n", UE->nb_of_pdusessions);
    itti_send_msg_to_task (TASK_NGAP, rrc->module_id, msg_p);
  } else {
    itti_free (ITTI_MSG_ORIGIN_ID(msg_p), msg_p);
  }

  return 0;
}

//------------------------------------------------------------------------------
void rrc_gNB_send_NGAP_UE_CONTEXT_RELEASE_REQ(const module_id_t gnb_mod_idP,
                                              const rrc_gNB_ue_context_t *const ue_context_pP,
                                              const ngap_cause_t causeP)
//------------------------------------------------------------------------------
{
  if (ue_context_pP == NULL) {
    LOG_E(RRC, "[gNB] In NGAP_UE_CONTEXT_RELEASE_REQ: invalid UE\n");
  } else {
    const gNB_RRC_UE_t *UE = &ue_context_pP->ue_context;
    MessageDef *msg = itti_alloc_new_message(TASK_RRC_GNB, 0, NGAP_UE_CONTEXT_RELEASE_REQ);
    ngap_ue_release_req_t *req = &NGAP_UE_CONTEXT_RELEASE_REQ(msg);
    memset(req, 0, sizeof(*req));
    req->gNB_ue_ngap_id = UE->rrc_ue_id;
    req->cause.type = causeP.type;
    req->cause.value = causeP.value;
    for (int i = 0; i < UE->nb_of_pdusessions; i++) {
      req->pdusessions[i].pdusession_id = UE->pduSession[i].param.pdusession_id;
      req->nb_of_pdusessions++;
    }
    itti_send_msg_to_task(TASK_NGAP, GNB_MODULE_ID_TO_INSTANCE(gnb_mod_idP), msg);
  }
}
/*------------------------------------------------------------------------------*/
int rrc_gNB_process_NGAP_UE_CONTEXT_RELEASE_REQ(MessageDef *msg_p, instance_t instance)
{
  uint32_t gNB_ue_ngap_id;
  gNB_ue_ngap_id = NGAP_UE_CONTEXT_RELEASE_REQ(msg_p).gNB_ue_ngap_id;
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(RC.nrrrc[instance], gNB_ue_ngap_id);

  if (ue_context_p == NULL) {
    /* Can not associate this message to an UE index, send a failure to ngAP and discard it! */
    MessageDef *msg_fail_p;
    LOG_W(RRC, "[gNB %ld] In NGAP_UE_CONTEXT_RELEASE_REQ: unknown UE from gNB_ue_ngap_id (%u)\n",
          instance,
          gNB_ue_ngap_id);
    msg_fail_p = itti_alloc_new_message(TASK_RRC_GNB, 0, NGAP_UE_CONTEXT_RELEASE_RESP); /* TODO change message ID. */
    NGAP_UE_CONTEXT_RELEASE_RESP(msg_fail_p).gNB_ue_ngap_id = gNB_ue_ngap_id;
    // TODO add failure cause when defined!
    itti_send_msg_to_task(TASK_NGAP, instance, msg_fail_p);
    return (-1);
  } else {

    /* Send the response */
    MessageDef *msg_resp_p;
    msg_resp_p = itti_alloc_new_message(TASK_RRC_GNB, 0, NGAP_UE_CONTEXT_RELEASE_RESP);
    NGAP_UE_CONTEXT_RELEASE_RESP(msg_resp_p).gNB_ue_ngap_id = gNB_ue_ngap_id;
    itti_send_msg_to_task(TASK_NGAP, instance, msg_resp_p);
    return (0);
  }
}

//-----------------------------------------------------------------------------
/*
* Process the NG command NGAP_UE_CONTEXT_RELEASE_COMMAND, sent by AMF.
* The gNB should remove all pdu session, NG context, and other context of the UE.
*/
int rrc_gNB_process_NGAP_UE_CONTEXT_RELEASE_COMMAND(MessageDef *msg_p, instance_t instance)
{
  gNB_RRC_INST *rrc = RC.nrrrc[0];
  uint32_t gNB_ue_ngap_id = 0;
  gNB_ue_ngap_id = NGAP_UE_CONTEXT_RELEASE_COMMAND(msg_p).gNB_ue_ngap_id;
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(RC.nrrrc[instance], gNB_ue_ngap_id);

  if (ue_context_p == NULL) {
    /* Can not associate this message to an UE index */
    LOG_W(NR_RRC, "[gNB %ld] In NGAP_UE_CONTEXT_RELEASE_COMMAND: unknown UE from gNB_ue_ngap_id (%u)\n",
          instance,
          gNB_ue_ngap_id);
    rrc_gNB_send_NGAP_UE_CONTEXT_RELEASE_COMPLETE(instance, gNB_ue_ngap_id, 0, NULL);
    return -1;
  }

  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;
  UE->an_release = true;
#ifdef E2_AGENT
  signal_rrc_state_changed_to(UE, RRC_IDLE_RRC_STATE_E2SM_RC);
#endif

  /* a UE might not be associated to a CU-UP if it never requested a PDU
   * session (intentionally, or because of erros) */
  if (ue_associated_to_cuup(rrc, UE)) {
    sctp_assoc_t assoc_id = get_existing_cuup_for_ue(rrc, UE);
    e1ap_cause_t cause = {.type = E1AP_CAUSE_RADIO_NETWORK, .value = E1AP_RADIO_CAUSE_NORMAL_RELEASE};
    e1ap_bearer_release_cmd_t cmd = {
      .gNB_cu_cp_ue_id = UE->rrc_ue_id,
      .gNB_cu_up_ue_id = UE->rrc_ue_id,
      .cause = cause,
    };
    rrc->cucp_cuup.bearer_context_release(assoc_id, &cmd);
  }

  /* special case: the DU might be offline, in which case the f1_ue_data exists
   * but is set to 0 */
  if (cu_exists_f1_ue_data(UE->rrc_ue_id) && cu_get_f1_ue_data(UE->rrc_ue_id).du_assoc_id != 0) {
    rrc_gNB_generate_RRCRelease(rrc, UE);

    /* UE will be freed after UE context release complete */
  } else {
    // the DU is offline already
    uint32_t pdu_sessions[NGAP_MAX_PDU_SESSION];
    get_pduSession_array(UE, pdu_sessions);
    rrc_gNB_send_NGAP_UE_CONTEXT_RELEASE_COMPLETE(0, UE->rrc_ue_id, UE->nb_of_pdusessions, pdu_sessions);
    rrc_remove_ue(rrc, ue_context_p);
  }

  return 0;
}

void rrc_gNB_send_NGAP_UE_CONTEXT_RELEASE_COMPLETE(instance_t instance,
                                                   uint32_t gNB_ue_ngap_id,
                                                   int num_pdu,
                                                   uint32_t pdu_session_id[256])
{
  MessageDef *msg = itti_alloc_new_message(TASK_RRC_GNB, 0, NGAP_UE_CONTEXT_RELEASE_COMPLETE);
  NGAP_UE_CONTEXT_RELEASE_COMPLETE(msg).gNB_ue_ngap_id = gNB_ue_ngap_id;
  NGAP_UE_CONTEXT_RELEASE_COMPLETE(msg).num_pdu_sessions = num_pdu;
  for (int i = 0; i < num_pdu; ++i)
    NGAP_UE_CONTEXT_RELEASE_COMPLETE(msg).pdu_session_id[i] = pdu_session_id[i];
  itti_send_msg_to_task(TASK_NGAP, instance, msg);
}

void rrc_gNB_send_NGAP_UE_CAPABILITIES_IND(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, const NR_UECapabilityInformation_t *const ue_cap_info)
//------------------------------------------------------------------------------
{
  NR_UE_CapabilityRAT_ContainerList_t *ueCapabilityRATContainerList =
      ue_cap_info->criticalExtensions.choice.ueCapabilityInformation->ue_CapabilityRAT_ContainerList;
  void *buf;
  NR_UERadioAccessCapabilityInformation_t rac = {0};

  if (ueCapabilityRATContainerList->list.count == 0) {
    LOG_W(RRC, "[UE %d] bad UE capabilities\n", UE->rrc_ue_id);
    }

    int ret = uper_encode_to_new_buffer(&asn_DEF_NR_UE_CapabilityRAT_ContainerList, NULL, ueCapabilityRATContainerList, &buf);
    AssertFatal(ret > 0, "fail to encode ue capabilities\n");

    rac.criticalExtensions.present = NR_UERadioAccessCapabilityInformation__criticalExtensions_PR_c1;
    asn1cCalloc(rac.criticalExtensions.choice.c1, c1);
    c1->present = NR_UERadioAccessCapabilityInformation__criticalExtensions__c1_PR_ueRadioAccessCapabilityInformation;
    asn1cCalloc(c1->choice.ueRadioAccessCapabilityInformation, info);
    info->ue_RadioAccessCapabilityInfo.buf = buf;
    info->ue_RadioAccessCapabilityInfo.size = ret;
    info->nonCriticalExtension = NULL;
    /* 8192 is arbitrary, should be big enough */
    void *buf2 = NULL;
    int encoded = uper_encode_to_new_buffer(&asn_DEF_NR_UERadioAccessCapabilityInformation, NULL, &rac, &buf2);

    AssertFatal(encoded > 0, "fail to encode ue capabilities\n");
    ;
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NR_UERadioAccessCapabilityInformation, &rac);
    MessageDef *msg_p;
    msg_p = itti_alloc_new_message (TASK_RRC_GNB, rrc->module_id, NGAP_UE_CAPABILITIES_IND);
    ngap_ue_cap_info_ind_t *ind = &NGAP_UE_CAPABILITIES_IND(msg_p);
    memset(ind, 0, sizeof(*ind));
    ind->gNB_ue_ngap_id = UE->rrc_ue_id;
    ind->ue_radio_cap.len = encoded;
    ind->ue_radio_cap.buf = buf2;
    itti_send_msg_to_task (TASK_NGAP, rrc->module_id, msg_p);
    LOG_I(NR_RRC,"Send message to ngap: NGAP_UE_CAPABILITIES_IND\n");
}

void rrc_gNB_send_NGAP_PDUSESSION_RELEASE_RESPONSE(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, uint8_t xid)
{
  int pdu_sessions_released = 0;
  MessageDef   *msg_p;
  msg_p = itti_alloc_new_message (TASK_RRC_GNB, rrc->module_id, NGAP_PDUSESSION_RELEASE_RESPONSE);
  ngap_pdusession_release_resp_t *resp = &NGAP_PDUSESSION_RELEASE_RESPONSE(msg_p);
  memset(resp, 0, sizeof(*resp));
  resp->gNB_ue_ngap_id = UE->rrc_ue_id;

  for (int i = 0; i < UE->nb_of_pdusessions; i++) {
    if (xid == UE->pduSession[i].xid) {
      resp->pdusession_release[pdu_sessions_released].pdusession_id = UE->pduSession[i].param.pdusession_id;
      pdu_sessions_released++;
      //clear
      memset(&UE->pduSession[i], 0, sizeof(*UE->pduSession));
      UE->pduSession[i].status = PDU_SESSION_STATUS_RELEASED;
      LOG_W(NR_RRC, "Released pdu session, but code to finish to free memory\n");
    }
  }

  resp->nb_of_pdusessions_released = pdu_sessions_released;
  resp->nb_of_pdusessions_failed = 0;
  LOG_I(NR_RRC, "NGAP PDUSESSION RELEASE RESPONSE: rrc_ue_id %u release_pdu_sessions %d\n", resp->gNB_ue_ngap_id, pdu_sessions_released);
  itti_send_msg_to_task (TASK_NGAP, rrc->module_id, msg_p);
}

//------------------------------------------------------------------------------
int rrc_gNB_process_NGAP_PDUSESSION_RELEASE_COMMAND(MessageDef *msg_p, instance_t instance)
//------------------------------------------------------------------------------
{
  uint32_t gNB_ue_ngap_id;
  ngap_pdusession_release_command_t *cmd = &NGAP_PDUSESSION_RELEASE_COMMAND(msg_p);
  gNB_ue_ngap_id = cmd->gNB_ue_ngap_id;
  gNB_RRC_INST *rrc = RC.nrrrc[instance];
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(rrc, gNB_ue_ngap_id);

  if (!ue_context_p) {
    LOG_E(NR_RRC, "[gNB %ld] not found ue context gNB_ue_ngap_id %u \n", instance, gNB_ue_ngap_id);
    return -1;
  }

  LOG_I(NR_RRC, "[gNB %ld] gNB_ue_ngap_id %u \n", instance, gNB_ue_ngap_id);
  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;
  LOG_I(
      NR_RRC, "PDU Session Release Command: AMF_UE_NGAP_ID %lu  rrc_ue_id %u release_pdusessions %d \n", cmd->amf_ue_ngap_id, gNB_ue_ngap_id, cmd->nb_pdusessions_torelease);
  bool found = false;
  uint8_t xid = rrc_gNB_get_next_transaction_identifier(rrc->module_id);
  UE->xids[xid] = RRC_PDUSESSION_RELEASE;
  for (int pdusession = 0; pdusession < cmd->nb_pdusessions_torelease; pdusession++) {
    rrc_pdu_session_param_t *pduSession = find_pduSession(UE, cmd->pdusession_release_params[pdusession].pdusession_id, false);
    if (!pduSession) {
      LOG_I(NR_RRC, "no pdusession_id, AMF requested to close it id=%d\n", cmd->pdusession_release_params[pdusession].pdusession_id);
      int j=UE->nb_of_pdusessions++;
      UE->pduSession[j].status = PDU_SESSION_STATUS_FAILED;
      UE->pduSession[j].param.pdusession_id = cmd->pdusession_release_params[pdusession].pdusession_id;
      ngap_cause_t cause = {.type = NGAP_CAUSE_RADIO_NETWORK, .value = NGAP_CAUSE_RADIO_NETWORK_UNKNOWN_PDU_SESSION_ID};
      UE->pduSession[j].cause = cause;
      continue;
    }
    if (pduSession->status == PDU_SESSION_STATUS_FAILED) {
      pduSession->xid = xid;
      continue;
    }
    if (pduSession->status == PDU_SESSION_STATUS_ESTABLISHED) {
      found = true;
      LOG_I(NR_RRC, "RELEASE pdusession %d \n", pduSession->param.pdusession_id);
      pduSession->status = PDU_SESSION_STATUS_TORELEASE;
      pduSession->xid = xid;
    }
  }

  if (found) {
    // TODO RRCReconfiguration To UE
    LOG_I(NR_RRC, "Send RRCReconfiguration To UE \n");
    rrc_gNB_generate_dedicatedRRCReconfiguration_release(rrc, UE, xid, cmd->nas_pdu.len, cmd->nas_pdu.buf);
  } else {
    // gtp tunnel delete
    LOG_I(NR_RRC, "gtp tunnel delete all tunnels for UE %04x\n", UE->rnti);
    gtpv1u_gnb_delete_tunnel_req_t req = {0};
    req.ue_id = UE->rnti;
    gtpv1u_delete_ngu_tunnel(rrc->module_id, &req);
    // NGAP_PDUSESSION_RELEASE_RESPONSE
    rrc_gNB_send_NGAP_PDUSESSION_RELEASE_RESPONSE(rrc, UE, xid);
    LOG_I(NR_RRC, "Send PDU Session Release Response \n");
  }
  return 0;
}

int rrc_gNB_process_PAGING_IND(MessageDef *msg_p, instance_t instance)
{
  ngap_paging_ind_t *msg = &NGAP_PAGING_IND(msg_p);
  for (uint16_t tai_size = 0; tai_size < msg->tai_size; tai_size++) {
    plmn_id_t *p = &msg->plmn_identity[tai_size];
    LOG_I(NR_RRC,
          "[gNB %ld] TAI List for Paging: MCC=%03d, MNC=%0*d, TAC=%d\n",
          instance,
          p->mcc,
          p->mnc_digit_length,
          p->mnc,
          msg->tac[tai_size]);
    gNB_RrcConfigurationReq *req = &RC.nrrrc[instance]->configuration;
    for (uint8_t j = 0; j < req->num_plmn; j++) {
      plmn_id_t *req_plmn = &req->plmn[j];
      if (req_plmn->mcc == p->mcc && req_plmn->mnc == p->mnc && req->tac == msg->tac[tai_size]) {
        for (uint8_t CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {
          AssertFatal(false, "to be implemented properly\n");
          if (NODE_IS_CU(RC.nrrrc[instance]->node_type)) {
            MessageDef *m = itti_alloc_new_message(TASK_RRC_GNB, 0, F1AP_PAGING_IND);
            F1AP_PAGING_IND(m).plmn = *req_plmn;
            F1AP_PAGING_IND (m).nr_cellid        = RC.nrrrc[j]->nr_cellid;
            F1AP_PAGING_IND(m).ueidentityindexvalue = (uint16_t)(msg->ue_paging_identity.s_tmsi.m_tmsi % 1024);
            F1AP_PAGING_IND(m).fiveg_s_tmsi = msg->ue_paging_identity.s_tmsi.m_tmsi;
            F1AP_PAGING_IND(m).paging_drx = msg->paging_drx;
            LOG_E(F1AP, "ueidentityindexvalue %u fiveg_s_tmsi %ld paging_drx %u\n", F1AP_PAGING_IND (m).ueidentityindexvalue, F1AP_PAGING_IND (m).fiveg_s_tmsi, F1AP_PAGING_IND (m).paging_drx);
            itti_send_msg_to_task(TASK_CU_F1, instance, m);
          } else {
            //rrc_gNB_generate_pcch_msg(NGAP_PAGING_IND(msg_p).ue_paging_identity.s_tmsi.m_tmsi,(uint8_t)NGAP_PAGING_IND(msg_p).paging_drx, instance, CC_id);
          } // end of nodetype check
        } // end of cc loop
      } // end of mcc mnc check
    } // end of num_plmn
  } // end of tai size

  return 0;
}
