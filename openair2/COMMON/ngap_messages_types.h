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
 * ngap_messages_types.h
 *
 *  Created on: 2020
 *      Author: Yoshio INOUE, Masayuki HARADA
 *      Email: yoshio.inoue@fujitsu.com,masayuki.harada@fujitsu.com (yoshio.inoue%40fujitsu.com%2cmasayuki.harada%40fujitsu.com)
 */

#ifndef NGAP_MESSAGES_TYPES_H_
#define NGAP_MESSAGES_TYPES_H_
#include "common/5g_platform_types.h"
#include "common/platform_constants.h"
#include "common/platform_types.h"
#include "common/5g_platform_types.h"
#include "s1ap_messages_types.h"
#include "ds/byte_array.h"

// Defines to access message fields.
#define NGAP_REGISTER_GNB_REQ(mSGpTR)           (mSGpTR)->ittiMsg.ngap_register_gnb_req

#define NGAP_REGISTER_GNB_CNF(mSGpTR)           (mSGpTR)->ittiMsg.ngap_register_gnb_cnf
#define NGAP_DEREGISTERED_GNB_IND(mSGpTR)       (mSGpTR)->ittiMsg.ngap_deregistered_gnb_ind

#define NGAP_NAS_FIRST_REQ(mSGpTR)              (mSGpTR)->ittiMsg.ngap_nas_first_req
#define NGAP_UPLINK_NAS(mSGpTR)                 (mSGpTR)->ittiMsg.ngap_uplink_nas
#define NGAP_UE_CAPABILITIES_IND(mSGpTR)        (mSGpTR)->ittiMsg.ngap_ue_cap_info_ind
#define NGAP_INITIAL_CONTEXT_SETUP_RESP(mSGpTR) (mSGpTR)->ittiMsg.ngap_initial_context_setup_resp
#define NGAP_INITIAL_CONTEXT_SETUP_FAIL(mSGpTR) (mSGpTR)->ittiMsg.ngap_initial_context_setup_fail
#define NGAP_UE_CONTEXT_RELEASE_RESP(mSGpTR)    (mSGpTR)->ittiMsg.ngap_ue_release_resp
#define NGAP_NAS_NON_DELIVERY_IND(mSGpTR)       (mSGpTR)->ittiMsg.ngap_nas_non_delivery_ind
#define NGAP_UE_CTXT_MODIFICATION_RESP(mSGpTR)  (mSGpTR)->ittiMsg.ngap_ue_ctxt_modification_resp
#define NGAP_UE_CTXT_MODIFICATION_FAIL(mSGpTR)  (mSGpTR)->ittiMsg.ngap_ue_ctxt_modification_fail
#define NGAP_PDUSESSION_SETUP_RESP(mSGpTR)           (mSGpTR)->ittiMsg.ngap_pdusession_setup_resp
#define NGAP_PDUSESSION_MODIFY_RESP(mSGpTR)           (mSGpTR)->ittiMsg.ngap_pdusession_modify_resp
#define NGAP_PATH_SWITCH_REQ(mSGpTR)            (mSGpTR)->ittiMsg.ngap_path_switch_req
#define NGAP_PATH_SWITCH_REQ_ACK(mSGpTR)        (mSGpTR)->ittiMsg.ngap_path_switch_req_ack
#define NGAP_PDUSESSION_MODIFICATION_IND(mSGpTR)     (mSGpTR)->ittiMsg.ngap_pdusession_modification_ind

#define NGAP_DOWNLINK_NAS(mSGpTR)               (mSGpTR)->ittiMsg.ngap_downlink_nas
#define NGAP_INITIAL_CONTEXT_SETUP_REQ(mSGpTR)  (mSGpTR)->ittiMsg.ngap_initial_context_setup_req
#define NGAP_UE_CTXT_MODIFICATION_REQ(mSGpTR)   (mSGpTR)->ittiMsg.ngap_ue_ctxt_modification_req
#define NGAP_UE_CONTEXT_RELEASE_COMMAND(mSGpTR) (mSGpTR)->ittiMsg.ngap_ue_release_command
#define NGAP_UE_CONTEXT_RELEASE_COMPLETE(mSGpTR) (mSGpTR)->ittiMsg.ngap_ue_release_complete
#define NGAP_PDUSESSION_SETUP_REQ(mSGpTR)              (mSGpTR)->ittiMsg.ngap_pdusession_setup_req
#define NGAP_PDUSESSION_MODIFY_REQ(mSGpTR)              (mSGpTR)->ittiMsg.ngap_pdusession_modify_req
#define NGAP_PAGING_IND(mSGpTR)                 (mSGpTR)->ittiMsg.ngap_paging_ind

#define NGAP_UE_CONTEXT_RELEASE_REQ(mSGpTR)     (mSGpTR)->ittiMsg.ngap_ue_release_req
#define NGAP_PDUSESSION_RELEASE_COMMAND(mSGpTR)      (mSGpTR)->ittiMsg.ngap_pdusession_release_command
#define NGAP_PDUSESSION_RELEASE_RESPONSE(mSGpTR)     (mSGpTR)->ittiMsg.ngap_pdusession_release_resp

//-------------------------------------------------------------------------------------------//

/* Length of the transport layer address string
 * 160 bits / 8 bits by char.
 */
#define NGAP_TRANSPORT_LAYER_ADDRESS_SIZE (160 / 8)

#define NGAP_MAX_NB_AMF_IP_ADDRESS 10
#define NGAP_IMSI_LENGTH           16

#define QOSFLOW_MAX_VALUE           64

#define NGAP_MAX_NO_TAI_PAGING 16 // 9.2.4.1 3GPP TS 38.413

/* Security key length used within gNB
 * Even if only 16 bytes will be effectively used,
 * the key length is 32 bytes (256 bits)
 */
#define SECURITY_KEY_LENGTH 32

typedef enum ngap_paging_drx_e {
  NGAP_PAGING_DRX_32  = 0x0,
  NGAP_PAGING_DRX_64  = 0x1,
  NGAP_PAGING_DRX_128 = 0x2,
  NGAP_PAGING_DRX_256 = 0x3
} ngap_paging_drx_t;

/* Lower value codepoint
 * indicates higher priority.
 */
typedef enum ngap_paging_priority_s {
  NGAP_PAGING_PRIO_LEVEL1  = 0,
  NGAP_PAGING_PRIO_LEVEL2  = 1,
  NGAP_PAGING_PRIO_LEVEL3  = 2,
  NGAP_PAGING_PRIO_LEVEL4  = 3,
  NGAP_PAGING_PRIO_LEVEL5  = 4,
  NGAP_PAGING_PRIO_LEVEL6  = 5,
  NGAP_PAGING_PRIO_LEVEL7  = 6,
  NGAP_PAGING_PRIO_LEVEL8  = 7
} ngap_paging_priority_t;

typedef enum ngap_cn_domain_s {
  NGAP_CN_DOMAIN_PS = 1,
  NGAP_CN_DOMAIN_CS = 2
} ngap_cn_domain_t;

typedef struct ngap_net_ip_address_s {
  unsigned ipv4:1;
  unsigned ipv6:1;
  char ipv4_address[16];
  char ipv6_address[46];
} ngap_net_ip_address_t;

typedef uint64_t bitrate_t;

typedef struct ngap_ambr_s {
  bitrate_t br_ul;
  bitrate_t br_dl;
} ngap_ambr_t;

typedef enum ngap_priority_level_s {
  NGAP_PRIORITY_LEVEL_SPARE       = 0,
  NGAP_PRIORITY_LEVEL_HIGHEST     = 1,
  NGAP_PRIORITY_LEVEL_2           = 2,
  NGAP_PRIORITY_LEVEL_3           = 3,
  NGAP_PRIORITY_LEVEL_4           = 4,
  NGAP_PRIORITY_LEVEL_5           = 5,
  NGAP_PRIORITY_LEVEL_6           = 6,
  NGAP_PRIORITY_LEVEL_7           = 7,
  NGAP_PRIORITY_LEVEL_8           = 8,
  NGAP_PRIORITY_LEVEL_9           = 9,
  NGAP_PRIORITY_LEVEL_10          = 10,
  NGAP_PRIORITY_LEVEL_11          = 11,
  NGAP_PRIORITY_LEVEL_12          = 12,
  NGAP_PRIORITY_LEVEL_13          = 13,
  NGAP_PRIORITY_LEVEL_LOWEST      = 14,
  NGAP_PRIORITY_LEVEL_NO_PRIORITY = 15
} ngap_priority_level_t;

typedef enum ngap_pre_emp_capability_e {
  NGAP_PRE_EMPTION_CAPABILITY_SHALL_NOT_TRIGGER_PREEMPTION = 0,
  NGAP_PRE_EMPTION_CAPABILITY_MAY_TRIGGER_PREEMPTION = 1,
  NGAP_PRE_EMPTION_CAPABILITY_MAX,
} ngap_pre_emp_capability_t;

typedef enum ngap_pre_emp_vulnerability_e {
  NGAP_PRE_EMPTION_VULNERABILITY_NOT_PREEMPTABLE = 0,
  NGAP_PRE_EMPTION_VULNERABILITY_PREEMPTABLE = 1,
  NGAP_PRE_EMPTION_VULNERABILITY_MAX,
} ngap_pre_emp_vulnerability_t;

typedef struct ngap_allocation_retention_priority_s {
  ngap_priority_level_t        priority_level;
  ngap_pre_emp_capability_t    pre_emp_capability;
  ngap_pre_emp_vulnerability_t pre_emp_vulnerability;
} ngap_allocation_retention_priority_t;

typedef struct ngap_security_capabilities_s {
  uint16_t nRencryption_algorithms;
  uint16_t nRintegrity_algorithms;
  uint16_t eUTRAencryption_algorithms;
  uint16_t eUTRAintegrity_algorithms;
} ngap_security_capabilities_t;

/* Provides the establishment cause for the RRC connection request as provided
 * by the upper layers. W.r.t. the cause value names: highPriorityAccess
 * concerns AC11..AC15, ‘mt Estands for ‘Mobile Terminating Eand ‘mo Efor
 * 'Mobile Originating'. Defined in TS 36.331.
 */
typedef enum ngap_rrc_establishment_cause_e {
  NGAP_RRC_CAUSE_EMERGENCY             = 0x0,
  NGAP_RRC_CAUSE_HIGH_PRIO_ACCESS      = 0x1,
  NGAP_RRC_CAUSE_MT_ACCESS             = 0x2,
  NGAP_RRC_CAUSE_MO_SIGNALLING         = 0x3,
  NGAP_RRC_CAUSE_MO_DATA               = 0x4,
  NGAP_RRC_CAUSE_MO_VOICECALL          = 0x5,
  NGAP_RRC_CAUSE_MO_VIDEOCALL          = 0x6,
  NGAP_RRC_CAUSE_MO_SMS                = 0x7,
  NGAP_RRC_CAUSE_MPS_PRIORITY_ACCESS   = 0x8,
  NGAP_RRC_CAUSE_MCS_PRIORITY_ACCESS   = 0x9,
  NGAP_RRC_CAUSE_NOTAVAILABLE          = 0x10,
  NGAP_RRC_CAUSE_LAST
} ngap_rrc_establishment_cause_t;

typedef struct pdusession_level_qos_parameter_s {
  uint8_t qfi;
  uint64_t fiveQI;
  uint64_t qos_priority;
  fiveQI_t fiveQI_type;
  ngap_allocation_retention_priority_t allocation_retention_priority;
} pdusession_level_qos_parameter_t;

typedef struct fiveg_s_tmsi_s {
  uint16_t amf_set_id;
  uint8_t  amf_pointer;
  uint32_t m_tmsi;
} fiveg_s_tmsi_t;

typedef struct ngap_ue_paging_identity_s {
  fiveg_s_tmsi_t s_tmsi;
} ngap_ue_paging_identity_t;

typedef enum ngap_ue_identities_presenceMask_e {
  NGAP_UE_IDENTITIES_FiveG_s_tmsi  = 1 << 1,
  NGAP_UE_IDENTITIES_guami         = 1 << 2,
} ngap_ue_identities_presenceMask_t;

typedef struct ngap_ue_identity_s {
  ngap_ue_identities_presenceMask_t presenceMask;
  fiveg_s_tmsi_t  s_tmsi;
  nr_guami_t guami;
} ngap_ue_identity_t;

typedef struct ngap_mobility_restriction_s{
  plmn_id_t serving_plmn;
}ngap_mobility_restriction_t;

typedef enum pdusession_qosflow_mapping_ind_e{
  QOSFLOW_MAPPING_INDICATION_UL = 0,
  QOSFLOW_MAPPING_INDICATION_DL = 1,
  QOSFLOW_MAPPING_INDICATION_NON = 0xFF
}pdusession_qosflow_mapping_ind_t;

typedef struct pdusession_associate_qosflow_s{
  uint8_t                           qfi;
  pdusession_qosflow_mapping_ind_t  qos_flow_mapping_ind;
} pdusession_associate_qosflow_t;

typedef struct pdusession_setup_s {
  /* Unique pdusession_id for the UE. */
  uint8_t pdusession_id;

  /* The transport layer address for the IP packets */
  uint8_t pdu_session_type;

  // NG-U (N3) Tunnel Endpoint on the RAN side
  gtpu_tunnel_t n3_outgoing;

  /* qos flow list number */
  uint8_t  nb_of_qos_flow;
  
  /* qos flow list(1 ~ 64) */
  pdusession_associate_qosflow_t associated_qos_flows[QOSFLOW_MAX_VALUE];
} pdusession_setup_t;

typedef struct qos_flow_tobe_modified_s {
  uint8_t qfi; // 0~63
} qos_flow_tobe_modified_t;

typedef struct pdusession_modify_s {
  /* Unique pdusession_id for the UE. */
  uint8_t pdusession_id;

  uint8_t nb_of_qos_flow;

  // qos_flow_add_or_modify
  qos_flow_tobe_modified_t qos[QOSFLOW_MAX_VALUE];
} pdusession_modify_t;

/* Cause (9.3.1.2 of 3GPP TS 38.413) */
typedef enum ngap_cause_group_e {
  NGAP_CAUSE_NOTHING, /* No components present */
  NGAP_CAUSE_RADIO_NETWORK,
  NGAP_CAUSE_TRANSPORT,
  NGAP_CAUSE_NAS,
  NGAP_CAUSE_PROTOCOL,
  NGAP_CAUSE_MISC,
} ngap_cause_group_t;

/* Cause (9.3.1.2 of 3GPP TS 38.413) */
typedef struct ngap_cause_s {
  ngap_cause_group_t type;
  uint8_t value;
} ngap_cause_t;

typedef enum ngap_Cause_radio_network_e {
  NGAP_CAUSE_RADIO_NETWORK_UNSPECIFIED,
  NGAP_CAUSE_RADIO_NETWORK_TXNRELOCOVERALL_EXPIRY,
  NGAP_CAUSE_RADIO_NETWORK_SUCCESSFUL_HANDOVER,
  NGAP_CAUSE_RADIO_NETWORK_RELEASE_DUE_TO_NGRAN_GENERATED_REASON,
  NGAP_CAUSE_RADIO_NETWORK_RELEASE_DUE_TO_5GC_GENERATED_REASON,
  NGAP_CAUSE_RADIO_NETWORK_HANDOVER_CANCELLED,
  NGAP_CAUSE_RADIO_NETWORK_PARTIAL_HANDOVER,
  NGAP_CAUSE_RADIO_NETWORK_HO_FAILURE_IN_TARGET_5GC_NGRAN_NODE_OR_TARGET_SYSTEM,
  NGAP_CAUSE_RADIO_NETWORK_HO_TARGET_NOT_ALLOWED,
  NGAP_CAUSE_RADIO_NETWORK_TNGRELOCOVERALL_EXPIRY,
  NGAP_CAUSE_RADIO_NETWORK_TNGRELOCPREP_EXPIRY,
  NGAP_CAUSE_RADIO_NETWORK_CELL_NOT_AVAILABLE,
  NGAP_CAUSE_RADIO_NETWORK_UNKNOWN_TARGETID,
  NGAP_CAUSE_RADIO_NETWORK_NO_RADIO_RESOURCES_AVAILABLE_IN_TARGET_CELL,
  NGAP_CAUSE_RADIO_NETWORK_UNKNOWN_LOCAL_UE_NGAP_ID,
  NGAP_CAUSE_RADIO_NETWORK_INCONSISTENT_REMOTE_UE_NGAP_ID,
  NGAP_CAUSE_RADIO_NETWORK_HANDOVER_DESIRABLE_FOR_RADIO_REASON,
  NGAP_CAUSE_RADIO_NETWORK_TIME_CRITICAL_HANDOVER,
  NGAP_CAUSE_RADIO_NETWORK_RESOURCE_OPTIMISATION_HANDOVER,
  NGAP_CAUSE_RADIO_NETWORK_REDUCE_LOAD_IN_SERVING_CELL,
  NGAP_CAUSE_RADIO_NETWORK_USER_INACTIVITY,
  NGAP_CAUSE_RADIO_NETWORK_RADIO_CONNECTION_WITH_UE_LOST,
  NGAP_CAUSE_RADIO_NETWORK_RADIO_RESOURCES_NOT_AVAILABLE,
  NGAP_CAUSE_RADIO_NETWORK_INVALID_QOS_COMBINATION,
  NGAP_CAUSE_RADIO_NETWORK_FAILURE_IN_RADIO_INTERFACE_PROCEDURE,
  NGAP_CAUSE_RADIO_NETWORK_INTERACTION_WITH_OTHER_PROCEDURE,
  NGAP_CAUSE_RADIO_NETWORK_UNKNOWN_PDU_SESSION_ID,
  NGAP_CAUSE_RADIO_NETWORK_UNKNOWN_QOS_FLOW_ID,
  NGAP_CAUSE_RADIO_NETWORK_MULTIPLE_PDU_SESSION_ID_INSTANCES,
  NGAP_CAUSE_RADIO_NETWORK_MULTIPLE_QOS_FLOW_ID_INSTANCES,
  NGAP_CAUSE_RADIO_NETWORK_ENCRYPTION_AND_OR_INTEGRITY_PROTECTION_ALGORITHMS_NOT_SUPPORTED,
  NGAP_CAUSE_RADIO_NETWORK_NG_INTRA_SYSTEM_HANDOVER_TRIGGERED,
  NGAP_CAUSE_RADIO_NETWORK_NG_INTER_SYSTEM_HANDOVER_TRIGGERED,
  NGAP_CAUSE_RADIO_NETWORK_XN_HANDOVER_TRIGGERED,
  NGAP_CAUSE_RADIO_NETWORK_NOT_SUPPORTED_5QI_VALUE,
  NGAP_CAUSE_RADIO_NETWORK_UE_CONTEXT_TRANSFER,
  NGAP_CAUSE_RADIO_NETWORK_IMS_VOICE_EPS_FALLBACK_OR_RAT_FALLBACK_TRIGGERED,
  NGAP_CAUSE_RADIO_NETWORK_UP_INTEGRITY_PROTECTION_NOT_POSSIBLE,
  NGAP_CAUSE_RADIO_NETWORK_UP_CONFIDENTIALITY_PROTECTION_NOT_POSSIBLE,
  NGAP_CAUSE_RADIO_NETWORK_SLICE_NOT_SUPPORTED,
  NGAP_CAUSE_RADIO_NETWORK_UE_IN_RRC_INACTIVE_STATE_NOT_REACHABLE,
  NGAP_CAUSE_RADIO_NETWORK_REDIRECTION,
  NGAP_CAUSE_RADIO_NETWORK_RESOURCES_NOT_AVAILABLE_FOR_THE_SLICE,
  NGAP_CAUSE_RADIO_NETWORK_UE_MAX_INTEGRITY_PROTECTED_DATA_RATE_REASON,
  NGAP_CAUSE_RADIO_NETWORK_RELEASE_DUE_TO_CN_DETECTED_MOBILITY,
  NGAP_CAUSE_RADIO_NETWORK_N26_INTERFACE_NOT_AVAILABLE,
  NGAP_CAUSE_RADIO_NETWORK_RELEASE_DUE_TO_PRE_EMPTION,
  NGAP_CAUSE_RADIO_NETWORK_MULTIPLE_LOCATION_REPORTING_REFERENCE_ID_INSTANCES,
  NGAP_CAUSE_RADIO_NETWORK_RSN_NOT_AVAILABLE_FOR_THE_UP,
  NGAP_CAUSE_RADIO_NETWORK_NPN_ACCESS_DENIED,
  NGAP_CAUSE_RADIO_NETWORK_CAG_ONLY_ACCESS_DENIED,
  NGAP_CAUSE_RADIO_NETWORK_INSUFFICIENT_UE_CAPABILITIES
} ngap_cause_radio_network_t;

/**
 * NGAP protocol cause values as per 9.3.1.2 `Cause` section in 3GPP TS 38.413.
 */
typedef enum ngap_cause_protocol_e {
  NGAP_CAUSE_PROTOCOL_TRANSFER_SYNTAX_ERROR,
  NGAP_CAUSE_PROTOCOL_ABSTRACT_SYNTAX_ERROR_REJECT,
  NGAP_CAUSE_PROTOCOL_ABSTRACT_SYNTAX_ERROR_IGNORE,
  NGAP_CAUSE_PROTOCOL_MSG_NOT_COMPATIBLE_WITH_RECEIVER_STATE,
  NGAP_CAUSE_PROTOCOL_SEMANTIC_ERROR,
  NGAP_CAUSE_PROTOCOL_ABSTRACT_SYNTAX_ERROR_FCM,
  NGAP_CAUSE_PROTOCOL_UNSPECIFIED
} ngap_cause_protocol_t;

typedef struct pdusession_failed_s {
  /* Unique pdusession_id for the UE. */
  uint8_t pdusession_id;
  /* Cause of the failure */
  ngap_cause_t cause;
} pdusession_failed_t;

typedef enum ngap_ue_ctxt_modification_present_s {
  NGAP_UE_CONTEXT_MODIFICATION_SECURITY_KEY = (1 << 0),
  NGAP_UE_CONTEXT_MODIFICATION_UE_AMBR      = (1 << 1),
  NGAP_UE_CONTEXT_MODIFICATION_UE_SECU_CAP  = (1 << 2),
} ngap_ue_ctxt_modification_present_t;

typedef enum ngap_paging_ind_present_s {
  NGAP_PAGING_IND_PAGING_DRX      = (1 << 0),
  NGAP_PAGING_IND_PAGING_PRIORITY = (1 << 1),
} ngap_paging_ind_present_t;

typedef struct {
  plmn_id_t plmn;
  uint16_t num_nssai;
  nssai_t s_nssai[8];
} ngap_plmn_t;

//-------------------------------------------------------------------------------------------//
// gNB application layer -> NGAP messages
typedef struct ngap_register_gnb_req_s {
  /* Unique gNB_id to identify the gNB within EPC.
   * For macro gNB ids this field should be 20 bits long.
   * For home gNB ids this field should be 28 bits long.
   */
  uint32_t gNB_id;
  /* The type of the cell */
  enum cell_type_e cell_type;

  /* Optional name for the cell
   * NOTE: the name can be NULL (i.e no name) and will be cropped to 150
   * characters.
   */
  char *gNB_name;

  /* Tracking area code */
  uint32_t tac;

#define PLMN_LIST_MAX_SIZE 6
  /* Mobile Country Code
   * Mobile Network Code
   */
  uint8_t  num_plmn;
  ngap_plmn_t plmn[PLMN_LIST_MAX_SIZE];

  /* Default Paging DRX of the gNB as defined in TS 38.304 */
  ngap_paging_drx_t default_drx;

  /* The gNB IP address to bind */
  net_ip_address_t gnb_ip_address;

  /* Nb of AMF to connect to */
  uint8_t          nb_amf;
  /* List of AMF to connect to */
  net_ip_address_t amf_ip_address[NGAP_MAX_NB_AMF_IP_ADDRESS];
  uint8_t          broadcast_plmn_num[NGAP_MAX_NB_AMF_IP_ADDRESS];
  uint8_t          broadcast_plmn_index[NGAP_MAX_NB_AMF_IP_ADDRESS][PLMN_LIST_MAX_SIZE];

  /* Number of SCTP streams used for a amf association */
  uint16_t sctp_in_streams;
  uint16_t sctp_out_streams;
} ngap_register_gnb_req_t;

//-------------------------------------------------------------------------------------------//
// NGAP -> gNB application layer messages
typedef struct ngap_register_gnb_cnf_s {
  /* Nb of AMF connected */
  uint8_t          nb_amf;
} ngap_register_gnb_cnf_t;

typedef struct ngap_deregistered_gnb_ind_s {
  /* Nb of AMF connected */
  uint8_t          nb_amf;
} ngap_deregistered_gnb_ind_t;

//-------------------------------------------------------------------------------------------//
// RRC -> NGAP messages

/* The NAS First Req is the first message exchanged between RRC and NGAP
 * for an UE.
 * The rnti uniquely identifies an UE within a cell. Later the gnb_ue_ngap_id
 * will be the unique identifier used between RRC and NGAP.
 */
typedef struct ngap_nas_first_req_s {
  // RAN UE NGAP ID (mandatory)
  uint32_t gNB_ue_ngap_id;
  /* PLMN: Selected PLMN Identity (optional)
   * User Location Information (mandatory) */
  plmn_id_t plmn;
  // RRC Establishment Cause (mandatory)
  ngap_rrc_establishment_cause_t establishment_cause;
  // NAS-PDU (mandatory)
  byte_array_t nas_pdu;
  // UE identity: 5G-S-TMSI, GUAMI
  ngap_ue_identity_t ue_identity;
} ngap_nas_first_req_t;

typedef struct ngap_uplink_nas_s {
  /* Unique UE identifier within an gNB */
  uint32_t gNB_ue_ngap_id;
  /* NAS pdu */
  byte_array_t nas_pdu;
} ngap_uplink_nas_t;

typedef struct ngap_ue_cap_info_ind_s {
  uint32_t  gNB_ue_ngap_id;
  byte_array_t ue_radio_cap;
} ngap_ue_cap_info_ind_t;

typedef struct ngap_initial_context_setup_resp_s {
  uint32_t  gNB_ue_ngap_id;

  /* Number of pdusession setup-ed in the list */
  uint8_t       nb_of_pdusessions;
  /* list of pdusession setup-ed by RRC layers */
  pdusession_setup_t pdusessions[NGAP_MAX_PDU_SESSION];

  /* Number of pdusession failed to be setup in list */
  uint8_t        nb_of_pdusessions_failed;
  /* list of pdusessions that failed to be setup */
  pdusession_failed_t pdusessions_failed[NGAP_MAX_PDU_SESSION];
} ngap_initial_context_setup_resp_t;

typedef struct ngap_initial_context_setup_fail_s {
  uint32_t gNB_ue_ngap_id;

  uint64_t amf_ue_ngap_id;

  ngap_cause_t cause;
} ngap_initial_context_setup_fail_t, ngap_ue_ctxt_modification_fail_t;

typedef struct ngap_nas_non_delivery_ind_s {
  uint32_t     gNB_ue_ngap_id;
  byte_array_t nas_pdu;
  /* TODO: add cause */
} ngap_nas_non_delivery_ind_t;

typedef struct ngap_ue_ctxt_modification_req_s {
  uint32_t  gNB_ue_ngap_id;

  /* Bit-mask of possible present parameters */
  ngap_ue_ctxt_modification_present_t present;

  /* Following fields are optionnaly present */

  /* Security key */
  uint8_t security_key[SECURITY_KEY_LENGTH];

  /* UE aggregate maximum bitrate */
  ngap_ambr_t ue_ambr;

  /* NR Security capabilities */
  ngap_security_capabilities_t security_capabilities;
} ngap_ue_ctxt_modification_req_t;

typedef struct ngap_ue_ctxt_modification_resp_s {
  uint32_t  gNB_ue_ngap_id;
} ngap_ue_ctxt_modification_resp_t;

typedef struct ngap_ue_release_complete_s {
  uint32_t gNB_ue_ngap_id;
  int num_pdu_sessions;
  uint32_t pdu_session_id[256];
} ngap_ue_release_complete_t;

//-------------------------------------------------------------------------------------------//
// NGAP -> RRC messages
typedef struct ngap_downlink_nas_s {
  /* UE id for initial connection to NGAP */
  uint32_t gNB_ue_ngap_id;
  /* NAS pdu */
  byte_array_t nas_pdu;
} ngap_downlink_nas_t;

/* PDU Session Resource Setup Request Transfer (9.3.4.1 3GPP TS 38.413) */
typedef struct {
  uint8_t nb_qos;
  pdusession_level_qos_parameter_t qos[QOSFLOW_MAX_VALUE];
  pdu_session_type_t pdu_session_type;
  // UPF endpoint of the NG-U (N3) transport bearer
  gtpu_tunnel_t n3_incoming;
} pdusession_transfer_t;

/* PDU Session Resource Setup/Modify Request Item */
typedef struct {
  int pdusession_id;
  byte_array_t nas_pdu;
  nssai_t nssai;
  pdusession_transfer_t pdusessionTransfer;
} pdusession_resource_item_t;

typedef struct ngap_initial_context_setup_req_s {
  /* UE id for initial connection to NGAP */
  uint32_t gNB_ue_ngap_id;

  uint64_t amf_ue_ngap_id;

  /* UE aggregate maximum bitrate */
  bool has_ue_ambr;
  ngap_ambr_t ue_ambr;

  /* guami */
  nr_guami_t guami;

  /* allowed nssai */
  uint8_t nb_allowed_nssais;
  nssai_t allowed_nssai[8];

  /* Security algorithms */
  ngap_security_capabilities_t security_capabilities;

  /* Security key */
  uint8_t security_key[SECURITY_KEY_LENGTH];

  /* Number of pdusession to be setup in the list */
  uint8_t  nb_of_pdusessions;
  // PDU Session Resource Setup Request List
  pdusession_resource_item_t pdusession[NGAP_MAX_PDU_SESSION];

  /* Mobility Restriction List */
  uint8_t                        mobility_restriction_flag;
  ngap_mobility_restriction_t    mobility_restriction;

  /* Nas Pdu */
  uint8_t                        nas_pdu_flag;
  byte_array_t nas_pdu;
} ngap_initial_context_setup_req_t;


typedef struct ngap_paging_ind_s {
  /* UE paging identity */
  ngap_ue_paging_identity_t ue_paging_identity;

  /* Indicates origin of paging */
  ngap_cn_domain_t cn_domain;

  /* PLMN_identity in TAI of Paging*/
  plmn_id_t plmn_identity[NGAP_MAX_NO_TAI_PAGING];

  /* TAC in TAIList of Paging*/
  int16_t tac[NGAP_MAX_NO_TAI_PAGING];

  /* size of TAIList*/
  int16_t tai_size;

  /* Optional fields */
  ngap_paging_drx_t paging_drx;

  ngap_paging_priority_t paging_priority;
} ngap_paging_ind_t;

typedef struct ngap_pdusession_setup_req_s {
  /* UE id for initial connection to NGAP */
  uint32_t gNB_ue_ngap_id;

  /* AMF UE id  */
  uint64_t amf_ue_ngap_id;

  /* S-NSSAI */
  // Fixme: illogical, nssai is part of each pdu session
  nssai_t allowed_nssai[8];

  /* Number of pdusession to be setup in the list */
  uint8_t nb_pdusessions_tosetup;

  // PDU Session Resource Setup Request List
  pdusession_resource_item_t pdusession[NGAP_MAX_PDU_SESSION];

  /* UE Aggregated Max Bitrates */
  bool has_ue_ambr;
  ngap_ambr_t ueAggMaxBitRate;

} ngap_pdusession_setup_req_t;

typedef struct ngap_pdusession_setup_resp_s {
  uint32_t gNB_ue_ngap_id;
  /* Number of pdusession setup-ed in the list */
  uint8_t       nb_of_pdusessions;
  /* list of pdusession setup-ed by RRC layers */
  pdusession_setup_t pdusessions[NGAP_MAX_PDU_SESSION];

  /* Number of pdusession failed to be setup in list */
  uint8_t        nb_of_pdusessions_failed;
  /* list of pdusessions that failed to be setup */
  pdusession_failed_t pdusessions_failed[NGAP_MAX_PDU_SESSION];
} ngap_pdusession_setup_resp_t;

// NGAP --> RRC messages
typedef struct ngap_ue_release_command_s {

  uint32_t  gNB_ue_ngap_id;

} ngap_ue_release_command_t;


//-------------------------------------------------------------------------------------------//
// NGAP <-- RRC messages
typedef struct pdusession_release_s {
  /* Unique pdusession_id for the UE. */
  uint8_t                     pdusession_id;
  byte_array_t data;
} pdusession_release_t;

typedef struct ngap_ue_release_req_s {
  uint32_t             gNB_ue_ngap_id;
  /* Number of pdusession resource in the list */
  uint8_t              nb_of_pdusessions;
  /* list of pdusession resource by RRC layers */
  pdusession_release_t pdusessions[NGAP_MAX_PDU_SESSION];
  ngap_cause_t cause;
} ngap_ue_release_req_t, ngap_ue_release_resp_t;

typedef struct ngap_pdusession_modify_req_s {
  /* AMF UE id  */
  uint64_t amf_ue_ngap_id;

  /* gNB ue ngap id as initialized by NGAP layer */
  uint32_t  gNB_ue_ngap_id;

  /* Number of pdusession to be modify in the list */
  uint8_t nb_pdusessions_tomodify;

  // PDU Session Resource Modify Request List
  pdusession_resource_item_t pdusession[NGAP_MAX_PDU_SESSION];
} ngap_pdusession_modify_req_t;

typedef struct ngap_pdusession_modify_resp_s {
  uint32_t  gNB_ue_ngap_id;

  /* Number of pdusession modify-ed in the list */
  uint8_t       nb_of_pdusessions;
  /* list of pdusession modify-ed by RRC layers */
  pdusession_modify_t pdusessions[NGAP_MAX_PDU_SESSION];

  /* Number of pdusession failed to be modify in list */
  uint8_t        nb_of_pdusessions_failed;
  /* list of pdusessions that failed to be modify */
  pdusession_failed_t pdusessions_failed[NGAP_MAX_PDU_SESSION];
} ngap_pdusession_modify_resp_t;

typedef struct ngap_pdusession_release_command_s {
  /* AMF UE id  */
  uint64_t amf_ue_ngap_id;

  /* gNB ue ngap id as initialized by NGAP layer */
  uint32_t                       gNB_ue_ngap_id;

  /* The NAS PDU should be forwarded by the RRC layer to the NAS layer */
  byte_array_t nas_pdu;

  /* Number of pdusession to be released in the list */
  uint8_t                        nb_pdusessions_torelease;

  /* PDUSession release command */
  pdusession_release_t pdusession_release_params[NGAP_MAX_PDU_SESSION];

} ngap_pdusession_release_command_t;

typedef struct ngap_pdusession_release_resp_s {
  /* AMF UE id  */
  uint64_t amf_ue_ngap_id;

  /* gNB ue ngap id as initialized by NGAP layer */
  uint32_t             gNB_ue_ngap_id;

  /* Number of pdusession released in the list */
  uint8_t              nb_of_pdusessions_released;

  /* list of pdusessions released */
  pdusession_release_t pdusession_release[NGAP_MAX_PDU_SESSION];

  /* Number of pdusession failed to be released in list */
  uint8_t              nb_of_pdusessions_failed;
  /* list of pdusessions that failed to be released */
  pdusession_failed_t  pdusessions_failed[NGAP_MAX_PDU_SESSION];

} ngap_pdusession_release_resp_t;

#endif /* NGAP_MESSAGES_TYPES_H_ */
