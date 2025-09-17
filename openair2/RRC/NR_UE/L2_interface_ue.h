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

#include "rrc_defs.h"
#include "common/utils/ocp_itti/intertask_interface.h"

#ifndef _L2_INTERFACE_UE_H_
#define _L2_INTERFACE_UE_H_
void nr_mac_rrc_sync_ind(const module_id_t module_id, const frame_t frame, const bool in_sync);
void nr_mac_rrc_data_ind_ue(const module_id_t module_id,
                            const int CC_id,
                            const uint8_t gNB_index,
                            const frame_t frame,
                            const int slot,
                            const rnti_t rnti,
                            const uint32_t cellid,
                            const long arfcn,
                            const channel_t channel,
                            const uint8_t* pduP,
                            const sdu_size_t pdu_len);
void nr_mac_rrc_inactivity_timer_ind(const module_id_t mod_id);
void nr_mac_rrc_msg3_ind(const module_id_t mod_id, const int rnti, int gnb_id);
void nr_ue_rrc_timer_trigger(int instance, int frame, int gnb_id);
void nr_mac_rrc_ra_ind(const module_id_t mod_id, int frame, bool success);
void nsa_sendmsg_to_lte_ue(const void *message, size_t msg_len, Rrc_Msg_Type_t msg_type);
#endif

