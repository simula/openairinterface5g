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

#ifndef SOCKET_PNF_H
#define SOCKET_PNF_H
#include "socket_common.h"
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "pnf.h"
#include "pnf_p7.h"
#include "nr_fapi_p5.h"
#include "nr_nfapi_p7.h"

int pnf_connect_socket(pnf_t *pnf);
int pnf_nr_message_pump(pnf_t *pnf);
bool pnf_nr_send_p5_message(pnf_t *pnf, nfapi_nr_p4_p5_message_header_t *msg, uint32_t msg_len);
bool pnf_nr_send_p7_message(pnf_p7_t* pnf_p7, nfapi_nr_p7_message_header_t* header, uint32_t msg_len);
void *pnf_start_p5_thread(void *ptr);
void *pnf_nr_p7_thread_start(void *ptr);
#endif // SOCKET_PNF_H
