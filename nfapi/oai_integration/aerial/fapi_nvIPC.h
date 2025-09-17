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

/*! \file fapi/oai-integration/fapi_nvIPC.h
* \brief Header file for fapi_nvIPC.c
* \author Ruben S. Silva
* \date 2023
* \version 0.1
* \company OpenAirInterface Software Alliance
* \email: contact@openairinterface.org, rsilva@allbesmart.pt
* \note
* \warning
 */
#ifndef OPENAIRINTERFACE_FAPI_NVIPC_H
#define OPENAIRINTERFACE_FAPI_NVIPC_H

#include "nv_ipc.h"
#include "nv_ipc_utils.h"
#include "nvlog.h"
#include "nfapi/open-nFAPI/vnf/public_inc/nfapi_vnf_interface.h"
#include "vnf.h"
#include "debug.h"

#include "openair2/LAYER2/NR_MAC_gNB/nr_mac_gNB.h"

typedef struct {
  uint8_t num_msg;
  uint8_t opaque_handle;
  uint16_t message_id;
  uint32_t message_length;
} fapi_phy_api_msg;

bool aerial_nr_send_p5_message(vnf_t *vnf, uint16_t p5_idx, nfapi_nr_p4_p5_message_header_t *msg, uint32_t msg_len);
bool aerial_send_P5_msg(void *packedBuf, uint32_t packedMsgLength, nfapi_nr_p4_p5_message_header_t *header);
bool aerial_send_P7_msg(void *packedBuf, uint32_t packedMsgLength, nfapi_nr_p7_message_header_t *header);
bool aerial_send_P7_msg_with_data(void *packedBuf,
                                 uint32_t packedMsgLength,
                                 void *dataBuf,
                                 uint32_t dataLength,
                                 nfapi_nr_p7_message_header_t *header);
void set_config(nfapi_vnf_config_t *conf);
int nvIPC_Init(nvipc_params_t nvipc_params_s);
int oai_fapi_send_end_request(int cell_id, uint32_t frame, uint32_t slot);
int oai_fapi_ul_tti_req(nfapi_nr_ul_tti_request_t *ul_tti_req);
int oai_fapi_ul_dci_req(nfapi_nr_ul_dci_request_t *ul_dci_req);
int oai_fapi_tx_data_req(nfapi_nr_tx_data_request_t *tx_data_req);
int oai_fapi_dl_tti_req(nfapi_nr_dl_tti_request_t *dl_config_req);
#endif // OPENAIRINTERFACE_FAPI_NVIPC_H
