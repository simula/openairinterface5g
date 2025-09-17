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

#ifndef SOFTMODEM_BITS_H
#define SOFTMODEM_BITS_H

// clang-format off
#define BIT_0        (1 << 0)
#define BIT_1        (1 << 1)
#define BIT_2        (1 << 2)
#define BIT_10       (1 << 10)
#define BIT_12       (1 << 12)
#define BIT_13       (1 << 13)
#define BIT_15       (1 << 15)
#define BIT_16       (1 << 16)
#define BIT_17       (1 << 17)
#define BIT_18       (1 << 18)
#define BIT_20       (1 << 20)
#define BIT_21       (1 << 21)
#define BIT_22       (1 << 22)
#define BIT_23       (1 << 23)
#define BIT_24       (1 << 24)
#define BIT_25       (1 << 25)

#define SOFTMODEM_NOS1_BIT                  BIT_0
#define SOFTMODEM_NOKRNMOD_BIT              BIT_1
#define SOFTMODEM_NONBIOT_BIT               BIT_2
#define SOFTMODEM_RFSIM_BIT                 BIT_10
#define SOFTMODEM_SIML1_BIT                 BIT_12
#define SOFTMODEM_DLSIM_BIT                 BIT_13
#define SOFTMODEM_DOSCOPE_BIT               BIT_15
#define SOFTMODEM_RECPLAY_BIT               BIT_16
#define SOFTMODEM_TELNETCLT_BIT             BIT_17
#define SOFTMODEM_RECRECORD_BIT             BIT_18
#define SOFTMODEM_ENB_BIT                   BIT_20
#define SOFTMODEM_GNB_BIT                   BIT_21
#define SOFTMODEM_4GUE_BIT                  BIT_22
#define SOFTMODEM_5GUE_BIT                  BIT_23
#define SOFTMODEM_NOSTATS_BIT               BIT_24
#define SOFTMODEM_IMSCOPE_BIT               BIT_25
// clang-format on

#endif // SOFTMODEM_BITS_H
