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

/*! \file openair2/LAYER2/NR_MAC_COMMON/nr_prach_config.h
* \brief Tools to query the PRACH configuration tables
* \author Romain beurdouche
* \date Jul. 2025
* \version 0.1
* \company Eurecom
* \email romain.beurdouche@eurecom.fr
* \note The PRACH configuration tables are used in the MAC layer for scheduling and beyond.
*       For example the PRACH duration is requested in the O-RAN 7.2 FrontHaul Interface
*/

/**
 * @brief Fetch PRACH format (format only) from PRACH configuration tables
 *
 * @param index PRACH configuration index
 * @param unpaired Duplex mode TDD or FDD
 * @param freq_range Frequency range FR1 or FR2
 * @return PRACH format (format only)
 */
int get_format0(uint8_t index, uint8_t unpaired, frequency_range_t frequency_range);

typedef struct {
  uint32_t format;
  uint32_t start_symbol;
  uint32_t N_t_slot;
  uint32_t N_dur;
  uint32_t N_RA_slot;
  uint32_t N_RA_sfn;
  uint32_t max_association_period;
  int x;
  int y;
  int y2;
  uint64_t s_map;
} nr_prach_info_t;

/**
 * @brief Fetch PRACH occasion info from PRACH configuration tables
 *
 * @param index PRACH configuration index
 * @param freq_range Frequency range FR1 or FR2
 * @param unpaired Duplex mode TDD or FDD
 * @return PRACH occasion information
 */
nr_prach_info_t get_nr_prach_occasion_info_from_index(uint8_t index, frequency_range_t freq_range, uint8_t unpaired);

/**
 * @brief Fetch PRACH format (format concatenated with format2) from PRACH configuration tables
 *
 * @param index PRACH configuration index
 * @param pointa Point A ARFCN to determine the Frequency range FR1 or FR2
 * @param unpaired Duplex mode TDD or FDD
 * @return PRACH format (format concatenated with format2)
 */
uint16_t get_nr_prach_format_from_index(uint8_t index, uint32_t pointa, uint8_t unpaired);
