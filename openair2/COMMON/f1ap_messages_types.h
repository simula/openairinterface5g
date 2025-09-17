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

#ifndef F1AP_MESSAGES_TYPES_H_
#define F1AP_MESSAGES_TYPES_H_

#include <netinet/in.h>
#include <netinet/sctp.h>
#include "common/5g_platform_types.h"

//-------------------------------------------------------------------------------------------//
// Defines to access message fields.

#define F1AP_CU_SCTP_REQ(mSGpTR)                   (mSGpTR)->ittiMsg.f1ap_cu_sctp_req

#define F1AP_DU_REGISTER_REQ(mSGpTR)               (mSGpTR)->ittiMsg.f1ap_du_register_req

#define F1AP_RESET(mSGpTR)                         (mSGpTR)->ittiMsg.f1ap_reset
#define F1AP_RESET_ACK(mSGpTR)                         (mSGpTR)->ittiMsg.f1ap_reset_ack

#define F1AP_SETUP_REQ(mSGpTR)                     (mSGpTR)->ittiMsg.f1ap_setup_req
#define F1AP_SETUP_RESP(mSGpTR)                    (mSGpTR)->ittiMsg.f1ap_setup_resp
#define F1AP_GNB_CU_CONFIGURATION_UPDATE(mSGpTR)   (mSGpTR)->ittiMsg.f1ap_gnb_cu_configuration_update
#define F1AP_GNB_CU_CONFIGURATION_UPDATE_ACKNOWLEDGE(mSGpTR)   (mSGpTR)->ittiMsg.f1ap_gnb_cu_configuration_update_acknowledge
#define F1AP_GNB_CU_CONFIGURATION_UPDATE_FAILURE(mSGpTR)   (mSGpTR)->ittiMsg.f1ap_gnb_cu_configuration_update_failure
#define F1AP_GNB_DU_CONFIGURATION_UPDATE(mSGpTR)   (mSGpTR)->ittiMsg.f1ap_gnb_du_configuration_update
#define F1AP_GNB_DU_CONFIGURATION_UPDATE_ACKNOWLEDGE(mSGpTR)   (mSGpTR)->ittiMsg.f1ap_gnb_du_configuration_update_acknowledge
#define F1AP_GNB_DU_CONFIGURATION_UPDATE_FAILURE(mSGpTR)   (mSGpTR)->ittiMsg.f1ap_gnb_du_configuration_update_failure

#define F1AP_SETUP_FAILURE(mSGpTR)                 (mSGpTR)->ittiMsg.f1ap_setup_failure

#define F1AP_LOST_CONNECTION(mSGpTR)   (mSGpTR)->ittiMsg.f1ap_lost_connection

#define F1AP_INITIAL_UL_RRC_MESSAGE(mSGpTR)        (mSGpTR)->ittiMsg.f1ap_initial_ul_rrc_message
#define F1AP_UL_RRC_MESSAGE(mSGpTR)                (mSGpTR)->ittiMsg.f1ap_ul_rrc_message
#define F1AP_UE_CONTEXT_SETUP_REQ(mSGpTR)          (mSGpTR)->ittiMsg.f1ap_ue_context_setup_req
#define F1AP_UE_CONTEXT_SETUP_RESP(mSGpTR)         (mSGpTR)->ittiMsg.f1ap_ue_context_setup_resp
#define F1AP_UE_CONTEXT_MODIFICATION_REQ(mSGpTR)   (mSGpTR)->ittiMsg.f1ap_ue_context_modification_req
#define F1AP_UE_CONTEXT_MODIFICATION_RESP(mSGpTR)  (mSGpTR)->ittiMsg.f1ap_ue_context_modification_resp
#define F1AP_UE_CONTEXT_MODIFICATION_FAIL(mSGpTR)  (mSGpTR)->ittiMsg.f1ap_ue_context_modification_fail
#define F1AP_UE_CONTEXT_MODIFICATION_REQUIRED(mSGpTR)   (mSGpTR)->ittiMsg.f1ap_ue_context_modification_required
#define F1AP_UE_CONTEXT_MODIFICATION_CONFIRM(mSGpTR)  (mSGpTR)->ittiMsg.f1ap_ue_context_modification_confirm
#define F1AP_UE_CONTEXT_MODIFICATION_REFUSE(mSGpTR)  (mSGpTR)->ittiMsg.f1ap_ue_context_modification_refuse

#define F1AP_DL_RRC_MESSAGE(mSGpTR)                (mSGpTR)->ittiMsg.f1ap_dl_rrc_message
#define F1AP_UE_CONTEXT_RELEASE_REQ(mSGpTR)        (mSGpTR)->ittiMsg.f1ap_ue_context_release_req
#define F1AP_UE_CONTEXT_RELEASE_CMD(mSGpTR)        (mSGpTR)->ittiMsg.f1ap_ue_context_release_cmd
#define F1AP_UE_CONTEXT_RELEASE_COMPLETE(mSGpTR)   (mSGpTR)->ittiMsg.f1ap_ue_context_release_complete

#define F1AP_PAGING_IND(mSGpTR)                    (mSGpTR)->ittiMsg.f1ap_paging_ind

/* Length of the transport layer address string
 * 160 bits / 8 bits by char.
 */
#define F1AP_TRANSPORT_LAYER_ADDRESS_SIZE (160 / 8)

#define F1AP_MAX_NB_CU_IP_ADDRESS 10

// Note this should be 512 from maxval in 38.473
#define F1AP_MAX_NB_CELLS 2

#define F1AP_MAX_NO_OF_TNL_ASSOCIATIONS 32
#define F1AP_MAX_NO_UE_ID 1024

#define F1AP_MAX_NO_OF_INDIVIDUAL_CONNECTIONS_TO_RESET 65536
/* 9.3.1.42 of 3GPP TS 38.473 - gNB-CU System Information */
#define F1AP_MAX_NO_SIB_TYPES 32

typedef struct f1ap_net_config_t {
  char *CU_f1_ip_address;
  char *DU_f1c_ip_address;
  char *DU_f1u_ip_address;
  uint16_t CUport;
  uint16_t DUport;
} f1ap_net_config_t;

typedef struct f1ap_plmn_t {
  uint16_t mcc;
  uint16_t mnc;
  uint8_t  mnc_digit_length;
} f1ap_plmn_t;

typedef struct f1ap_cp_tnl_s {
  in_addr_t tl_address; // currently only IPv4 supported
  uint16_t port;
} f1ap_cp_tnl_t;

typedef enum f1ap_mode_t { F1AP_MODE_TDD = 0, F1AP_MODE_FDD = 1 } f1ap_mode_t;

typedef struct f1ap_nr_frequency_info_t {
  uint32_t arfcn;
  int band;
} f1ap_nr_frequency_info_t;

typedef struct f1ap_transmission_bandwidth_t {
  uint8_t scs;
  uint16_t nrb;
} f1ap_transmission_bandwidth_t;

typedef struct f1ap_fdd_info_t {
  f1ap_nr_frequency_info_t ul_freqinfo;
  f1ap_nr_frequency_info_t dl_freqinfo;
  f1ap_transmission_bandwidth_t ul_tbw;
  f1ap_transmission_bandwidth_t dl_tbw;
} f1ap_fdd_info_t;

typedef struct f1ap_tdd_info_t {
  f1ap_nr_frequency_info_t freqinfo;
  f1ap_transmission_bandwidth_t tbw;
} f1ap_tdd_info_t;

typedef struct f1ap_served_cell_info_t {
  // NR CGI
  f1ap_plmn_t plmn;
  uint64_t nr_cellid; // NR Global Cell Id

  // NR Physical Cell Ids
  uint16_t nr_pci;

  /* Tracking area code */
  uint32_t *tac;

  // Number of slice support items (max 16, could be increased to as much as 1024)
  uint16_t num_ssi;
  nssai_t nssai[16];

  f1ap_mode_t mode;
  union {
    f1ap_fdd_info_t fdd;
    f1ap_tdd_info_t tdd;
  };

  uint8_t *measurement_timing_config;
  int measurement_timing_config_len;
} f1ap_served_cell_info_t;

typedef struct f1ap_gnb_du_system_info_t {
  uint8_t *mib;
  int mib_length;
  uint8_t *sib1;
  int sib1_length;
} f1ap_gnb_du_system_info_t;

typedef struct f1ap_setup_req_s {
  /// ulong transaction id
  uint64_t transaction_id;

  // F1_Setup_Req payload
  uint64_t gNB_DU_id;
  char *gNB_DU_name;

  /// rrc version
  uint8_t rrc_ver[3];

  /// number of DU cells available
  uint16_t num_cells_available; //0< num_cells_available <= 512;
  struct {
    f1ap_served_cell_info_t info;
    f1ap_gnb_du_system_info_t *sys_info;
  } cell[F1AP_MAX_NB_CELLS];
} f1ap_setup_req_t;

typedef struct f1ap_du_register_req_t {
  f1ap_setup_req_t setup_req;
  f1ap_net_config_t net_config;
} f1ap_du_register_req_t;

typedef struct f1ap_sib_msg_t {
  /// RRC container with system information owned by gNB-CU
  uint8_t *SI_container;
  int SI_container_length;
  /// SIB block type, e.g. 2 for sibType2
  int SI_type;
} f1ap_sib_msg_t;

typedef struct served_cells_to_activate_s {
  f1ap_plmn_t plmn;
  // NR Global Cell Id
  uint64_t nr_cellid;
  /// NRPCI [int 0..1007]
  uint16_t nrpci;
  /// num SI messages per DU cell
  uint8_t num_SI;
  /// gNB-CU System Information message (up to 32 messages per cell)
  f1ap_sib_msg_t SI_msg[F1AP_MAX_NO_SIB_TYPES];
} served_cells_to_activate_t;

typedef struct f1ap_setup_resp_s {
  /// ulong transaction id
  uint64_t transaction_id;
  /// string holding gNB_CU_name
  char     *gNB_CU_name;
  /// number of DU cells to activate
  uint16_t num_cells_to_activate; //0< num_cells_to_activate <= 512;
  served_cells_to_activate_t cells_to_activate[F1AP_MAX_NB_CELLS];

  /// rrc version
  uint8_t rrc_ver[3];

} f1ap_setup_resp_t;

typedef struct f1ap_gnb_cu_configuration_update_s {
  /// Transaction ID
  uint64_t transaction_id;
  /// number of DU cells to activate
  uint16_t num_cells_to_activate; //0< num_cells_to_activate/mod <= 512;
  served_cells_to_activate_t cells_to_activate[F1AP_MAX_NB_CELLS];
} f1ap_gnb_cu_configuration_update_t;

typedef struct f1ap_setup_failure_s {
  uint16_t cause;
  uint16_t time_to_wait;
  uint16_t criticality_diagnostics;
  /// Transaction ID (M)
  uint64_t transaction_id;
} f1ap_setup_failure_t;

typedef struct f1ap_gnb_cu_configuration_update_acknowledge_s {
  uint64_t transaction_id;
  // Cells Failed to be Activated List
  uint16_t num_cells_failed_to_be_activated;
  struct {
    // NR CGI
    f1ap_plmn_t plmn;
    uint64_t nr_cellid;
    uint16_t cause;
  } cells_failed_to_be_activated[F1AP_MAX_NB_CELLS];
  int have_criticality;
  uint16_t criticality_diagnostics;
  // gNB-CU TNL Association Setup List
  uint16_t noofTNLAssociations_to_setup;
  f1ap_cp_tnl_t tnlAssociations_to_setup[F1AP_MAX_NO_OF_TNL_ASSOCIATIONS];
  // gNB-CU TNL Association Failed to Setup List
  uint16_t noofTNLAssociations_failed;
  f1ap_cp_tnl_t tnlAssociations_failed[F1AP_MAX_NO_OF_TNL_ASSOCIATIONS];
  // Dedicated SI Delivery Needed UE List
  uint16_t noofDedicatedSIDeliveryNeededUEs;
  struct {
    uint32_t gNB_CU_ue_id;
    // NR CGI
    f1ap_plmn_t ue_plmn;
    uint64_t ue_nr_cellid;
  } dedicatedSIDeliveryNeededUEs[F1AP_MAX_NO_UE_ID];
} f1ap_gnb_cu_configuration_update_acknowledge_t;

typedef struct f1ap_gnb_cu_configuration_update_failure_s {
  uint16_t cause;
  uint16_t time_to_wait;
  uint16_t criticality_diagnostics; 
} f1ap_gnb_cu_configuration_update_failure_t;

/*DU configuration messages*/
typedef struct f1ap_gnb_du_configuration_update_s {
  /*TODO UPDATE TO SUPPORT DU CONFIG*/

  /* Transaction ID */
  uint64_t transaction_id;
  /// int cells_to_add
  uint16_t num_cells_to_add;
  struct {
    f1ap_served_cell_info_t info;
    f1ap_gnb_du_system_info_t *sys_info;
  } cell_to_add[F1AP_MAX_NB_CELLS];

  /// int cells_to_modify
  uint16_t num_cells_to_modify;
  struct {
    f1ap_plmn_t old_plmn;
    uint64_t old_nr_cellid; // NR Global Cell Id
    f1ap_served_cell_info_t info;
    f1ap_gnb_du_system_info_t *sys_info;
  } cell_to_modify[F1AP_MAX_NB_CELLS];

  /// int cells_to_delete
  uint16_t num_cells_to_delete;
  struct {
    // NR CGI
    f1ap_plmn_t plmn;
    uint64_t nr_cellid; // NR Global Cell Id
  } cell_to_delete[F1AP_MAX_NB_CELLS];
  /// gNB-DU unique ID, at least within a gNB-CU (0 .. 2^36 - 1)
  uint64_t *gNB_DU_ID;
} f1ap_gnb_du_configuration_update_t;

typedef struct f1ap_gnb_du_configuration_update_acknowledge_s {
  /// ulong transaction id
  uint64_t transaction_id;
  /// number of DU cells to activate
  uint16_t num_cells_to_activate; // 0< num_cells_to_activate <= 512;
  served_cells_to_activate_t cells_to_activate[F1AP_MAX_NB_CELLS];
} f1ap_gnb_du_configuration_update_acknowledge_t;

typedef struct f1ap_gnb_du_configuration_update_failure_s {
  /*TODO UPDATE TO SUPPORT DU CONFIG*/
  uint16_t cause;
  uint16_t time_to_wait;
  uint16_t criticality_diagnostics;
} f1ap_gnb_du_configuration_update_failure_t;

typedef struct f1ap_dl_rrc_message_s {

  uint32_t gNB_CU_ue_id;
  uint32_t gNB_DU_ue_id;
  uint32_t *old_gNB_DU_ue_id;
  uint8_t  srb_id;
  uint8_t  execute_duplication;
  uint8_t *rrc_container;
  int      rrc_container_length;
  union {
    // both map 0..255 => 1..256
    uint8_t en_dc;
    uint8_t ngran;
  } RAT_frequency_priority_information;
} f1ap_dl_rrc_message_t;

typedef struct f1ap_initial_ul_rrc_message_s {
  uint32_t gNB_DU_ue_id;
  f1ap_plmn_t plmn;
  /// nr cell id
  uint64_t nr_cellid;
  /// crnti
  uint16_t crnti;
  uint8_t *rrc_container;
  int      rrc_container_length;
  uint8_t *du2cu_rrc_container;
  int      du2cu_rrc_container_length;
  uint8_t transaction_id;
} f1ap_initial_ul_rrc_message_t;

typedef struct f1ap_ul_rrc_message_s {
  uint32_t gNB_CU_ue_id;
  uint32_t gNB_DU_ue_id;
  uint8_t  srb_id;
  uint8_t *rrc_container;
  int      rrc_container_length;
} f1ap_ul_rrc_message_t;

typedef struct f1ap_up_tnl_s {
  in_addr_t tl_address; // currently only IPv4 supported
  uint32_t teid;
  uint16_t port;
} f1ap_up_tnl_t;

typedef enum preemption_capability_e {
  SHALL_NOT_TRIGGER_PREEMPTION,
  MAY_TRIGGER_PREEMPTION,
} preemption_capability_t;

typedef enum preemption_vulnerability_e {
  NOT_PREEMPTABLE,
  PREEMPTABLE,
} preemption_vulnerability_t;

typedef struct f1ap_qos_characteristics_s {
  union {
    struct {
      long fiveqi;
      long qos_priority_level;
    } non_dynamic;
    struct {
      long fiveqi; // -1 -> optional
      long qos_priority_level;
      long packet_delay_budget;
      struct {
        long per_scalar;
        long per_exponent;
      } packet_error_rate;
    } dynamic;
  };
  fiveQI_t qos_type;
} f1ap_qos_characteristics_t;

typedef struct f1ap_ngran_allocation_retention_priority_s {
  uint16_t priority_level;
  preemption_capability_t preemption_capability;
  preemption_vulnerability_t preemption_vulnerability;
} f1ap_ngran_allocation_retention_priority_t;

typedef struct f1ap_qos_flow_level_qos_parameters_s {
  f1ap_qos_characteristics_t qos_characteristics;
  f1ap_ngran_allocation_retention_priority_t alloc_reten_priority;
} f1ap_qos_flow_level_qos_parameters_t;

typedef struct f1ap_flows_mapped_to_drb_s {
  long qfi; // qos flow identifier
  f1ap_qos_flow_level_qos_parameters_t qos_params;
} f1ap_flows_mapped_to_drb_t;

typedef struct f1ap_drb_information_s {
  f1ap_qos_flow_level_qos_parameters_t drb_qos;
  f1ap_flows_mapped_to_drb_t *flows_mapped_to_drb;
  uint8_t flows_to_be_setup_length;
} f1ap_drb_information_t;

typedef enum f1ap_rlc_mode_t { F1AP_RLC_MODE_AM, F1AP_RLC_MODE_UM_BIDIR, F1AP_RLC_UM_UNI_UL, F1AP_RLC_UM_UNI_DL } f1ap_rlc_mode_t;
typedef struct f1ap_drb_to_be_setup_s {
  long           drb_id;
  f1ap_up_tnl_t  up_ul_tnl[2];
  uint8_t        up_ul_tnl_length;
  f1ap_up_tnl_t  up_dl_tnl[2];
  uint8_t        up_dl_tnl_length;
  f1ap_drb_information_t drb_info;
  f1ap_rlc_mode_t rlc_mode;
  nssai_t nssai;
} f1ap_drb_to_be_setup_t;

typedef struct f1ap_srb_to_be_setup_s {
  long           srb_id;
  uint8_t        lcid;
} f1ap_srb_to_be_setup_t;

typedef struct f1ap_rb_failed_to_be_setup_s {
  long           rb_id;
} f1ap_rb_failed_to_be_setup_t;

typedef struct f1ap_drb_to_be_released_t {
  long rb_id;
} f1ap_drb_to_be_released_t;

typedef struct cu_to_du_rrc_information_s {
  uint8_t * cG_ConfigInfo;
  uint32_t   cG_ConfigInfo_length;
  uint8_t * uE_CapabilityRAT_ContainerList;
  uint32_t   uE_CapabilityRAT_ContainerList_length;
  uint8_t * measConfig;
  uint32_t   measConfig_length;
}cu_to_du_rrc_information_t;

typedef struct du_to_cu_rrc_information_s {
  uint8_t * cellGroupConfig;
  uint32_t  cellGroupConfig_length;
  uint8_t * measGapConfig;
  uint32_t  measGapConfig_length;
  uint8_t * requestedP_MaxFR1;
  uint32_t  requestedP_MaxFR1_length;
}du_to_cu_rrc_information_t;

typedef enum QoS_information_e {
  NG_RAN_QoS    = 0,
  EUTRAN_QoS    = 1,
} QoS_information_t;

typedef enum ReconfigurationCompl_e {
  RRCreconf_info_not_present = 0,
  RRCreconf_failure          = 1,
  RRCreconf_success          = 2,
} ReconfigurationCompl_t;

typedef struct f1ap_ue_context_setup_s {
  uint32_t gNB_CU_ue_id;
  uint32_t gNB_DU_ue_id;
  // SpCell Info
  f1ap_plmn_t plmn;
  uint64_t nr_cellid;
  uint8_t servCellIndex;
  uint8_t *cellULConfigured;
  uint32_t servCellId;
  cu_to_du_rrc_information_t *cu_to_du_rrc_information;
  uint8_t  cu_to_du_rrc_information_length;
  //uint8_t *du_to_cu_rrc_information;
  du_to_cu_rrc_information_t *du_to_cu_rrc_information;
  uint32_t  du_to_cu_rrc_information_length;
  f1ap_drb_to_be_setup_t *drbs_to_be_setup;
  uint8_t  drbs_to_be_setup_length;
  f1ap_drb_to_be_setup_t *drbs_to_be_modified;
    uint8_t  drbs_to_be_modified_length;
  QoS_information_t QoS_information_type;
  uint8_t  drbs_failed_to_be_setup_length;
  f1ap_rb_failed_to_be_setup_t *drbs_failed_to_be_setup;
  f1ap_srb_to_be_setup_t *srbs_to_be_setup;
  uint8_t  srbs_to_be_setup_length;
  uint8_t  srbs_failed_to_be_setup_length;
  uint8_t drbs_to_be_released_length;
  f1ap_drb_to_be_released_t *drbs_to_be_released;
  f1ap_rb_failed_to_be_setup_t *srbs_failed_to_be_setup;
  ReconfigurationCompl_t ReconfigComplOutcome;
  uint8_t *rrc_container;
  int      rrc_container_length;
} f1ap_ue_context_setup_t, f1ap_ue_context_modif_req_t, f1ap_ue_context_modif_resp_t;

typedef enum F1ap_Cause_e {
  F1AP_CAUSE_NOTHING,  /* No components present */
  F1AP_CAUSE_RADIO_NETWORK,
  F1AP_CAUSE_TRANSPORT,
  F1AP_CAUSE_PROTOCOL,
  F1AP_CAUSE_MISC,
} f1ap_Cause_t;

typedef struct f1ap_ue_context_modif_required_t {
  uint32_t gNB_CU_ue_id;
  uint32_t gNB_DU_ue_id;
  du_to_cu_rrc_information_t *du_to_cu_rrc_information;
  f1ap_Cause_t cause;
  long cause_value;
} f1ap_ue_context_modif_required_t;

typedef struct f1ap_ue_context_modif_confirm_t {
  uint32_t gNB_CU_ue_id;
  uint32_t gNB_DU_ue_id;
  uint8_t *rrc_container;
  int      rrc_container_length;
} f1ap_ue_context_modif_confirm_t;

typedef struct f1ap_ue_context_modif_refuse_t {
  uint32_t gNB_CU_ue_id;
  uint32_t gNB_DU_ue_id;
  f1ap_Cause_t cause;
  long cause_value;
} f1ap_ue_context_modif_refuse_t;

typedef struct f1ap_ue_context_release_s {
  uint32_t gNB_CU_ue_id;
  uint32_t gNB_DU_ue_id;
  f1ap_Cause_t  cause;
  long          cause_value;
  uint8_t      *rrc_container;
  int           rrc_container_length;
  int           srb_id;
} f1ap_ue_context_release_req_t, f1ap_ue_context_release_cmd_t, f1ap_ue_context_release_complete_t;

typedef struct f1ap_paging_ind_s {
  uint16_t ueidentityindexvalue;
  uint64_t fiveg_s_tmsi;
  uint8_t  fiveg_s_tmsi_length;
  f1ap_plmn_t plmn;
  uint64_t nr_cellid;
  uint8_t  paging_drx;
} f1ap_paging_ind_t;

typedef struct f1ap_lost_connection_t {
  int dummy;
} f1ap_lost_connection_t;

typedef enum F1AP_ResetType_e {
  F1AP_RESET_ALL,
  F1AP_RESET_PART_OF_F1_INTERFACE
} f1ap_ResetType_t;

typedef struct f1ap_reset_t {
  uint64_t          transaction_id;
  f1ap_Cause_t      cause;
  long              cause_value;
  f1ap_ResetType_t  reset_type;
  struct {
    uint32_t gNB_CU_ue_id;
    uint32_t gNB_DU_ue_id;
  } ue_to_reset[F1AP_MAX_NO_OF_INDIVIDUAL_CONNECTIONS_TO_RESET];
} f1ap_reset_t;

typedef struct f1ap_reset_ack_t {
  uint64_t          transaction_id;
  struct {
    uint32_t gNB_CU_ue_id;
    uint32_t gNB_DU_ue_id;
  } ue_to_reset[F1AP_MAX_NO_OF_INDIVIDUAL_CONNECTIONS_TO_RESET];
  uint16_t criticality_diagnostics;
} f1ap_reset_ack_t;

#endif /* F1AP_MESSAGES_TYPES_H_ */
