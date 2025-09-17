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
* FILENAME    :  srs_modulation_nr.h
*
* MODULE      :
*
* DESCRIPTION :  function to generate uplink reference sequences
*                see 3GPP TS 38.211 6.4.1.4 Sounding reference signal
*
************************************************************************/

#ifndef SRS_MODULATION_NR_H
#define SRS_MODULATION_NR_H

#include "PHY/defs_nr_UE.h"

#define SRS_PERIODICITY                 (17)

static const uint16_t srs_periodicity[SRS_PERIODICITY] = {1, 2, 4, 5, 8, 10, 16, 20, 32, 40, 64, 80, 160, 320, 640, 1280, 2560};

// TS 38.211 - Table 6.4.1.4.2-1
static const uint16_t srs_max_number_cs[3] = {8, 12, 6};

/*************** FUNCTIONS *****************************************/

/** \brief This function generates the sounding reference symbol (SRS) for the uplink according to 38.211 6.4.1.4 Sounding reference signal
    @param frame_parms NR DL Frame parameters
    @param txdataF pointer to the frequency domain TX signal
    @param symbol_offset symbol offset added in txdataF
    @param nr_srs_info pointer to the srs info structure
    @param amp amplitude of generated signal
    @param frame_number frame number
    @param slot_number slot number
    @returns 0 on success -1 on error with message */

int generate_srs_nr(nfapi_nr_srs_pdu_t *srs_config_pdu,
                    NR_DL_FRAME_PARMS *frame_parms,
                    c16_t **txdataF,
                    uint16_t symbol_offset,
                    nr_srs_info_t *nr_srs_info,
                    int16_t amp,
                    frame_t frame_number,
                    slot_t slot_number);

/** \brief This function checks for periodic srs if srs should be transmitted in this slot
 *  @param p_SRS_Resource pointer to active resource
    @param frame_parms NR DL Frame parameters
    @param txdataF pointer to the frequency domain TX signal
    @returns 0 if srs should be transmitted -1 on error with message */

int is_srs_period_nr(SRS_Resource_t *p_SRS_Resource,
                     NR_DL_FRAME_PARMS *frame_parms,
                     int frame_tx, int slot_tx);

/** \brief This function processes srs configuration
 *  @param ue context
    @param rxtx context
    @param current gNB_id identifier
    @returns 0 if srs is transmitted -1 otherwise */

int ue_srs_procedures_nr(PHY_VARS_NR_UE *ue, const UE_nr_rxtx_proc_t *proc, c16_t **txdataF);

#endif /* SRS_MODULATION_NR_H */
