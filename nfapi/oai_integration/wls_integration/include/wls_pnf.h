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

#ifndef OPENAIRINTERFACE_WLS_PNF_H
#define OPENAIRINTERFACE_WLS_PNF_H
#include "nfapi_pnf_interface.h"
#include "pnf.h"
#include "wls_common.h"

bool wls_pnf_nr_send_p5_message(pnf_t *pnf, nfapi_nr_p4_p5_message_header_t* msg, uint32_t msg_len);
bool wls_pnf_nr_send_p7_message(pnf_p7_t* pnf_p7,nfapi_nr_p7_message_header_t *msg, uint32_t msg_len);
void *wls_fapi_pnf_nr_start_thread(void *ptr);
int wls_fapi_nr_pnf_start();
void wls_pnf_set_p7_config(void *p7_config);
#endif // OPENAIRINTERFACE_WLS_PNF_H
