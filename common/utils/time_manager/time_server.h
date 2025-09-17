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

#ifndef COMMON_UTIL_TIME_MANAGER_TIME_SERVER
#define COMMON_UTIL_TIME_MANAGER_TIME_SERVER

#include "time_source.h"

/* opaque data type */
typedef void time_server_t;

time_server_t *new_time_server(const char *ip,
                               int port,
                               void (*callback)(void *),
                               void *callback_data);
void free_time_server(time_server_t *time_server);

void time_server_attach_time_source(time_server_t *time_server,
                                    time_source_t *time_source);

#endif /* COMMON_UTIL_TIME_MANAGER_TIME_SERVER */

