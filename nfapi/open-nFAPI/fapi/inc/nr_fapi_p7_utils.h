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
/*! \file nfapi/open-nFAPI/fapi/inc/p5/nr_fapi_p5_utils.h
 * \brief
 * \author Ruben S. Silva
 * \date 2024
 * \version 0.1
 * \company OpenAirInterface Software Alliance
 * \email: contact@openairinterface.org, rsilva@allbesmart.pt
 * \note
 * \warning
 */

#ifndef OPENAIRINTERFACE_NR_FAPI_P7_UTILS_H
#define OPENAIRINTERFACE_NR_FAPI_P7_UTILS_H
#include "stdio.h"
#include "stdint.h"
#include "nfapi/open-nFAPI/fapi/inc/nr_fapi.h"

bool eq_dl_tti_request(const nfapi_nr_dl_tti_request_t *a, const nfapi_nr_dl_tti_request_t *b);
bool eq_ul_tti_request(const nfapi_nr_ul_tti_request_t *a, const nfapi_nr_ul_tti_request_t *b);
bool eq_slot_indication(const nfapi_nr_slot_indication_scf_t *a, const nfapi_nr_slot_indication_scf_t *b);
bool eq_ul_dci_request(const nfapi_nr_ul_dci_request_t *a, const nfapi_nr_ul_dci_request_t *b);
bool eq_tx_data_request(const nfapi_nr_tx_data_request_t *a, const nfapi_nr_tx_data_request_t *b);
bool eq_rx_data_indication(const nfapi_nr_rx_data_indication_t *a, const nfapi_nr_rx_data_indication_t *b);
bool eq_crc_indication(const nfapi_nr_crc_indication_t *a, const nfapi_nr_crc_indication_t *b);
bool eq_uci_indication(const nfapi_nr_uci_indication_t *a, const nfapi_nr_uci_indication_t *b);
bool eq_srs_indication(const nfapi_nr_srs_indication_t *a, const nfapi_nr_srs_indication_t *b);
bool eq_rach_indication(const nfapi_nr_rach_indication_t *a, const nfapi_nr_rach_indication_t *b);

void free_dl_tti_request(nfapi_nr_dl_tti_request_t *msg);
void free_ul_tti_request(nfapi_nr_ul_tti_request_t *msg);
void free_slot_indication(nfapi_nr_slot_indication_scf_t *msg);
void free_ul_dci_request(nfapi_nr_ul_dci_request_t *msg);
void free_tx_data_request(nfapi_nr_tx_data_request_t *msg);
void free_rx_data_indication(nfapi_nr_rx_data_indication_t *msg);
void free_crc_indication(nfapi_nr_crc_indication_t *msg);
void free_uci_indication(nfapi_nr_uci_indication_t *msg);
void free_srs_indication(nfapi_nr_srs_indication_t *msg);
void free_rach_indication(nfapi_nr_rach_indication_t *msg);

void copy_dl_tti_request(const nfapi_nr_dl_tti_request_t *src, nfapi_nr_dl_tti_request_t *dst);
void copy_ul_tti_request(const nfapi_nr_ul_tti_request_t *src, nfapi_nr_ul_tti_request_t *dst);
void copy_slot_indication(const nfapi_nr_slot_indication_scf_t *src, nfapi_nr_slot_indication_scf_t *dst);
void copy_ul_dci_request(const nfapi_nr_ul_dci_request_t *src, nfapi_nr_ul_dci_request_t *dst);
void copy_tx_data_request(const nfapi_nr_tx_data_request_t *src, nfapi_nr_tx_data_request_t *dst);
void copy_rx_data_indication(const nfapi_nr_rx_data_indication_t *src, nfapi_nr_rx_data_indication_t *dst);
void copy_crc_indication(const nfapi_nr_crc_indication_t *src, nfapi_nr_crc_indication_t *dst);
void copy_uci_indication(const nfapi_nr_uci_indication_t *src, nfapi_nr_uci_indication_t *dst);
void copy_srs_indication(const nfapi_nr_srs_indication_t *src, nfapi_nr_srs_indication_t *dst);
void copy_rach_indication(const nfapi_nr_rach_indication_t *src, nfapi_nr_rach_indication_t *dst);


size_t get_tx_data_request_size(const nfapi_nr_tx_data_request_t *msg);
size_t get_rx_data_indication_size(const nfapi_nr_rx_data_indication_t *msg);
size_t get_crc_indication_size(const nfapi_nr_crc_indication_t *msg);
size_t get_uci_indication_size(const nfapi_nr_uci_indication_t *msg);
size_t get_srs_indication_size(const nfapi_nr_srs_indication_t *msg);
size_t get_rach_indication_size(nfapi_nr_rach_indication_t *msg);

void dump_dl_tti_request(const nfapi_nr_dl_tti_request_t *msg);
void dump_ul_tti_request(const nfapi_nr_ul_tti_request_t *msg);
void dump_slot_indication(const nfapi_nr_slot_indication_scf_t *msg);
void dump_ul_dci_request(const nfapi_nr_ul_dci_request_t *msg);
void dump_tx_data_request(const nfapi_nr_tx_data_request_t *msg);
void dump_rx_data_indication(const nfapi_nr_rx_data_indication_t *msg);
void dump_crc_indication(const nfapi_nr_crc_indication_t *msg);
void dump_uci_indication(const nfapi_nr_uci_indication_t *msg);
void dump_srs_indication(const nfapi_nr_srs_indication_t *msg);
void dump_rach_indication(const nfapi_nr_rach_indication_t *msg);

#endif // OPENAIRINTERFACE_NR_FAPI_P7_UTILS_H
