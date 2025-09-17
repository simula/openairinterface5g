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

#ifndef SOCKET_VNF_H
#define SOCKET_VNF_H
#include "socket_common.h"
#include "vnf.h"
#include "vnf_p7.h"
#include "nr_fapi_p5.h"
#include "nr_nfapi_p7.h"
#include "nr_fapi_p7.h"
bool vnf_nr_send_p5_msg(vnf_t *vnf, uint16_t p5_idx, nfapi_nr_p4_p5_message_header_t* msg, uint32_t msg_len);
bool vnf_nr_send_p7_msg(vnf_p7_t *vnf_p7, nfapi_nr_p7_message_header_t* header);
void vnf_start_p5_thread(void* ptr);
void* vnf_nr_start_p7_thread(void* ptr);
#endif // SOCKET_VNF_H
