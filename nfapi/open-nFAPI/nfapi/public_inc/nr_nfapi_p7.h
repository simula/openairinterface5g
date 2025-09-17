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
/*! \file nfapi/open-nFAPI/nfapi/public_inc/nr_nfapi_p7.h
 * \brief
 * \author Ruben S. Silva
 * \date 2024
 * \version 0.1
 * \company OpenAirInterface Software Alliance
 * \email: contact@openairinterface.org, rsilva@allbesmart.pt
 * \note
 * \warning
 */

#ifndef OPENAIRINTERFACE_NR_NFAPI_P7_H
#define OPENAIRINTERFACE_NR_NFAPI_P7_H
#include "nfapi_interface.h"
#include "nfapi_nr_interface_scf.h"

uint8_t unpack_nr_slot_indication(uint8_t **ppReadPackedMsg,
                                  uint8_t *end,
                                  nfapi_nr_slot_indication_scf_t *msg,
                                  nfapi_p7_codec_config_t *config);

void *nfapi_p7_allocate(size_t size, nfapi_p7_codec_config_t *config);

uint8_t unpack_nr_srs_indication(uint8_t **ppReadPackedMsg,
                                 uint8_t *end,
                                 nfapi_nr_srs_indication_t *pNfapiMsg,
                                 nfapi_p7_codec_config_t *config);

uint8_t pack_dl_tti_request(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config);

uint8_t pack_ul_tti_request(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config);

uint8_t pack_ul_dci_request(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config);

uint8_t pack_ue_release_request(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config);

uint8_t pack_ue_release_response(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config);

uint8_t pack_nr_slot_indication(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config);

uint8_t pack_nr_rx_data_indication(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config);

uint8_t pack_nr_crc_indication(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config);

uint8_t pack_nr_uci_indication(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config);

uint8_t pack_nr_srs_indication(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config);

uint8_t pack_nr_rach_indication(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config);

uint8_t pack_nr_dl_node_sync(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config);

uint8_t pack_nr_ul_node_sync(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config);

uint8_t pack_nr_timing_info(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config);

uint8_t unpack_nr_crc_indication(uint8_t **ppReadPackedMsg,
                                 uint8_t *end,
                                 nfapi_nr_crc_indication_t *msg,
                                 nfapi_p7_codec_config_t *config);

uint8_t unpack_nr_uci_indication(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg, nfapi_p7_codec_config_t *config);

uint8_t unpack_nr_rach_indication(uint8_t **ppReadPackedMsg,
                                  uint8_t *end,
                                  nfapi_nr_rach_indication_t *msg,
                                  nfapi_p7_codec_config_t *config);

#endif // OPENAIRINTERFACE_NR_NFAPI_P7_H
