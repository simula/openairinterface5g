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
#include <stdint.h>

#include "ProtocolDiscriminator.h"
#include "SecurityHeaderType.h"
#include "NasKeySetIdentifier.h"
#include "ServiceType.h"
#include "FGCNasMessageContainer.h"
#include "MessageType.h"
#include "FGSMobileIdentity.h"

#ifndef FGS_SERVICE_REQUEST_H_
#define FGS_SERVICE_REQUEST_H_

/**
 * Message name: Service request (8.2.16 of 3GPP TS 24.501)
 * Description: The SERVICE REQUEST message is sent by the UE to the AMF
 *              in order to request the establishment of an N1 NAS signalling
 *              connection and/or to request the establishment of user-plane
 *              resources for PDU sessions which are established without
 *              user-plane resources
 * Direction: UE to network
 */

typedef struct {
  /* Mandatory fields */
  NasKeySetIdentifier naskeysetidentifier;
  ServiceType serviceType: 4;
  Stmsi5GSMobileIdentity_t fiveg_s_tmsi;
  /* Optional fields */
  FGCNasMessageContainer *fgsnasmessagecontainer;
} fgs_service_request_msg_t;

int encode_fgs_service_request(uint8_t *buffer, const fgs_service_request_msg_t *servicerequest, uint32_t len);
int decode_fgs_service_request(fgs_service_request_msg_t *sr, const uint8_t *buffer, uint32_t len);

#endif /* ! defined(FGS_SERVICE_REQUEST_H_) */
