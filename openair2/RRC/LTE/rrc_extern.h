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

/*! \file rrc_extern.h
 * \brief rrc external vars
 * \author Navid Nikaein and Raymond Knopp
 * \date 2011
 * \version 1.0
 * \company Eurecom
 * \email: navid.nikaein@eurecom.fr
 */

#ifndef __OPENAIR_RRC_EXTERN_H__
#define __OPENAIR_RRC_EXTERN_H__
#include "rrc_defs.h"
#include "COMMON/mac_rrc_primitives.h"
#include "LAYER2/MAC/mac.h"
#include "LAYER2/RLC/rlc.h"
#include "openair2/RRC/LTE/rrc_defs.h"

extern UE_RRC_INST *UE_rrc_inst;

extern LTE_LogicalChannelConfig_t SRB1_logicalChannelConfig_defaultValue;
extern LTE_LogicalChannelConfig_t SRB2_logicalChannelConfig_defaultValue;

extern int NB_UE_INST;
extern void* bigphys_malloc(int);
extern LCHAN_DESC CCCH_LCHAN_DESC, DCCH_LCHAN_DESC;

extern UE_PF_PO_t UE_PF_PO[NFAPI_CC_MAX][MAX_MOBILES_PER_ENB];
extern pthread_mutex_t ue_pf_po_mutex;

extern uint16_t reestablish_rnti_map[MAX_MOBILES_PER_ENB][2];

#endif


