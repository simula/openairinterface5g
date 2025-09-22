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

/*! \file ngap_common.h
 * \brief ngap procedures for both gNB and AMF
 * \author Yoshio INOUE, Masayuki HARADA
 * \email yoshio.inoue@fujitsu.com,masayuki.harada@fujitsu.com (yoshio.inoue%40fujitsu.com%2cmasayuki.harada%40fujitsu.com)
 * \date 2020
 * \version 0.1
 */

/** @defgroup _ngap_impl_ NGAP Layer Reference Implementation
 * @ingroup _ref_implementation_
 * @{
 */

 
#ifndef NGAP_COMMON_H_
#define NGAP_COMMON_H_

#include "common/utils/LOG/log.h"
#include "oai_asn1.h"
#include "ngap_msg_includes.h"
#include "openair2/COMMON/ngap_messages_types.h"

/* Checking version of ASN1C compiler */
#if (ASN1C_ENVIRONMENT_VERSION < ASN1C_MINIMUM_VERSION)
# error "You are compiling ngap with the wrong version of ASN1C"
#endif

#define NGAP_UE_ID_FMT  "0x%06"PRIX32

# include "common/utils/LOG/log.h"
# include "ngap_gNB_default_values.h"
# define NGAP_ERROR(x, args...) LOG_E(NGAP, x, ##args)
# define NGAP_WARN(x, args...)  LOG_W(NGAP, x, ##args)
# define NGAP_TRAF(x, args...)  LOG_I(NGAP, x, ##args)
# define NGAP_INFO(x, args...) LOG_I(NGAP, x, ##args)
# define NGAP_DEBUG(x, args...) LOG_D(NGAP, x, ##args)

#define NGAP_FIND_PROTOCOLIE_BY_ID(IE_TYPE, ie, container, IE_ID, mandatory)                                                            \
  do {                                                                                                                                  \
    IE_TYPE **ptr;                                                                                                                      \
    ie = NULL;                                                                                                                          \
    for (ptr = container->protocolIEs.list.array; ptr < &container->protocolIEs.list.array[container->protocolIEs.list.count]; ptr++) { \
      if ((*ptr)->id == IE_ID) {                                                                                                        \
        ie = *ptr;                                                                                                                      \
        break;                                                                                                                          \
      }                                                                                                                                 \
    }                                                                                                                                   \
    if (ie == NULL) {                                                                                                                   \
      if (mandatory) {                                                                                                                  \
        AssertFatal(NGAP, "NGAP_FIND_PROTOCOLIE_BY_ID ie is NULL (searching for ie: %ld)\n", IE_ID);                                    \
      } else {                                                                                                                          \
        NGAP_DEBUG("NGAP_FIND_PROTOCOLIE_BY_ID ie is NULL (searching for ie: %ld)\n", IE_ID);                                            \
      }                                                                                                                                 \
    }                                                                                                                                   \
  } while (0);                                                                                                                          \
  if (mandatory && !ie)                                                                                                                 \
  return -1

/** \brief Function callback prototype.
 **/
typedef int (*ngap_message_decoded_callback)(sctp_assoc_t assoc_id, uint32_t stream, NGAP_NGAP_PDU_t *pdu);

void encode_ngap_cause(NGAP_Cause_t *out, const ngap_cause_t *in);
nr_guami_t decode_ngap_guami(const NGAP_GUAMI_t *in);
ngap_cause_t decode_ngap_cause(const NGAP_Cause_t *in);
ngap_ambr_t decode_ngap_UEAggregateMaximumBitRate(const NGAP_UEAggregateMaximumBitRate_t *in);
nssai_t decode_ngap_nssai(const NGAP_S_NSSAI_t *in);
ngap_security_capabilities_t decode_ngap_security_capabilities(const NGAP_UESecurityCapabilities_t *in);
ngap_mobility_restriction_t decode_ngap_mobility_restriction(const NGAP_MobilityRestrictionList_t *in);
void encode_ngap_target_id(NGAP_HandoverRequiredIEs_t *out, const target_ran_node_id_t *in);
void encode_ngap_nr_cgi(NGAP_NR_CGI_t *out, const plmn_id_t *plmn, const uint32_t cell_id);
pdusession_level_qos_parameter_t fill_qos(uint8_t qfi, const NGAP_QosFlowLevelQosParameters_t *params);
void *decode_pdusession_transfer(const asn_TYPE_descriptor_t *td, const OCTET_STRING_t buf);
bool decodePDUSessionResourceSetup(pdusession_transfer_t *out, const OCTET_STRING_t in);

/** @}*/

#endif /* NGAP_COMMON_H_ */
