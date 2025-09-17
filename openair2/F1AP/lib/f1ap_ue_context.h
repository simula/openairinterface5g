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

#ifndef F1AP_UE_CONTEXT_SETUP_REQ_H_
#define F1AP_UE_CONTEXT_SETUP_REQ_H_

#include <stdbool.h>
#include "f1ap_messages_types.h"

struct F1AP_F1AP_PDU;

struct F1AP_F1AP_PDU *encode_ue_context_setup_req(const f1ap_ue_context_setup_req_t *msg);
bool decode_ue_context_setup_req(const struct F1AP_F1AP_PDU *pdu, f1ap_ue_context_setup_req_t *out);
f1ap_ue_context_setup_req_t cp_ue_context_setup_req(const f1ap_ue_context_setup_req_t *orig);
bool eq_ue_context_setup_req(const f1ap_ue_context_setup_req_t *a, const f1ap_ue_context_setup_req_t *b);
void free_ue_context_setup_req(f1ap_ue_context_setup_req_t *req);

struct F1AP_F1AP_PDU *encode_ue_context_setup_resp(const f1ap_ue_context_setup_resp_t *msg);
bool decode_ue_context_setup_resp(const struct F1AP_F1AP_PDU *pdu, f1ap_ue_context_setup_resp_t *out);
f1ap_ue_context_setup_resp_t cp_ue_context_setup_resp(const f1ap_ue_context_setup_resp_t *orig);
bool eq_ue_context_setup_resp(const f1ap_ue_context_setup_resp_t *a, const f1ap_ue_context_setup_resp_t *b);
void free_ue_context_setup_resp(f1ap_ue_context_setup_resp_t *resp);

struct F1AP_F1AP_PDU *encode_ue_context_mod_req(const f1ap_ue_context_mod_req_t *msg);
bool decode_ue_context_mod_req(const struct F1AP_F1AP_PDU *pdu, f1ap_ue_context_mod_req_t *out);
f1ap_ue_context_mod_req_t cp_ue_context_mod_req(const f1ap_ue_context_mod_req_t *orig);
bool eq_ue_context_mod_req(const f1ap_ue_context_mod_req_t *a, const f1ap_ue_context_mod_req_t *b);
void free_ue_context_mod_req(f1ap_ue_context_mod_req_t *req);

struct F1AP_F1AP_PDU *encode_ue_context_mod_resp(const f1ap_ue_context_mod_resp_t *msg);
bool decode_ue_context_mod_resp(const struct F1AP_F1AP_PDU *pdu, f1ap_ue_context_mod_resp_t *out);
f1ap_ue_context_mod_resp_t cp_ue_context_mod_resp(const f1ap_ue_context_mod_resp_t *orig);
bool eq_ue_context_mod_resp(const f1ap_ue_context_mod_resp_t *a, const f1ap_ue_context_mod_resp_t *b);
void free_ue_context_mod_resp(f1ap_ue_context_mod_resp_t *resp);

struct F1AP_F1AP_PDU *encode_ue_context_rel_req(const f1ap_ue_context_rel_req_t *msg);
bool decode_ue_context_rel_req(const struct F1AP_F1AP_PDU *pdu, f1ap_ue_context_rel_req_t *out);
f1ap_ue_context_rel_req_t cp_ue_context_rel_req(const f1ap_ue_context_rel_req_t *orig);
bool eq_ue_context_rel_req(const f1ap_ue_context_rel_req_t *a, const f1ap_ue_context_rel_req_t *b);
void free_ue_context_rel_req(f1ap_ue_context_rel_req_t *req);

struct F1AP_F1AP_PDU *encode_ue_context_rel_cmd(const f1ap_ue_context_rel_cmd_t *msg);
bool decode_ue_context_rel_cmd(const struct F1AP_F1AP_PDU *pdu, f1ap_ue_context_rel_cmd_t *out);
f1ap_ue_context_rel_cmd_t cp_ue_context_rel_cmd(const f1ap_ue_context_rel_cmd_t *orig);
bool eq_ue_context_rel_cmd(const f1ap_ue_context_rel_cmd_t *a, const f1ap_ue_context_rel_cmd_t *b);
void free_ue_context_rel_cmd(f1ap_ue_context_rel_cmd_t *cmd);

struct F1AP_F1AP_PDU *encode_ue_context_rel_cplt(const f1ap_ue_context_rel_cplt_t *msg);
bool decode_ue_context_rel_cplt(const struct F1AP_F1AP_PDU *pdu, f1ap_ue_context_rel_cplt_t *out);
f1ap_ue_context_rel_cplt_t cp_ue_context_rel_cplt(const f1ap_ue_context_rel_cplt_t *orig);
bool eq_ue_context_rel_cplt(const f1ap_ue_context_rel_cplt_t *a, const f1ap_ue_context_rel_cplt_t *b);
void free_ue_context_rel_cplt(f1ap_ue_context_rel_cplt_t *cplt);

#endif /* F1AP_UE_CONTEXT_SETUP_REQ_H_ */
