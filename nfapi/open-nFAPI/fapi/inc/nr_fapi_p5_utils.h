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

#ifndef OPENAIRINTERFACE_NR_FAPI_P5_UTILS_H
#define OPENAIRINTERFACE_NR_FAPI_P5_UTILS_H
#include "stdio.h"
#include "stdint.h"
#include "nfapi/open-nFAPI/fapi/inc/nr_fapi.h"
#include "nfapi/oai_integration/vendor_ext.h"

bool eq_param_request(const nfapi_nr_param_request_scf_t *unpacked_req, const nfapi_nr_param_request_scf_t *req);
bool eq_param_response(const nfapi_nr_param_response_scf_t *unpacked_req, const nfapi_nr_param_response_scf_t *req);
bool eq_config_request(const nfapi_nr_config_request_scf_t *unpacked_req, const nfapi_nr_config_request_scf_t *req);
bool eq_config_response(const nfapi_nr_config_response_scf_t *unpacked_req, const nfapi_nr_config_response_scf_t *req);
bool eq_start_request(const nfapi_nr_start_request_scf_t *unpacked_req, const nfapi_nr_start_request_scf_t *req);
bool eq_start_response(const nfapi_nr_start_response_scf_t *unpacked_req, const nfapi_nr_start_response_scf_t *req);
bool eq_stop_request(const nfapi_nr_stop_request_scf_t *unpacked_req, const nfapi_nr_stop_request_scf_t *req);
bool eq_stop_indication(const nfapi_nr_stop_indication_scf_t *unpacked_req, const nfapi_nr_stop_indication_scf_t *req);
bool eq_error_indication(const nfapi_nr_error_indication_scf_t *unpacked_req, const nfapi_nr_error_indication_scf_t *req);

void free_param_request(nfapi_nr_param_request_scf_t *msg);
void free_param_response(nfapi_nr_param_response_scf_t *msg);
void free_config_request(nfapi_nr_config_request_scf_t *msg);
void free_config_response(nfapi_nr_config_response_scf_t *msg);
void free_start_request(nfapi_nr_start_request_scf_t *msg);
void free_start_response(nfapi_nr_start_response_scf_t *msg);
void free_stop_request(nfapi_nr_stop_request_scf_t *msg);
void free_stop_indication(nfapi_nr_stop_indication_scf_t *msg);
void free_error_indication(nfapi_nr_error_indication_scf_t *msg);

void copy_param_request(const nfapi_nr_param_request_scf_t *src, nfapi_nr_param_request_scf_t *dst);
void copy_param_response(const nfapi_nr_param_response_scf_t *src, nfapi_nr_param_response_scf_t *dst);
void copy_config_request(const nfapi_nr_config_request_scf_t *src, nfapi_nr_config_request_scf_t *dst);
void copy_config_response(const nfapi_nr_config_response_scf_t *src, nfapi_nr_config_response_scf_t *dst);
void copy_start_request(const nfapi_nr_start_request_scf_t *src, nfapi_nr_start_request_scf_t *dst);
void copy_start_response(const nfapi_nr_start_response_scf_t *src, nfapi_nr_start_response_scf_t *dst);
void copy_stop_request(const nfapi_nr_stop_request_scf_t *src, nfapi_nr_stop_request_scf_t *dst);
void copy_stop_indication(const nfapi_nr_stop_indication_scf_t *src, nfapi_nr_stop_indication_scf_t *dst);
void copy_error_indication(const nfapi_nr_error_indication_scf_t *src, nfapi_nr_error_indication_scf_t *dst);

void dump_param_request(const nfapi_nr_param_request_scf_t *msg);
void dump_param_response(const nfapi_nr_param_response_scf_t *msg);
void dump_config_request(const nfapi_nr_config_request_scf_t *msg);
void dump_config_response(const nfapi_nr_config_response_scf_t *msg);
void dump_start_request(const nfapi_nr_start_request_scf_t *msg);
void dump_start_response(const nfapi_nr_start_response_scf_t *msg);
void dump_stop_request(const nfapi_nr_stop_request_scf_t *msg);
void dump_stop_indication(const nfapi_nr_stop_indication_scf_t *msg);
char* error_ind_code_to_str(nfapi_nr_phy_notifications_errors_e error_code);
void dump_error_indication(const nfapi_nr_error_indication_scf_t *msg);


#endif // OPENAIRINTERFACE_NR_FAPI_P5_UTILS_H
