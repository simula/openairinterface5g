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

/*
                                gnb_config.h
                             -------------------
  AUTHOR  : Lionel GAUTHIER, Navid Nikaein, Laurent Winckel, WEI-TAI CHEN
  COMPANY : EURECOM, NTSUT
  EMAIL   : Lionel.Gauthier@eurecom.fr, navid.nikaein@eurecom.fr, kroempa@gmail.com
*/

#ifndef GNB_CONFIG_H_
#define GNB_CONFIG_H_
#include <stdint.h>
#include "RRC/NR/nr_rrc_defs.h"
#include "assertions.h"
#include "common/config/config_load_configmodule.h"
#include "common/ngran_types.h"
#include "f1ap_messages_types.h"
#include "intertask_interface.h"

#define IPV4_STR_ADDR_TO_INT_NWBO(AdDr_StR,NwBo,MeSsAgE ) do {\
            struct in_addr inp;\
            if ( inet_aton(AdDr_StR, &inp ) < 0 ) {\
                AssertFatal (0, MeSsAgE);\
            } else {\
                NwBo = inp.s_addr;\
            }\
        } while (0);

// Hard to find a defined value for max enb...
#define MAX_GNB 16

void RCconfig_verify(configmodule_interface_t *cfg, ngran_node_t node_type);
extern void RCconfig_nr_prs(void);
extern void RCconfig_NR_L1(void);
extern void RCconfig_nr_macrlc(configmodule_interface_t *cfg);
extern void NRRCConfig(void);

gNB_RRC_INST *RCconfig_NRRRC();
int RCconfig_NR_NG(MessageDef *msg_p, uint32_t i);
int RCconfig_NR_X2(MessageDef *msg_p, uint32_t i);
void wait_f1_setup_response(void);
f1ap_tdd_info_t read_tdd_config(const NR_ServingCellConfigCommon_t *scc);
f1ap_gnb_du_system_info_t *get_sys_info(NR_BCCH_BCH_Message_t *mib, const NR_BCCH_DL_SCH_Message_t *sib1, seq_arr_t *du_SIBs);
int gNB_app_handle_f1ap_gnb_cu_configuration_update(f1ap_gnb_cu_configuration_update_t *gnb_cu_cfg_update);
MessageDef *RCconfig_NR_CU_E1(const E1_t *entity);
ngran_node_t get_node_type(void);

#ifdef E2_AGENT
#include "openair2/E2AP/e2_agent_arg.h"
e2_agent_args_t RCconfig_NR_E2agent(void);
#endif // E2_AGENT

#endif /* GNB_CONFIG_H_ */
/** @} */
