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

#include "fgmm_authentication_reject.h"
#include <string.h>
#include <arpa/inet.h> // For htons and ntohs
#include <stdlib.h> // For malloc and free
#include "fgmm_lib.h"

#define MIN_AUTH_REJECT_LEN 3

/** @brief Encode Authentication Reject (8.2.5 of 3GPP TS 24.501) */
int encode_fgmm_auth_reject(byte_array_t *buffer, const fgmm_auth_reject_msg_t *msg)
{
  uint32_t encoded = 0;

  const byte_array_t *eap_msg = &msg->eap_msg;
  if (eap_msg->len > 0) {
    return encode_eap_msg_ie(buffer, eap_msg);
  }
  return encoded;
}

/** @brief Decode Authentication Reject (8.2.5 of 3GPP TS 24.501)
 *         the message contains only optional IEIs (EAP MSG) */
int decode_fgmm_auth_reject(fgmm_auth_reject_msg_t *msg, const byte_array_t *buffer)
{
  if (buffer->len < MIN_AUTH_REJECT_LEN) {
    // No optional IEIs present
    return 0;
  }
  byte_array_t ba = *buffer; // Local copy of buffer
  uint32_t decoded = 0;
  // Decode the IEI
  uint8_t iei = buffer->buf[decoded++];
  UPDATE_BYTE_ARRAY(ba, decoded);
  if (iei == IEI_EAPMSG) {
    return (decoded + decode_eap_msg_ie(&msg->eap_msg, &ba));
  }
  PRINT_NAS_ERROR("Expected EAP MSG but it is not present");
  return -1;
}

bool eq_auth_reject(fgmm_auth_reject_msg_t *a, fgmm_auth_reject_msg_t *b)
{
  _NAS_EQ_CHECK_LONG(a->eap_msg.len, b->eap_msg.len);
  if (a->eap_msg.len > 0) {
    return !memcmp(a->eap_msg.buf, b->eap_msg.buf, a->eap_msg.len);
  }
  return true;
}
