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

#ifndef F1AP_RRC_MESSAGE_TRANSFER_H_
#define F1AP_RRC_MESSAGE_TRANSFER_H_

#include <stdbool.h>
#include "f1ap_messages_types.h"

struct F1AP_F1AP_PDU;

struct F1AP_F1AP_PDU *encode_initial_ul_rrc_message_transfer(const f1ap_initial_ul_rrc_message_t *msg);
bool decode_initial_ul_rrc_message_transfer(const struct F1AP_F1AP_PDU *pdu, f1ap_initial_ul_rrc_message_t *out);
f1ap_initial_ul_rrc_message_t cp_initial_ul_rrc_message_transfer(const f1ap_initial_ul_rrc_message_t *msg);
bool eq_initial_ul_rrc_message_transfer(const f1ap_initial_ul_rrc_message_t *a, const f1ap_initial_ul_rrc_message_t *b);
void free_initial_ul_rrc_message_transfer(const f1ap_initial_ul_rrc_message_t *msg);

/* DL RRC Message transfer */
struct F1AP_F1AP_PDU *encode_dl_rrc_message_transfer(const f1ap_dl_rrc_message_t *msg);
bool decode_dl_rrc_message_transfer(const struct F1AP_F1AP_PDU *pdu, f1ap_dl_rrc_message_t *out);
f1ap_dl_rrc_message_t cp_dl_rrc_message_transfer(const f1ap_dl_rrc_message_t *msg);
bool eq_dl_rrc_message_transfer(const f1ap_dl_rrc_message_t *a, const f1ap_dl_rrc_message_t *b);
void free_dl_rrc_message_transfer(const f1ap_dl_rrc_message_t *msg);

/* UL RRC Message transfer */
struct F1AP_F1AP_PDU *encode_ul_rrc_message_transfer(const f1ap_ul_rrc_message_t *msg);
bool decode_ul_rrc_message_transfer(const struct F1AP_F1AP_PDU *pdu, f1ap_ul_rrc_message_t *out);
f1ap_ul_rrc_message_t cp_ul_rrc_message_transfer(const f1ap_ul_rrc_message_t *msg);
bool eq_ul_rrc_message_transfer(const f1ap_ul_rrc_message_t *a, const f1ap_ul_rrc_message_t *b);
void free_ul_rrc_message_transfer(const f1ap_ul_rrc_message_t *msg);

#endif /* F1AP_RRC_MESSAGE_TRANSFER_H_ */
