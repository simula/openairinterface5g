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

/***********************************************************************
*
* FILENAME    :  phy_frame_configuration_nr.h
*
* DESCRIPTION :  functions related to FDD/TDD configuration  for NR
*                see TS 38.213 11.1 Slot configuration
*                and TS 38.331 for RRC configuration
*
************************************************************************/

#include "PHY/defs_gNB.h"

#ifndef PHY_FRAME_CONFIG_NR_H
#define PHY_FRAME_CONFIG_NR_H

/*************** FUNCTIONS *****************************************/

/** @brief This function processes TDD dedicated configuration for NR
 *         by processing the tdd_slot_bitmap and period_cfg, and
 *         allocates memory and fills max_num_of_symbol_per_slot_list
 *         in the nfapi config request (cfg)
 *  @param cfg NR config request structure pointer
 *  @param fs  frame structure pointer
 *  @returns nb_periods_per_frame if TDD has been properly configurated
 *           -1 tdd configuration can not be done
 */
void set_tdd_config_nr(nfapi_nr_config_request_scf_t *cfg, frame_structure_t *fs);

/** \brief This function checks nr slot direction : downlink or uplink
 *  @param frame_parms NR DL Frame parameters
 *  @param nr_frame : frame number
 *  @param nr_slot  : slot number
    @returns int : downlink, uplink or mixed slot type*/

int nr_slot_select(nfapi_nr_config_request_scf_t *cfg, int nr_frame, int nr_slot);

void do_tdd_config_sim(PHY_VARS_gNB *gNB, int mu);

#endif  /* PHY_FRAME_CONFIG_NR_H */

