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

#ifndef FGS_DEREGISTRATION_REQUEST_UE_ORIGINATING_H_
#define FGS_DEREGISTRATION_REQUEST_UE_ORIGINATING_H_

#include <stdint.h>
#include "FGSDeregistrationType.h"
#include "FGSMobileIdentity.h"
#include "MessageType.h"
#include "NasKeySetIdentifier.h"
#include "SecurityHeaderType.h"

/*
 * Message name: De-registration request (UE originating de-registration)
 * Description: This message is sent by the UE to the AMF. See TS24.501 table 8.2.12.1.1.
 * Significance: dual
 * Direction: UE to network
 */

typedef struct fgs_deregistration_request_ue_originating_msg_tag {
  /* Mandatory fields */
  FGSDeregistrationType deregistrationtype;
  NasKeySetIdentifier naskeysetidentifier;
  FGSMobileIdentity fgsmobileidentity;
} fgs_deregistration_request_ue_originating_msg;

int encode_fgs_deregistration_request_ue_originating(const fgs_deregistration_request_ue_originating_msg *registrationrequest,
                                                     uint8_t *buffer,
                                                     uint32_t len);

#endif /* ! defined(REGISTRATION_REQUEST_H_) */
