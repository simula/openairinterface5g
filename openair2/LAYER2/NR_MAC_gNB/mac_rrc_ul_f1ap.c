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

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "nr_mac_gNB.h"
#include "intertask_interface.h"
#include "openair3/ocp-gtpu/gtp_itf.h"

#include "mac_rrc_ul.h"

#include "f1ap_lib_extern.h"
#include "lib/f1ap_interface_management.h"

static f1ap_net_config_t read_DU_IP_config(const eth_params_t* f1_params, const char *f1u_ip_addr)
{
  f1ap_net_config_t nc = {0};

  nc.CU_f1_ip_address= strdup(f1_params->remote_addr);
  nc.CUport = f1_params->remote_portd;

  nc.DU_f1c_ip_address = strdup(f1_params->my_addr);
  nc.DU_f1u_ip_address = strdup(f1u_ip_addr);
  nc.DUport = f1_params->my_portd;
  LOG_I(F1AP,
        "F1-C DU IPaddr %s, connect to F1-C CU %s, binding GTP to %s\n",
        nc.DU_f1c_ip_address,
        nc.CU_f1_ip_address,
        nc.DU_f1u_ip_address);

  // sctp_in_streams/sctp_out_streams are given by SCTP layer
  return nc;
}

static void f1_reset_du_initiated_f1ap(const f1ap_reset_t *reset)
{
  (void) reset;
  AssertFatal(false, "%s() not implemented yet\n", __func__);
}

static void f1_reset_acknowledge_cu_initiated_f1ap(const f1ap_reset_ack_t *ack)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_RESET_ACK);
  f1ap_reset_ack_t *f1ap_msg = &F1AP_RESET_ACK(msg);
  *f1ap_msg = *ack;
  itti_send_msg_to_task(TASK_DU_F1, 0, msg);
}

static void f1_setup_request_f1ap(const f1ap_setup_req_t *req)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_DU_REGISTER_REQ);
  f1ap_setup_req_t f1ap_setup = cp_f1ap_setup_request(req);
  F1AP_DU_REGISTER_REQ(msg).setup_req = f1ap_setup;
  F1AP_DU_REGISTER_REQ(msg).net_config = read_DU_IP_config(&RC.nrmac[0]->eth_params_n, RC.nrmac[0]->f1u_addr);
  itti_send_msg_to_task(TASK_DU_F1, 0, msg);
}

static void gnb_du_configuration_update_f1ap(const f1ap_gnb_du_configuration_update_t *upd)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_GNB_DU_CONFIGURATION_UPDATE);
  /* copy F1AP message */
  f1ap_gnb_du_configuration_update_t cp = cp_f1ap_du_configuration_update(upd);
  /* transfer to ITTI message */
  F1AP_GNB_DU_CONFIGURATION_UPDATE(msg) = cp;
  /* free after copy */
  free_f1ap_du_configuration_update(upd);
  /* send to RRC task */
  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
}

static void ue_context_setup_response_f1ap(const f1ap_ue_context_setup_t *req, const f1ap_ue_context_setup_t *resp)
{
  DevAssert(req->drbs_to_be_setup_length == resp->drbs_to_be_setup_length);

  DevAssert(req->srbs_to_be_setup_length == resp->srbs_to_be_setup_length);
  MessageDef *msg = itti_alloc_new_message (TASK_MAC_GNB, 0, F1AP_UE_CONTEXT_SETUP_RESP);
  f1ap_ue_context_setup_t *f1ap_msg = &F1AP_UE_CONTEXT_SETUP_RESP(msg);
  /* copy all fields, but reallocate rrc_containers! */
  *f1ap_msg = *resp;

  if (resp->srbs_to_be_setup_length > 0) {
    DevAssert(resp->srbs_to_be_setup != NULL);
    f1ap_msg->srbs_to_be_setup_length = resp->srbs_to_be_setup_length;
    f1ap_msg->srbs_to_be_setup = calloc(f1ap_msg->srbs_to_be_setup_length, sizeof(*f1ap_msg->srbs_to_be_setup));
    for (int i = 0; i < f1ap_msg->srbs_to_be_setup_length; ++i)
      f1ap_msg->srbs_to_be_setup[i] = resp->srbs_to_be_setup[i];
  }
  if (resp->drbs_to_be_setup_length > 0) {
    DevAssert(resp->drbs_to_be_setup != NULL);
    f1ap_msg->drbs_to_be_setup_length = resp->drbs_to_be_setup_length;
    f1ap_msg->drbs_to_be_setup = calloc(f1ap_msg->drbs_to_be_setup_length, sizeof(*f1ap_msg->drbs_to_be_setup));
    for (int i = 0; i < f1ap_msg->drbs_to_be_setup_length; ++i)
      f1ap_msg->drbs_to_be_setup[i] = resp->drbs_to_be_setup[i];
  }

  f1ap_msg->du_to_cu_rrc_information = malloc(sizeof(*resp->du_to_cu_rrc_information));
  AssertFatal(f1ap_msg->du_to_cu_rrc_information != NULL, "out of memory\n");
  f1ap_msg->du_to_cu_rrc_information_length = resp->du_to_cu_rrc_information_length;
  du_to_cu_rrc_information_t *du2cu = f1ap_msg->du_to_cu_rrc_information;
  du2cu->cellGroupConfig_length = resp->du_to_cu_rrc_information->cellGroupConfig_length;
  du2cu->cellGroupConfig = calloc(du2cu->cellGroupConfig_length, sizeof(*du2cu->cellGroupConfig));
  AssertFatal(du2cu->cellGroupConfig != NULL, "out of memory\n");
  memcpy(du2cu->cellGroupConfig, resp->du_to_cu_rrc_information->cellGroupConfig, du2cu->cellGroupConfig_length);

  itti_send_msg_to_task(TASK_DU_F1, 0, msg);
}

static void ue_context_modification_response_f1ap(const f1ap_ue_context_modif_req_t *req, const f1ap_ue_context_modif_resp_t *resp)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_UE_CONTEXT_MODIFICATION_RESP);
  f1ap_ue_context_modif_resp_t *f1ap_msg = &F1AP_UE_CONTEXT_MODIFICATION_RESP(msg);
  /* copy all fields, but reallocate rrc_containers! */
  *f1ap_msg = *resp;

  if (resp->srbs_to_be_setup_length > 0) {
    DevAssert(resp->srbs_to_be_setup != NULL);
    f1ap_msg->srbs_to_be_setup_length = resp->srbs_to_be_setup_length;
    f1ap_msg->srbs_to_be_setup = calloc(f1ap_msg->srbs_to_be_setup_length, sizeof(*f1ap_msg->srbs_to_be_setup));
    for (int i = 0; i < f1ap_msg->srbs_to_be_setup_length; ++i)
      f1ap_msg->srbs_to_be_setup[i] = resp->srbs_to_be_setup[i];
  }
  if (resp->drbs_to_be_setup_length > 0) {
    DevAssert(resp->drbs_to_be_setup != NULL);
    f1ap_msg->drbs_to_be_setup_length = resp->drbs_to_be_setup_length;
    f1ap_msg->drbs_to_be_setup = calloc(f1ap_msg->drbs_to_be_setup_length, sizeof(*f1ap_msg->drbs_to_be_setup));
    for (int i = 0; i < f1ap_msg->drbs_to_be_setup_length; ++i)
      f1ap_msg->drbs_to_be_setup[i] = resp->drbs_to_be_setup[i];
  }

  if (resp->du_to_cu_rrc_information != NULL) {
    f1ap_msg->du_to_cu_rrc_information = calloc(1, sizeof(*resp->du_to_cu_rrc_information));
    AssertFatal(f1ap_msg->du_to_cu_rrc_information != NULL, "out of memory\n");
    f1ap_msg->du_to_cu_rrc_information_length = resp->du_to_cu_rrc_information_length;
    du_to_cu_rrc_information_t *du2cu = f1ap_msg->du_to_cu_rrc_information;
    du2cu->cellGroupConfig_length = resp->du_to_cu_rrc_information->cellGroupConfig_length;
    du2cu->cellGroupConfig = calloc(du2cu->cellGroupConfig_length, sizeof(*du2cu->cellGroupConfig));
    AssertFatal(du2cu->cellGroupConfig != NULL, "out of memory\n");
    memcpy(du2cu->cellGroupConfig, resp->du_to_cu_rrc_information->cellGroupConfig, du2cu->cellGroupConfig_length);
  }

  itti_send_msg_to_task(TASK_DU_F1, 0, msg);
}

static void ue_context_modification_required_f1ap(const f1ap_ue_context_modif_required_t *required)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_UE_CONTEXT_MODIFICATION_REQUIRED);
  f1ap_ue_context_modif_required_t *f1ap_msg = &F1AP_UE_CONTEXT_MODIFICATION_REQUIRED(msg);
  f1ap_msg->gNB_CU_ue_id = required->gNB_CU_ue_id;
  f1ap_msg->gNB_DU_ue_id = required->gNB_DU_ue_id;
  f1ap_msg->du_to_cu_rrc_information = NULL;
  if (required->du_to_cu_rrc_information != NULL) {
    f1ap_msg->du_to_cu_rrc_information = calloc(1, sizeof(*f1ap_msg->du_to_cu_rrc_information));
    AssertFatal(f1ap_msg->du_to_cu_rrc_information != NULL, "out of memory\n");
    du_to_cu_rrc_information_t *du2cu = f1ap_msg->du_to_cu_rrc_information;
    AssertFatal(required->du_to_cu_rrc_information->cellGroupConfig != NULL && required->du_to_cu_rrc_information->cellGroupConfig_length > 0,
                "cellGroupConfig is mandatory\n");
    du2cu->cellGroupConfig_length = required->du_to_cu_rrc_information->cellGroupConfig_length;
    du2cu->cellGroupConfig = malloc(du2cu->cellGroupConfig_length * sizeof(*du2cu->cellGroupConfig));
    AssertFatal(du2cu->cellGroupConfig != NULL, "out of memory\n");
    memcpy(du2cu->cellGroupConfig, required->du_to_cu_rrc_information->cellGroupConfig, du2cu->cellGroupConfig_length);
    AssertFatal(required->du_to_cu_rrc_information->measGapConfig == NULL && required->du_to_cu_rrc_information->measGapConfig_length == 0, "not handled yet\n");
    AssertFatal(required->du_to_cu_rrc_information->requestedP_MaxFR1 == NULL && required->du_to_cu_rrc_information->requestedP_MaxFR1_length == 0, "not handled yet\n");
  }
  f1ap_msg->cause = required->cause;
  f1ap_msg->cause_value = required->cause_value;
  itti_send_msg_to_task(TASK_DU_F1, 0, msg);
}

static void ue_context_release_request_f1ap(const f1ap_ue_context_release_req_t* req)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_UE_CONTEXT_RELEASE_REQ);
  f1ap_ue_context_release_req_t *f1ap_msg = &F1AP_UE_CONTEXT_RELEASE_REQ(msg);
  *f1ap_msg = *req;
  itti_send_msg_to_task(TASK_DU_F1, 0, msg);
}

static void ue_context_release_complete_f1ap(const f1ap_ue_context_release_complete_t *complete)
{
  newGtpuDeleteAllTunnels(0, complete->gNB_DU_ue_id);

  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_UE_CONTEXT_RELEASE_COMPLETE);
  f1ap_ue_context_release_complete_t *f1ap_msg = &F1AP_UE_CONTEXT_RELEASE_COMPLETE(msg);
  *f1ap_msg = *complete;
  itti_send_msg_to_task(TASK_DU_F1, 0, msg);
}

static void initial_ul_rrc_message_transfer_f1ap(module_id_t module_id, const f1ap_initial_ul_rrc_message_t *ul_rrc)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_INITIAL_UL_RRC_MESSAGE);
  f1ap_initial_ul_rrc_message_t *f1ap_msg = &F1AP_INITIAL_UL_RRC_MESSAGE(msg);
  *f1ap_msg = cp_initial_ul_rrc_message_transfer(ul_rrc);
  itti_send_msg_to_task(TASK_DU_F1, module_id, msg);
}

void mac_rrc_ul_f1ap_init(struct nr_mac_rrc_ul_if_s *mac_rrc)
{
  mac_rrc->f1_reset = f1_reset_du_initiated_f1ap;
  mac_rrc->f1_reset_acknowledge = f1_reset_acknowledge_cu_initiated_f1ap;
  mac_rrc->f1_setup_request = f1_setup_request_f1ap;
  mac_rrc->gnb_du_configuration_update = gnb_du_configuration_update_f1ap;
  mac_rrc->ue_context_setup_response = ue_context_setup_response_f1ap;
  mac_rrc->ue_context_modification_response = ue_context_modification_response_f1ap;
  mac_rrc->ue_context_modification_required = ue_context_modification_required_f1ap;
  mac_rrc->ue_context_release_request = ue_context_release_request_f1ap;
  mac_rrc->ue_context_release_complete = ue_context_release_complete_f1ap;
  mac_rrc->initial_ul_rrc_message_transfer = initial_ul_rrc_message_transfer_f1ap;
}

