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

#ifndef RAN_FUNC_SM_RAN_CTRL_SUBSCRIPTION_AGENT_H
#define RAN_FUNC_SM_RAN_CTRL_SUBSCRIPTION_AGENT_H

#include "openair2/E2AP/flexric/src/sm/rc_sm/ie/rc_data_ie.h"
#include "common/utils/ds/seq_arr.h"

typedef struct ran_param_data {
  uint32_t ric_req_id;
  e2sm_rc_event_trigger_t ev_tr;
} ran_param_data_t;

typedef struct {
  seq_arr_t rs1_param3;   // E2SM_RC_RS1_RRC_MESSAGE
  seq_arr_t rs1_param4;   // E2SM_RC_RS1_UE_ID
  seq_arr_t rs4_param202; // E2SM_RC_RS4_RRC_STATE_CHANGED_TO
} rc_subs_data_t;

void init_rc_subs_data(rc_subs_data_t *rc_subs_data);
void insert_rc_subs_data(seq_arr_t *seq_arr, ran_param_data_t *data);
void remove_rc_subs_data(rc_subs_data_t *rc_subs_data, uint32_t ric_req_id);

#endif
