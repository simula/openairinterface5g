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

#ifndef E1AP_INTERFACE_MANAGEMENT_H_
#define E1AP_INTERFACE_MANAGEMENT_H_

#include <stdbool.h>
#include "openair2/COMMON/e1ap_messages_types.h"
#include "common/platform_types.h"

/* GNB-CU-UP E1 Setup Request */
struct E1AP_E1AP_PDU *encode_e1ap_cuup_setup_request(const e1ap_setup_req_t *msg);
bool decode_e1ap_cuup_setup_request(const struct E1AP_E1AP_PDU *pdu, e1ap_setup_req_t *out);
e1ap_setup_req_t cp_e1ap_cuup_setup_request(const e1ap_setup_req_t *msg);
void free_e1ap_cuup_setup_request(e1ap_setup_req_t *msg);
bool eq_e1ap_cuup_setup_request(const e1ap_setup_req_t *a, const e1ap_setup_req_t *b);

/* GNB-CU-UP E1 Setup Response */
struct E1AP_E1AP_PDU *encode_e1ap_cuup_setup_response(const e1ap_setup_resp_t *msg);
bool decode_e1ap_cuup_setup_response(const struct E1AP_E1AP_PDU *pdu, e1ap_setup_resp_t *out);
e1ap_setup_resp_t cp_e1ap_cuup_setup_response(const e1ap_setup_resp_t *msg);
void free_e1ap_cuup_setup_response(e1ap_setup_resp_t *msg);
bool eq_e1ap_cuup_setup_response(const e1ap_setup_resp_t *a, const e1ap_setup_resp_t *b);

/* GNB-CU-UP E1 Setup Failure */
struct E1AP_E1AP_PDU *encode_e1ap_cuup_setup_failure(const e1ap_setup_fail_t *msg);
bool decode_e1ap_cuup_setup_failure(const struct E1AP_E1AP_PDU *pdu, e1ap_setup_fail_t *out);
e1ap_setup_fail_t cp_e1ap_cuup_setup_failure(const e1ap_setup_fail_t *msg);
void free_e1ap_cuup_setup_failure(e1ap_setup_fail_t *msg);
bool eq_e1ap_cuup_setup_failure(const e1ap_setup_fail_t *a, const e1ap_setup_fail_t *b);

#endif /* E1AP_INTERFACE_MANAGEMENT_H_ */
