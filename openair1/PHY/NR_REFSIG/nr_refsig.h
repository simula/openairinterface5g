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

/* Definitions for LTE Reference signals */
/* Author R. Knopp / EURECOM / OpenAirInterface.org */
#ifndef __NR_REFSIG__H__
#define __NR_REFSIG__H__

#include "PHY/defs_gNB.h"
#include "openair1/PHY/NR_REFSIG/nr_refsig_common.h"
#include "PHY/nr_phy_common/inc/nr_phy_common.h"

int nr_pusch_dmrs_rx(PHY_VARS_gNB *gNB,
                     unsigned int Ns,
                     const uint32_t *nr_gold_pusch,
                     c16_t *output,
                     unsigned short p,
                     unsigned char lp,
                     unsigned short nb_pusch_rb,
                     uint32_t re_offset,
                     uint8_t dmrs_type,
                     int16_t dmrs_scaling);

void nr_generate_modulation_table(void);

extern simde__m128i byte2m128i[256];

int nr_pusch_lowpaprtype1_dmrs_rx(PHY_VARS_gNB *gNB,
                                  unsigned int Ns,
                                  c16_t *dmrs_seq,
                                  c16_t *output,
                                  unsigned short p,
                                  unsigned char lp,
                                  unsigned short nb_pusch_rb,
                                  uint32_t re_offset,
                                  uint8_t dmrs_type);

#endif
