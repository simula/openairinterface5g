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

/*! \file fapi/oai-integration/fapi_vnf_p7.h
* \brief Header file for fapi_vnf_p7.c
* \author Ruben S. Silva
* \date 2023
* \version 0.1
* \company OpenAirInterface Software Alliance
* \email: contact@openairinterface.org, rsilva@allbesmart.pt
* \note
* \warning
 */

#ifndef OPENAIRINTERFACE_FAPI_VNF_P7_H
#define OPENAIRINTERFACE_FAPI_VNF_P7_H

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "nfapi_interface.h"
#include "nfapi_nr_interface_scf.h"
#include "nfapi_vnf_interface.h"
#include "fapi_nvIPC.h"
#include "openair2/PHY_INTERFACE/queue_t.h"
#include "nfapi/open-nFAPI/vnf/inc/vnf_p7.h"
uint8_t aerial_unpack_nr_rx_data_indication(uint8_t **ppReadPackedMsg,
                                            uint8_t *end,
                                            uint8_t **pDataMsg,
                                            uint8_t *data_end,
                                            nfapi_nr_rx_data_indication_t *msg,
                                            nfapi_p7_codec_config_t *config);

uint8_t aerial_unpack_nr_srs_indication(uint8_t **ppReadPackedMsg,
                                        uint8_t *end,
                                        uint8_t **pDataMsg,
                                        uint8_t *data_end,
                                        void *msg,
                                        nfapi_p7_codec_config_t *config);
bool aerial_nr_send_p7_message(vnf_p7_t *vnf_p7, nfapi_nr_p7_message_header_t *header);
#endif // OPENAIRINTERFACE_FAPI_VNF_P7_H
