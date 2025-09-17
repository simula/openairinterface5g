/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1 (the "License"); you may not use this file
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

#ifndef FGS_service_reject_H
#define FGS_service_reject_H

#include <stdint.h>
#include "fgmm_lib.h"

typedef struct {
  // 5GMM cause (Mandatory)
  cause_id_t cause;
  // T3346 Value (Optional)
  gprs_timer_t *t3446;
  // PDU Session Status (Optional)
  uint8_t psi_status[MAX_NUM_PSI];
  bool has_psi_status;
  // T3448 Value (Optional)
  gprs_timer_t *t3448;
  // EAP message (optional)
  byte_array_t *eap_msg;
} fgs_service_reject_msg_t;

int decode_fgs_service_reject(fgs_service_reject_msg_t *msg, const byte_array_t *buffer);
int encode_fgs_service_reject(byte_array_t *buffer, const fgs_service_reject_msg_t *msg);
void free_fgs_service_reject(fgs_service_reject_msg_t *msg);
bool eq_service_reject(const fgs_service_reject_msg_t *a, const fgs_service_reject_msg_t *b);

#endif /* FGS_service_reject_H */
