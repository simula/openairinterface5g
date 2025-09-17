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

/*! \file nr_nas_msg.h
 * \brief simulator for nr nas message
 * \author Yoshio INOUE, Masayuki HARADA
 * \email yoshio.inoue@fujitsu.com,masayuki.harada@fujitsu.com
 * \protocol 5GS (5GMM and 5GSM)
 * \date 2020
 * \version 0.1
 */

#ifndef __NR_NAS_MSG_SIM_H__
#define __NR_NAS_MSG_SIM_H__

#include <common/utils/assertions.h>
#include <openair3/UICC/usim_interface.h>
#include <stdbool.h>
#include <stdint.h>
#include "as_message.h"
#include "NR_NAS_defs.h"
#include "secu_defs.h"
#include "NR_NAS_defs.h"

#define INITIAL_REGISTRATION 0b001

#define PLAIN_5GS_NAS_MESSAGE_HEADER_LENGTH 3
#define SECURITY_PROTECTED_5GS_NAS_MESSAGE_HEADER_LENGTH 7
#define PAYLOAD_CONTAINER_LENGTH_MIN 3
#define PAYLOAD_CONTAINER_LENGTH_MAX 65537
#define NAS_INTEGRITY_SIZE 4

/* 3GPP TS 24.501: 9.11.3.50 Service type */
#define SERVICE_TYPE_DATA 0x1

typedef enum fgs_mm_state_e {
  FGS_DEREGISTERED,
  FGS_DEREGISTERED_INITIATED,
  FGS_REGISTERED_INITIATED,
  FGS_REGISTERED,
  FGS_SERVICE_REQUEST_INITIATED,
} fgs_mm_state_t;

/*
 * 5GS mobility management (5GMM) modes
 * 5.1.3.2.1.1 of TS 24.501
 */
typedef enum fgs_mm_mode_e {
  FGS_NOT_CONNECTED,
  FGS_IDLE,
  FGS_CONNECTED,
} fgs_mm_mode_t;

/* Security Key for SA UE */
typedef struct {
  uint8_t kausf[32];
  uint8_t kseaf[32];
  uint8_t kamf[32];
  uint8_t knas_int[16];
  uint8_t knas_enc[16];
  uint8_t res[16];
  uint8_t rand[16];
  uint8_t kgnb[32];
  uint32_t nas_count_ul;
  uint32_t nas_count_dl;
} ue_sa_security_key_t;

typedef struct {
  /* 5GS Mobility Management States (5.1.3.2.1 of 3GPP TS 24.501) */
  fgs_mm_state_t fiveGMM_state;
  /* 5GS Mobility Management mode */
  fgs_mm_mode_t fiveGMM_mode;
  uicc_t *uicc;
  ue_sa_security_key_t security;
  stream_security_container_t *security_container;
  Guti5GSMobileIdentity_t *guti;
  bool termination_procedure;
  instance_t UE_id;
  /* RRC Inactive Indication */
  bool is_rrc_inactive;
  /* Timer T3512 */
  int t3512;
  // Timer t3448 in seconds (-1 = disabled)
  int t3448;
  // Timer t3446 in seconds (-1 = disabled)
  int t3446;
  /* NAS Key Set Identifier associated to the security context */
  uint8_t *ksi;
} nr_ue_nas_t;

nr_ue_nas_t *get_ue_nas_info(module_id_t module_id);
void generateRegistrationRequest(as_nas_info_t *initialNasMsg, nr_ue_nas_t *nas, bool is_security_mode);
void generateServiceRequest(as_nas_info_t *initialNasMsg, nr_ue_nas_t *nas);
void *nas_nrue_task(void *args_p);
void *nas_nrue(void *args_p);
void nas_init_nrue(int num_ues);
void nr_ue_create_ip_if(const char *ifnameprefix, const char *ipv4, const char *ipv6, int ue_id, int pdu_session_id);

#endif /* __NR_NAS_MSG_SIM_H__*/
