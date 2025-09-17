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
#ifndef SOCKET_COMMON_H
#define SOCKET_COMMON_H

#define _GNU_SOURCE /* required for pthread_getname_np */
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <assertions.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netdb.h>

int socket_send_p5_msg(const int sctp,
                       const int socket,
                       const void* remote_addr,
                       const void* msg,
                       const uint32_t len,
                       const uint16_t stream);
int socket_send_p7_msg(const int socket, const void* remote_addr, const void* msg, const uint32_t len);

#endif // SOCKET_COMMON_H
