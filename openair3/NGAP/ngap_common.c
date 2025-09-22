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

/*! \file ngap_common.c
 * \brief ngap procedures for both gNB and AMF
 * \author Yoshio INOUE, Masayuki HARADA
 * \email yoshio.inoue@fujitsu.com,masayuki.harada@fujitsu.com (yoshio.inoue%40fujitsu.com%2cmasayuki.harada%40fujitsu.com)
 * \date 2020
 * \version 0.1
 */

#include <stdint.h>
#include "conversions.h"
#include "ngap_common.h"

void encode_ngap_cause(NGAP_Cause_t *out, const ngap_cause_t *in)
{
  switch (in->type) {
    case NGAP_CAUSE_RADIO_NETWORK:
      out->present = NGAP_Cause_PR_radioNetwork;
      out->choice.radioNetwork = in->value;
      break;

    case NGAP_CAUSE_TRANSPORT:
      out->present = NGAP_Cause_PR_transport;
      out->choice.transport = in->value;
      break;

    case NGAP_CAUSE_NAS:
      out->present = NGAP_Cause_PR_nas;
      out->choice.nas = in->value;
      break;

    case NGAP_CAUSE_PROTOCOL:
      out->present = NGAP_Cause_PR_protocol;
      out->choice.protocol = in->value;
      break;

    case NGAP_CAUSE_MISC:
      out->present = NGAP_Cause_PR_misc;
      out->choice.misc = in->value;
      break;

    case NGAP_CAUSE_NOTHING:
    default:
      AssertFatal(false, "Unknown failure cause %d\n", in->type);
      break;
  }
}

ngap_cause_t decode_ngap_cause(const NGAP_Cause_t *in)
{
  ngap_cause_t out = {0};
  switch (in->present) {
    case NGAP_Cause_PR_radioNetwork:
      out.type = NGAP_CAUSE_RADIO_NETWORK;
      out.value = in->choice.radioNetwork;
      break;

    case NGAP_Cause_PR_transport:
      out.type = NGAP_CAUSE_TRANSPORT;
      out.value = in->choice.transport;
      break;

    case NGAP_Cause_PR_nas:
      out.type = NGAP_CAUSE_NAS;
      out.value = in->choice.nas;
      break;

    case NGAP_Cause_PR_protocol:
      out.type = NGAP_CAUSE_PROTOCOL;
      out.value = in->choice.protocol;
      break;

    case NGAP_Cause_PR_misc:
      out.type = NGAP_CAUSE_MISC;
      out.value = in->choice.misc;
      break;

    default:
      out.type = NGAP_CAUSE_NOTHING;
      NGAP_ERROR("Unknown failure cause %d\n", in->present);
      break;
  }
  return out;
}

nr_guami_t decode_ngap_guami(const NGAP_GUAMI_t *in)
{
  nr_guami_t out = {0};
  TBCD_TO_MCC_MNC(&in->pLMNIdentity, out.mcc, out.mnc, out.mnc_len);
  OCTET_STRING_TO_INT8(&in->aMFRegionID, out.amf_region_id);
  OCTET_STRING_TO_INT16(&in->aMFSetID, out.amf_set_id);
  OCTET_STRING_TO_INT8(&in->aMFPointer, out.amf_pointer);
  return out;
}

ngap_ambr_t decode_ngap_UEAggregateMaximumBitRate(const NGAP_UEAggregateMaximumBitRate_t *in)
{
  ngap_ambr_t ambr = {0};
  asn_INTEGER2ulong(&in->uEAggregateMaximumBitRateUL, &ambr.br_ul);
  asn_INTEGER2ulong(&in->uEAggregateMaximumBitRateDL, &ambr.br_dl);
  return ambr;
}

nssai_t decode_ngap_nssai(const NGAP_S_NSSAI_t *in)
{
  nssai_t nssai = {0};
  OCTET_STRING_TO_INT8(&in->sST, nssai.sst);
  if (in->sD != NULL) {
    BUFFER_TO_INT24(in->sD->buf, nssai.sd);
  } else {
    nssai.sd = 0xffffff;
  }
  return nssai;
}

ngap_security_capabilities_t decode_ngap_security_capabilities(const NGAP_UESecurityCapabilities_t *in)
{
  ngap_security_capabilities_t out = {0};
  out.nRencryption_algorithms = BIT_STRING_to_uint16(&in->nRencryptionAlgorithms);
  out.nRintegrity_algorithms = BIT_STRING_to_uint16(&in->nRintegrityProtectionAlgorithms);
  out.eUTRAencryption_algorithms = BIT_STRING_to_uint16(&in->eUTRAencryptionAlgorithms);
  out.eUTRAintegrity_algorithms = BIT_STRING_to_uint16(&in->eUTRAintegrityProtectionAlgorithms);
  return out;
}

ngap_mobility_restriction_t decode_ngap_mobility_restriction(const NGAP_MobilityRestrictionList_t *in)
{
  ngap_mobility_restriction_t out = {0};
  TBCD_TO_MCC_MNC(&in->servingPLMN, out.serving_plmn.mcc, out.serving_plmn.mnc, out.serving_plmn.mnc_digit_length);
  return out;
}

void encode_ngap_target_id(NGAP_HandoverRequiredIEs_t *out, const target_ran_node_id_t *in)
{
  out->id = NGAP_ProtocolIE_ID_id_TargetID;
  out->criticality = NGAP_Criticality_reject;
  out->value.present = NGAP_HandoverRequiredIEs__value_PR_TargetID;
  // CHOICE Target ID: NG-RAN (M)
  out->value.choice.TargetID.present = NGAP_TargetID_PR_targetRANNodeID;
  asn1cCalloc(out->value.choice.TargetID.choice.targetRANNodeID, targetRan);
  // Global RAN Node ID (M)
  targetRan->globalRANNodeID.present = NGAP_GlobalRANNodeID_PR_globalGNB_ID;
  asn1cCalloc(targetRan->globalRANNodeID.choice.globalGNB_ID, globalGnbId);
  globalGnbId->gNB_ID.present = NGAP_GNB_ID_PR_gNB_ID;
  MACRO_GNB_ID_TO_BIT_STRING(in->targetgNBId, &globalGnbId->gNB_ID.choice.gNB_ID);
  MCC_MNC_TO_PLMNID(in->plmn_identity.mcc, in->plmn_identity.mnc, in->plmn_identity.mnc_digit_length, &globalGnbId->pLMNIdentity);
  // Selected TAI (M)
  INT24_TO_OCTET_STRING(in->tac, &targetRan->selectedTAI.tAC);
  MCC_MNC_TO_PLMNID(in->plmn_identity.mcc,
                    in->plmn_identity.mnc,
                    in->plmn_identity.mnc_digit_length,
                    &targetRan->selectedTAI.pLMNIdentity);
}

void encode_ngap_nr_cgi(NGAP_NR_CGI_t *out, const plmn_id_t *plmn, const uint32_t cell_id)
{
  out->iE_Extensions = NULL;
  MCC_MNC_TO_PLMNID(plmn->mcc, plmn->mnc, plmn->mnc_digit_length, &out->pLMNIdentity);
  NR_CELL_ID_TO_BIT_STRING(cell_id, &out->nRCellIdentity);
}

pdusession_level_qos_parameter_t fill_qos(uint8_t qfi, const NGAP_QosFlowLevelQosParameters_t *params)
{
  pdusession_level_qos_parameter_t out = {0};
  // QFI
  out.qfi = qfi;
  AssertFatal(params != NULL, "QoS parameters are NULL\n");
  // QosCharacteristics
  const NGAP_QosCharacteristics_t *qosChar = &params->qosCharacteristics;
  AssertFatal(qosChar != NULL, "QoS characteristics are NULL\n");
  if (qosChar->present == NGAP_QosCharacteristics_PR_nonDynamic5QI) {
    AssertFatal(qosChar->choice.nonDynamic5QI != NULL, "nonDynamic5QI is NULL\n");
    out.fiveQI_type = NON_DYNAMIC;
    out.fiveQI = qosChar->choice.nonDynamic5QI->fiveQI;
  } else if (qosChar->present == NGAP_QosCharacteristics_PR_dynamic5QI) {
    AssertFatal(qosChar->choice.dynamic5QI != NULL, "dynamic5QI is NULL\n");
    out.fiveQI_type = DYNAMIC;
    out.fiveQI = *qosChar->choice.dynamic5QI->fiveQI;
  } else {
    AssertFatal(0, "Unsupported QoS Characteristics present value: %d\n", qosChar->present);
  }
  // Allocation and Retention Priority
  const NGAP_AllocationAndRetentionPriority_t *arp = &params->allocationAndRetentionPriority;
  out.allocation_retention_priority.priority_level = arp->priorityLevelARP;
  out.allocation_retention_priority.pre_emp_capability = arp->pre_emptionCapability;
  out.allocation_retention_priority.pre_emp_vulnerability = arp->pre_emptionVulnerability;

  return out;
}

static gtpu_tunnel_t decode_TNLInformation(const NGAP_GTPTunnel_t *gTPTunnel_p)
{
  gtpu_tunnel_t out = {0};
  // Transport layer address
  memcpy(out.addr.buffer, gTPTunnel_p->transportLayerAddress.buf, gTPTunnel_p->transportLayerAddress.size);
  out.addr.length = gTPTunnel_p->transportLayerAddress.size - gTPTunnel_p->transportLayerAddress.bits_unused;
  // GTP tunnel endpoint ID
  OCTET_STRING_TO_INT32(&gTPTunnel_p->gTP_TEID, out.teid);
  return out;
}

void *decode_pdusession_transfer(const asn_TYPE_descriptor_t *td, const OCTET_STRING_t buf)
{
  void *decoded = NULL;
  asn_codec_ctx_t ctx = {.max_stack_size = 100 * 1000};
  asn_dec_rval_t rval = aper_decode(&ctx, td, &decoded, buf.buf, buf.size, 0, 0);
  if (rval.code != RC_OK) {
    NGAP_ERROR("Decoding failed for %s\n", td->name);
    return NULL;
  }
  return decoded;
}

/** @brief Decode PDU Session Resource Setup Request Transfer (9.3.4.1 3GPP TS 38.413) */
bool decodePDUSessionResourceSetup(pdusession_transfer_t *out, const OCTET_STRING_t in)
{
  void *decoded = decode_pdusession_transfer(&asn_DEF_NGAP_PDUSessionResourceSetupRequestTransfer, in);
  if (!decoded) {
    LOG_E(NR_RRC, "Failed to decode PDUSessionResourceSetupRequestTransfer\n");
    return false;
  }

  NGAP_PDUSessionResourceSetupRequestTransfer_t *pdusessionTransfer = (NGAP_PDUSessionResourceSetupRequestTransfer_t *)decoded;
  for (int i = 0; i < pdusessionTransfer->protocolIEs.list.count; i++) {
    NGAP_PDUSessionResourceSetupRequestTransferIEs_t *pdusessionTransfer_ies = pdusessionTransfer->protocolIEs.list.array[i];
    switch (pdusessionTransfer_ies->id) {
      // UL NG-U UP TNL Information (Mandatory)
      case NGAP_ProtocolIE_ID_id_UL_NGU_UP_TNLInformation:
        out->n3_incoming = decode_TNLInformation(pdusessionTransfer_ies->value.choice.UPTransportLayerInformation.choice.gTPTunnel);
        break;

      // PDU Session Type (Mandatory)
      case NGAP_ProtocolIE_ID_id_PDUSessionType:
        out->pdu_session_type = pdusessionTransfer_ies->value.choice.PDUSessionType;
        break;

      // QoS Flow Setup Request List (Mandatory)
      case NGAP_ProtocolIE_ID_id_QosFlowSetupRequestList:
        out->nb_qos = pdusessionTransfer_ies->value.choice.QosFlowSetupRequestList.list.count;
        for (int i = 0; i < out->nb_qos; i++) {
          NGAP_QosFlowSetupRequestItem_t *item = pdusessionTransfer_ies->value.choice.QosFlowSetupRequestList.list.array[i];
          out->qos[i] = fill_qos(item->qosFlowIdentifier, &item->qosFlowLevelQosParameters);
        }
        break;

      default:
        LOG_D(NR_RRC, "Unhandled optional IE %ld\n", pdusessionTransfer_ies->id);
    }
  }
  ASN_STRUCT_FREE(asn_DEF_NGAP_PDUSessionResourceSetupRequestTransfer, pdusessionTransfer);

  return true;
}
