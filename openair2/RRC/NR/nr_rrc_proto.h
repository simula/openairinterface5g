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

/*! \file nr_rrc_proto.h
 * \brief RRC functions prototypes for gNB
 * \author Navid Nikaein and Raymond Knopp, WEI-TAI-CHEN
 * \date 2010 - 2014, 2018
 * \email navid.nikaein@eurecom.fr, kroempa@gmail.com
 * \version 1.0
 * \company Eurecom, NTUST
 */
/** \addtogroup _rrc
 *  @{
 */

#ifndef __NR_RRC_PROTO_H__
#define __NR_RRC_PROTO_H__

#include "RRC/NR/nr_rrc_defs.h"
#include "NR_CG-Config.h"
#include "NR_CG-ConfigInfo.h"
#include "NR_RRCReconfiguration.h"
#include "RRC/NR/MESSAGES/asn1_msg.h"

#define SRB1 1
#define SRB2 2

void rrc_add_nsa_user(gNB_RRC_INST *rrc, x2ap_ENDC_sgnb_addition_req_t *m, sctp_assoc_t assoc_id);
void rrc_add_nsa_user_resp(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, const f1ap_ue_context_setup_resp_t *resp);
void rrc_release_nsa_user(gNB_RRC_INST *rrc, rrc_gNB_ue_context_t *ue_context);
void rrc_remove_nsa_user_context(gNB_RRC_INST *rrc, rrc_gNB_ue_context_t *ue_context);

void rrc_remove_ue(gNB_RRC_INST *rrc, rrc_gNB_ue_context_t *ue_context_p);

int parse_CG_ConfigInfo(gNB_RRC_INST *rrc, NR_CG_ConfigInfo_t *CG_ConfigInfo, x2ap_ENDC_sgnb_addition_req_t *m);

void rrc_gNB_generate_SecurityModeCommand(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue_p);

void rrc_forward_ue_nas_message(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE);

unsigned int rrc_gNB_get_next_transaction_identifier(module_id_t gnb_mod_idP);

void rrc_gNB_generate_RRCRelease(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE);

/**\brief RRC eNB task.
   \param args_p Pointer on arguments to start the task. */
void *rrc_gnb_task(void *args_p);

int nr_rrc_reconfiguration_req(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue_p, const int dl_bwp_id, const int ul_bwp_id);

void rrc_gNB_generate_dedicatedRRCReconfiguration_release(gNB_RRC_INST *rrc,
                                                          gNB_RRC_UE_t *ue_p,
                                                          uint8_t xid,
                                                          uint32_t nas_length,
                                                          uint8_t *nas_buffer);

NR_MeasConfig_t *nr_rrc_get_measconfig(const gNB_RRC_INST *rrc, uint64_t nr_cellid);

bool ue_associated_to_cuup(const gNB_RRC_INST *rrc, const gNB_RRC_UE_t *ue);
sctp_assoc_t get_existing_cuup_for_ue(const gNB_RRC_INST *rrc, const gNB_RRC_UE_t *ue);
sctp_assoc_t get_new_cuup_for_ue(const gNB_RRC_INST *rrc, const gNB_RRC_UE_t *ue, int sst, int sd);
int rrc_gNB_process_e1_setup_req(sctp_assoc_t assoc_id, const e1ap_setup_req_t *req);
bool is_cuup_associated(gNB_RRC_INST *rrc);

/* Process indication of E1 connection loss on CU-CP */
void rrc_gNB_process_e1_lost_connection(gNB_RRC_INST *rrc, e1ap_lost_connection_t *lc, sctp_assoc_t assoc_id);

void bearer_context_setup_direct(e1ap_bearer_setup_req_t *req,
                                 instance_t instance);

void bearer_context_setup_e1ap(e1ap_bearer_setup_req_t *req,
                                 instance_t instance);

void ue_cxt_mod_send_e1ap(MessageDef *msg,
                          instance_t instance);

void ue_cxt_mod_direct(MessageDef *msg,
                       instance_t instance);

void prepare_and_send_ue_context_modification_f1(rrc_gNB_ue_context_t *ue_context_p,
                                                 e1ap_bearer_setup_resp_t *e1ap_resp);
bool trigger_bearer_setup(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, int n, pdusession_t *sessions, uint64_t ueAggMaxBitRateDownlink)
    __attribute__((warn_unused_result));

int rrc_gNB_generate_pcch_msg(sctp_assoc_t assoc_id, const NR_SIB1_t *sib, uint32_t tmsi, uint8_t paging_drx);

/** @}*/

/* UE Management Procedures */

void rrc_gNB_generate_UeContextSetupRequest(const gNB_RRC_INST *rrc,
                                            rrc_gNB_ue_context_t *const ue_context_pP,
                                            const e1ap_bearer_setup_resp_t *resp);

void rrc_gNB_generate_UeContextModificationRequest(const gNB_RRC_INST *rrc,
                                                   rrc_gNB_ue_context_t *const ue_context_pP,
                                                   const e1ap_bearer_setup_resp_t *resp,
                                                   int n_rel_drbs,
                                                   const f1ap_drb_to_release_t *rel_drbs);

void free_RRCReconfiguration_params(nr_rrc_reconfig_param_t params);

byte_array_t rrc_gNB_encode_RRCReconfiguration(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, nr_rrc_reconfig_param_t params);

nr_rrc_reconfig_param_t get_RRCReconfiguration_params(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, uint8_t srb_reest_bitmap, bool drb_reestablish);

pdusession_level_qos_parameter_t *get_qos_characteristics(const int qfi, rrc_pdu_session_param_t *pduSession);
f1ap_qos_flow_param_t get_qos_char_from_qos_flow_param(const pdusession_level_qos_parameter_t *qos_param);
void openair_rrc_gNB_configuration(gNB_RRC_INST *rrc, gNB_RrcConfigurationReq *configuration);
#endif
