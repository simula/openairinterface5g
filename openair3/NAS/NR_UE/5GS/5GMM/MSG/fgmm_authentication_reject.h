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

#ifndef FGMM_AUTH_REJECT_H
#define FGMM_AUTH_REJECT_H

#include <stdint.h>
#include "fgmm_lib.h"
#include "common/utils/ds/byte_array.h"

typedef struct {
  // EAP Message (Optional)
  byte_array_t eap_msg;
} fgmm_auth_reject_msg_t;

int decode_fgmm_auth_reject(fgmm_auth_reject_msg_t *msg, const byte_array_t *buffer);
int encode_fgmm_auth_reject(byte_array_t *buffer, const fgmm_auth_reject_msg_t *msg);
bool eq_auth_reject(fgmm_auth_reject_msg_t *a, fgmm_auth_reject_msg_t *b);

#endif /* FGMM_AUTH_REJECT_H */
