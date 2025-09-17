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

#ifndef OPENAIRINTERFACE_WLS_VNF_H
#define OPENAIRINTERFACE_WLS_VNF_H
#include "nfapi_vnf_interface.h"
#include "vnf.h"
#include "wls_common.h"

void *wls_fapi_vnf_nr_start_thread(void *ptr);
int wls_fapi_nr_vnf_start();
void wls_vnf_set_p7_config(void *p7_config);
bool wls_vnf_nr_send_p5_message(vnf_t *vnf,uint16_t p5_idx, nfapi_nr_p4_p5_message_header_t* msg, uint32_t msg_len);
bool wls_vnf_nr_send_p7_message(vnf_p7_t* vnf_p7,nfapi_nr_p7_message_header_t* msg);
#endif //OPENAIRINTERFACE_WLS_VNF_H
