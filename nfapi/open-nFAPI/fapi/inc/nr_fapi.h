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
/*! \file nfapi/open-nFAPI/fapi/inc/nr_fapi.h
 * \brief
 * \author Ruben S. Silva
 * \date 2024
 * \version 0.1
 * \company OpenAirInterface Software Alliance
 * \email: contact@openairinterface.org, rsilva@allbesmart.pt
 * \note
 * \warning
 */

#ifndef OPENAIRINTERFACE_NR_FAPI_H
#define OPENAIRINTERFACE_NR_FAPI_H

#include "stdint.h"
#include "string.h"
#include "nfapi_interface.h"
#include "nfapi_nr_interface.h"
#include "nfapi_nr_interface_scf.h"
#include "nfapi.h"
#include "nfapi/oai_integration/vendor_ext.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include "assertions.h"

typedef struct {
  uint8_t num_msg;
  uint8_t opaque_handle;
  uint16_t message_id;
  uint32_t message_length;
} fapi_message_header_t;

int fapi_nr_p5_message_header_unpack(uint8_t **pMessageBuf,
                                     uint32_t messageBufLen,
                                     void *pUnpackedBuf,
                                     uint32_t unpackedBufLen,
                                     nfapi_p4_p5_codec_config_t *config);

int fapi_nr_p5_message_pack(void *pMessageBuf,
                            uint32_t messageBufLen,
                            void *pPackedBuf,
                            uint32_t packedBufLen,
                            nfapi_p4_p5_codec_config_t *config);

int fapi_nr_p5_message_unpack(void *pMessageBuf,
                              uint32_t messageBufLen,
                              void *pUnpackedBuf,
                              uint32_t unpackedBufLen,
                              nfapi_p4_p5_codec_config_t *config);

int check_nr_fapi_unpack_length(nfapi_nr_phy_msg_type_e msgId, uint32_t unpackedBufLen);
#endif // OPENAIRINTERFACE_NR_FAPI_H
