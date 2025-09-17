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

#ifndef FIVEG_PLATFORM_TYPES_H__
#define FIVEG_PLATFORM_TYPES_H__

#include <stdint.h>

typedef struct plmn_id_s {
  uint16_t mcc;
  uint16_t mnc;
  uint8_t mnc_digit_length;
} plmn_id_t;

typedef struct nssai_s {
  uint8_t sst;
  uint32_t sd;
} nssai_t;

// Globally Unique AMF Identifier
typedef struct nr_guami_s {
  uint16_t mcc;
  uint16_t mnc;
  uint8_t mnc_len;
  uint8_t amf_region_id;
  uint16_t amf_set_id;
  uint8_t amf_pointer;
} nr_guami_t;

typedef enum {
  PDUSessionType_ipv4 = 0,
  PDUSessionType_ipv6 = 1,
  PDUSessionType_ipv4v6 = 2,
  PDUSessionType_ethernet = 3,
  PDUSessionType_unstructured = 4
} pdu_session_type_t;

typedef enum { NON_DYNAMIC, DYNAMIC } fiveQI_t;

#endif
