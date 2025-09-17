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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "TLVEncoder.h"
#include "TLVDecoder.h"
#include "fgs_service_request.h"
#include "FGSMobileIdentity.h"
#include "NasKeySetIdentifier.h"
#include "ServiceType.h"

#define LEN_FGS_MOBILE_ID_CONTENTS_SIZE 2 // octets
#define MIN_LEN_FGS_SERVICE_REQUEST 10 // octets
#define IEI_NULL 0x00

/**
 * @brief Encode 5GMM NAS Service Request message (8.2.16 of 3GPP TS 24.501)
 */
int encode_fgs_service_request(uint8_t *buffer, const fgs_service_request_msg_t *service_request, uint32_t len)
{
  // Return if buffer is shorter than min length
  if (len < MIN_LEN_FGS_SERVICE_REQUEST)
    return -1;

  int encoded = 0;

  // ngKSI + Service type (1 octet) (M)
  *buffer = ((encode_nas_key_set_identifier(&service_request->naskeysetidentifier, IEI_NULL) & 0x0f) << 4)
            | (service_request->serviceType & 0x0f);
  encoded++;
  len -= encoded;

  // 5GS Mobile Identity (M) type 5G-S-TMSI (9 octets)
  encoded += LEN_FGS_MOBILE_ID_CONTENTS_SIZE; // skip "Length of 5GS mobile identity contents" IE
  len -= encoded;
  encoded += encode_stmsi_5gs_mobile_identity(buffer + encoded, &service_request->fiveg_s_tmsi, len);
  // encode length of 5GS mobile identity contents
  uint16_t tmp = htons(encoded - LEN_FGS_MOBILE_ID_CONTENTS_SIZE - 1);
  memcpy(buffer + 1, &tmp, sizeof(tmp));

  return encoded;
}

/**
 * @brief Decode 5GMM NAS Service Request message (8.2.16 of 3GPP TS 24.501)
 */
int decode_fgs_service_request(fgs_service_request_msg_t *sr, const uint8_t *buffer, uint32_t len)
{
  uint32_t decoded = 0;
  int decoded_rc = 0;

  // Service type (1/2 octet) (M)
  sr->serviceType = *buffer & 0x0f;
  // KSI (1/2 octet) (M)
  if ((decoded_rc = decode_nas_key_set_identifier(&sr->naskeysetidentifier, IEI_NULL, *buffer >> 4)) < 0) {
    return decoded_rc;
  }
  decoded++;

  // 5GS Mobile Identity (M) type 5G-S-TMSI (9 octets) (M)
  decoded += LEN_FGS_MOBILE_ID_CONTENTS_SIZE; // skip "Length of 5GS mobile identity contents" IE
  if ((decoded_rc = decode_stmsi_5gs_mobile_identity(&sr->fiveg_s_tmsi, buffer + decoded, len - decoded)) < 0) {
    return decoded_rc;
  }
  decoded += decoded_rc;

  return decoded;
}
