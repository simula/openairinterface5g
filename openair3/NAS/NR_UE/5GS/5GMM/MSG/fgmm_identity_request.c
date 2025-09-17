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

#include "fgmm_identity_request.h"
#include <string.h>
#include "FGSMobileIdentity.h"

int decode_fgs_identity_request(fgs_identity_request_msg_t *msg, const byte_array_t *buffer)
{
  if (buffer->len < 1) {
    PRINT_NAS_ERROR("Failed to decode NAS Identity Request: invalid buffer length %ld\n", buffer->len);
    return -1;
  }

  // Bits 3-1 (least significant 3 bits)
  msg->fgsmobileidentity = buffer->buf[0] & 0x07;

  return 1;
}
