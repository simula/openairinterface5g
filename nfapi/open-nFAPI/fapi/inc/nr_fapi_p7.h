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
#ifndef OPENAIRINTERFACE_NR_FAPI_P7_H
#define OPENAIRINTERFACE_NR_FAPI_P7_H

int fapi_nr_p7_message_pack(void *pMessageBuf, void *pPackedBuf, uint32_t packedBufLen, nfapi_p7_codec_config_t *config);

bool fapi_nr_p7_message_unpack(void *pMessageBuf,
                              uint32_t messageBufLen,
                              void *pUnpackedBuf,
                              uint32_t unpackedBufLen,
                              nfapi_p7_codec_config_t *config);

uint8_t pack_dl_tti_request(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config);
uint8_t unpack_dl_tti_request(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg, nfapi_p7_codec_config_t *config);
uint8_t pack_ul_tti_request(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config);
uint8_t unpack_ul_tti_request(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg, nfapi_p7_codec_config_t *config);
uint8_t unpack_nr_slot_indication(uint8_t **ppReadPackedMsg,
                                  uint8_t *end,
                                  nfapi_nr_slot_indication_scf_t *msg,
                                  nfapi_p7_codec_config_t *config);
uint8_t pack_nr_slot_indication(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config);
uint8_t pack_ul_dci_request(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config);
uint8_t unpack_ul_dci_request(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg, nfapi_p7_codec_config_t *config);
uint8_t pack_tx_data_request(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config);
uint8_t unpack_tx_data_request(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg, nfapi_p7_codec_config_t *config);
uint8_t pack_nr_rx_data_indication(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config);
uint8_t unpack_nr_rx_data_indication(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg, nfapi_p7_codec_config_t *config);
uint8_t pack_nr_crc_indication(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config);
uint8_t unpack_nr_crc_indication(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg, nfapi_p7_codec_config_t *config);
uint8_t pack_nr_uci_indication(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config);
uint8_t unpack_nr_uci_indication(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg, nfapi_p7_codec_config_t *config);
uint8_t pack_nr_srs_indication(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config);
uint8_t unpack_nr_srs_indication(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg, nfapi_p7_codec_config_t *config);
uint8_t unpack_nr_srs_report_tlv_value(nfapi_srs_report_tlv_t *report_tlv, uint8_t **ppReadPackedMsg, uint8_t *end);
uint8_t pack_nr_rach_indication(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config);
uint8_t unpack_nr_rach_indication(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg, nfapi_p7_codec_config_t *config);
#endif // OPENAIRINTERFACE_NR_FAPI_P7_H
