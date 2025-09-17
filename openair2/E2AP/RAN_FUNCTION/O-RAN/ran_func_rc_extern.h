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

#ifndef RAN_FUNC_SM_RAN_CTRL_EXTERN_AGENT_H
#define RAN_FUNC_SM_RAN_CTRL_EXTERN_AGENT_H

#include "openair2/RRC/NR/nr_rrc_defs.h"
#include "openair2/E2AP/flexric/src/lib/3gpp/ie/rrc_msg_id.h"
#include "openair2/E2AP/flexric/src/lib/3gpp/ie/network_interface_type.h"
#include "openair2/E2AP/flexric/src/sm/rc_sm/ie/ir/rrc_state.h"

void signal_rrc_msg(const nr_rrc_class_e nr_channel, const uint32_t rrc_msg_id, const byte_array_t rrc_ba);

void signal_ue_id(const gNB_RRC_UE_t *rrc_ue_context, const uint16_t class, const uint32_t msg_id);

void signal_rrc_state_changed_to(const gNB_RRC_UE_t *rrc_ue_context, const rrc_state_e2sm_rc_e rrc_state);

#endif
