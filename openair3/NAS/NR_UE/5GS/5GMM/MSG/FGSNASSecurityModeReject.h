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

#ifndef FGS_NAS_security_mode_reject_H_
#define FGS_NAS_security_mode_reject_H_

#include <stdint.h>
#include "fgmm_lib.h"

/*
 * Message name: security mode reject
 * Description: The SECURITY MODE REJECT message is sent by the UE to the AMF to
 * indicate that the corresponding security mode command has been rejected.
 * See 3GPP TS 24.501 table 8.2.27.1.1
 *
 * Significance: dual
 * Direction: UE to AMF
 */

typedef struct {
  // 5GMM cause (Mandatory)
  cause_id_t cause;
} fgs_security_mode_reject_msg;

int encode_fgs_security_mode_reject(byte_array_t *buffer, const fgs_security_mode_reject_msg *msg);
int decode_fgs_security_mode_reject(fgs_security_mode_reject_msg *msg, const byte_array_t *buffer);
bool eq_sec_mode_reject(const fgs_security_mode_reject_msg *a, const fgs_security_mode_reject_msg *b);

#endif /* ! defined(FGS_NAS_security_mode_reject_H_) */
