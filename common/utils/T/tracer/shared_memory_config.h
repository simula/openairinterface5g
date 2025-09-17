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

/*! \file common/utils/T/tracer/shared_memory_config.h
 * \brief shared memory to store data captured by T-Tracer services to be processed by data recording application
 * \author Abdo Gaber
 * \date 2024
 * \version 1.0
 * \company Emerson, NI Test and Measurement
 * \email:
 * \note
 * \warning
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>

#ifndef _SHARED_MEMORY_CONFIG_H_
#define _SHARED_MEMORY_CONFIG_H_

#define SHMSIZE ((122.88e6 / (100 * 20)) * 1000 * 8) // Assume capture in 1000, each I+Q represented by 8 byts
//  122.88e6: 10ms at 100 MHz BW of 5G NR
// 20: 20 slots in 10ms
// subcarrier spacing: 30 KHz

// for gNB T Tracer App
#define GETKEYDIR1_gNB ("/tmp/gnb_app1")
#define GETKEYDIR2_gNB ("/tmp/gnb_app2")
#define PROJECTID_gNB (2335)

// for UET Tracer App
#define GETKEYDIR1_UE ("/tmp/ue_app1")
#define GETKEYDIR2_UE ("/tmp/ue_app2")
#define PROJECTID_UE (2336)

void err_exit(char *buf);
int create_shm(char **addrN, const char *pathname, int projectId);
void del_shm(char *addr, int shm_id);

#endif /* _SHARED_MEMORY_CONFIG_H_ */
