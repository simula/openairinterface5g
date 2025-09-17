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

#include "fgmm_authentication_failure.h"
#include <string.h>
#include <arpa/inet.h> // For htons and ntohs
#include <stdlib.h> // For malloc and free
#include "fgmm_lib.h"
#include "common/utils/ds/byte_array.h"
#include "common/utils/utils.h"

#define MIN_AUTH_FAILURE_LEN 1
#define AUTH_FAILURE_PARAM_LEN 16
#define AUTH_FAIL_PARAM_IEI 0x30

/** @brief Encode Authentication Failure (8.2.4 of 3GPP TS 24.501) */
int encode_fgmm_auth_failure(byte_array_t *buffer, const fgmm_auth_failure_t *msg)
{
  if (buffer->len < MIN_AUTH_FAILURE_LEN) {
    PRINT_NAS_ERROR("Missing mandatory IE in Authentication Failure");
    return -1; // missing mandatory IE
  }
  // 5GMM cause (Mandatory)
  int encoded = encode_fgs_nas_cause(buffer, &msg->cause);
  // Authentication failure parameter (see 8.2.4.2 3GPP TS 24.501)
  if (msg->cause == Synch_failure) {
    // Authentication failure parameter shall be present with cause Synch Failure
    if (msg->authentication_failure_param.len == 0) {
      PRINT_NAS_ERROR("Authentication Failure Cause is synch failure but Authentication Failure Parameter is not available");
      return -1;
    }
    if (buffer->len - encoded < msg->authentication_failure_param.len + 2) {
      PRINT_NAS_ERROR("Invalid buffer for encoding of Authentication Failure Parameter");
      return -1;
    }
    // Encode the IEI
    buffer->buf[encoded++] = AUTH_FAIL_PARAM_IEI;
    buffer->buf[encoded++] = msg->authentication_failure_param.len;
    for (int i = 0; i < msg->authentication_failure_param.len; i++) {
      buffer->buf[encoded++] = msg->authentication_failure_param.buf[i];
    }
  }
  return encoded;
}

/** @brief Decode Authentication Failure (8.2.4 of 3GPP TS 24.501) */
int decode_fgmm_auth_failure(fgmm_auth_failure_t *msg, const byte_array_t *buffer)
{
  if (buffer->len < MIN_AUTH_FAILURE_LEN) {
    return -1; // missing mandatory IE
  }
  byte_array_t ba = *buffer; // Local copy of buffer
  // 5GMM Cause (Mandatory)
  uint32_t decoded = decode_fgs_nas_cause(&msg->cause, buffer);
  // Authentication Failure Parameters (Optional)
  if (msg->cause == Synch_failure) {
    uint8_t iei = buffer->buf[decoded++];
    if (iei == AUTH_FAIL_PARAM_IEI) {
      msg->authentication_failure_param.len = ba.buf[decoded++];
      if (msg->authentication_failure_param.len > AUTH_FAILURE_PARAM_LEN - 2) { // maximum length of contents is 14 octets
        PRINT_NAS_ERROR("Length of Authentication Failure Parameter contents is too large");
        return -1;
      }
      msg->authentication_failure_param.buf = malloc_or_fail(msg->authentication_failure_param.len);
      for (int i = 0; i < msg->authentication_failure_param.len; i++) {
        msg->authentication_failure_param.buf[i] = buffer->buf[decoded++];
      }
    }
  }
  return decoded;
}

bool eq_fgmm_auth_failure(const fgmm_auth_failure_t *a, const fgmm_auth_failure_t *b)
{
  _NAS_EQ_CHECK_INT(a->cause, b->cause);
  eq_byte_array(&a->authentication_failure_param, &b->authentication_failure_param);
  return true;
}

void free_fgmm_auth_failure(const fgmm_auth_failure_t *msg)
{
  free(msg->authentication_failure_param.buf);
}