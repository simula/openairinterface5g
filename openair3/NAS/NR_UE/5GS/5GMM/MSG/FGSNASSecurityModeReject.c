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

#include <stdint.h>
#include "FGSNASSecurityModeReject.h"
#include "common/utils/ds/byte_array.h"
#include "fgmm_lib.h"

int encode_fgs_security_mode_reject(byte_array_t *buffer, const fgs_security_mode_reject_msg *msg)
{
  if (buffer->len < 1)
    return -1; // nothing to encode

  // 5GMM cause (Mandatory)
  return encode_fgs_nas_cause(buffer, &msg->cause);
}

/** @brief Decode Security Mode Reject (8.2.18 of 3GPP TS 24.501) */
int decode_fgs_security_mode_reject(fgs_security_mode_reject_msg *msg, const byte_array_t *buffer)
{
  if (buffer->len < 1)
    return -1; // nothing to decode

  // 5GMM cause (Mandatory)
  return decode_fgs_nas_cause(&msg->cause, buffer);
}

bool eq_sec_mode_reject(const fgs_security_mode_reject_msg *a, const fgs_security_mode_reject_msg *b)
{
  _NAS_EQ_CHECK_INT(a->cause, b->cause);
  return true;
}
