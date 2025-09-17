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

#include "e1ap_lib_common.h"
#include "e1ap_lib_includes.h"
#include "e1ap_messages_types.h"

E1AP_Cause_t e1_encode_cause_ie(const e1ap_cause_t *cause)
{
  E1AP_Cause_t ie;
  switch (cause->type) {
    case E1AP_CAUSE_RADIO_NETWORK:
      ie.present = E1AP_Cause_PR_radioNetwork;
      ie.choice.radioNetwork = cause->value;
      break;
    case E1AP_CAUSE_TRANSPORT:
      ie.present = E1AP_Cause_PR_transport;
      ie.choice.transport = cause->value;
      break;
    case E1AP_CAUSE_PROTOCOL:
      ie.present = E1AP_Cause_PR_protocol;
      ie.choice.protocol = cause->value;
      break;
    case E1AP_CAUSE_MISC:
      ie.present = E1AP_Cause_PR_misc;
      ie.choice.misc = cause->value;
      break;
    default:
      ie.present = E1AP_Cause_PR_NOTHING;
      PRINT_ERROR("encode_e1ap_cause_ie: no cause value provided\n");
      break;
  }
  return ie;
}

e1ap_cause_t e1_decode_cause_ie(const E1AP_Cause_t *ie)
{
  e1ap_cause_t cause;
  // Decode the 'choice' field based on the 'present' value
  switch (ie->present) {
    case E1AP_Cause_PR_radioNetwork:
      cause.value = ie->choice.radioNetwork;
      cause.type = E1AP_CAUSE_RADIO_NETWORK;
      break;
    case E1AP_Cause_PR_transport:
      cause.value = ie->choice.transport;
      cause.type = E1AP_CAUSE_TRANSPORT;
      break;
    case E1AP_Cause_PR_protocol:
      cause.value = ie->choice.protocol;
      cause.type = E1AP_CAUSE_PROTOCOL;
      break;
    case E1AP_Cause_PR_misc:
      cause.value = ie->choice.misc;
      cause.type = E1AP_CAUSE_MISC;
      break;
    default:
      cause.type = E1AP_CAUSE_NOTHING;
      PRINT_ERROR("decode_e1ap_cause_ie: no cause value provided\n");
      break;
  }
  return cause;
}
