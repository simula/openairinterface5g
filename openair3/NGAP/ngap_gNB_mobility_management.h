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

#ifndef NGAP_GNB_MOBILITY_MANAGEMENT_H_
#define NGAP_GNB_MOBILITY_MANAGEMENT_H_

#include <stdint.h>
#include "common/platform_types.h"
#include "ngap_messages_types.h"

NGAP_NGAP_PDU_t *encode_ng_handover_required(const ngap_handover_required_t *msg);
NGAP_NGAP_PDU_t *encode_ng_handover_failure(const ngap_handover_failure_t *msg);
int decode_ng_handover_request(ngap_handover_request_t *out, const NGAP_NGAP_PDU_t *pdu);
NGAP_NGAP_PDU_t *encode_ng_handover_request_ack(ngap_handover_request_ack_t *msg);
void free_ng_handover_req_ack(ngap_handover_request_ack_t *msg);
int decode_ng_handover_command(ngap_handover_command_t *msg, NGAP_NGAP_PDU_t *pdu);
void free_ng_handover_command(ngap_handover_command_t *msg);
NGAP_NGAP_PDU_t *encode_ng_handover_notify(const ngap_handover_notify_t *msg);
NGAP_NGAP_PDU_t *encode_ng_ul_ran_status_transfer(const ngap_ran_status_transfer_t *msg);
NGAP_NGAP_PDU_t *encode_ng_handover_cancel(const ngap_handover_cancel_t *msg);
int decode_ng_handover_cancel_ack(ngap_handover_cancel_ack_t *out, const NGAP_NGAP_PDU_t *pdu);

#endif /* NGAP_GNB_MOBILITY_MANAGEMENT_H_ */
