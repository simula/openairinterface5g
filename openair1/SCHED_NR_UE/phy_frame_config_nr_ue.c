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
* FILENAME    :  phy_frame_configuration_nr_ue.c
*
* DESCRIPTION :  functions related to FDD/TDD configuration for NR
*                see TS 38.213 11.1 Slot configuration
*                and TS 38.331 for RRC configuration
*
************************************************************************/

#include "PHY/defs_nr_UE.h"

/*******************************************************************
*
* NAME :         nr_ue_slot_select
*
* DESCRIPTION :  function for the UE equivalent to nr_slot_select
*
*********************************************************************/

int nr_ue_slot_select(fapi_nr_config_request_t *cfg, int nr_slot)
{
  if (cfg->cell_config.frame_duplex_type == FDD)
    return NR_UPLINK_SLOT | NR_DOWNLINK_SLOT;

  int period = cfg->tdd_table_1.tdd_period_in_slots +
               (cfg->tdd_table_2 ? cfg->tdd_table_2->tdd_period_in_slots : 0);
  int rel_slot = nr_slot % period;
  fapi_nr_tdd_table_t *tdd_table = &cfg->tdd_table_1;
  if (cfg->tdd_table_2 && rel_slot >= tdd_table->tdd_period_in_slots) {
    rel_slot -= tdd_table->tdd_period_in_slots;
    tdd_table = cfg->tdd_table_2;
  }

  if (tdd_table->max_tdd_periodicity_list == NULL) // this happens before receiving TDD configuration
    return NR_DOWNLINK_SLOT;

  fapi_nr_max_tdd_periodicity_t *current_slot = &tdd_table->max_tdd_periodicity_list[rel_slot];

  // if the 1st symbol is UL the whole slot is UL
  if (current_slot->max_num_of_symbol_per_slot_list[0].slot_config == 1)
    return NR_UPLINK_SLOT;

  // if the 1st symbol is flexible the whole slot is mixed
  if (current_slot->max_num_of_symbol_per_slot_list[0].slot_config == 2)
    return NR_MIXED_SLOT;

  for (int i = 1; i < NR_NUMBER_OF_SYMBOLS_PER_SLOT; i++) {
    // if the 1st symbol is DL and any other is not, the slot is mixed
    if (current_slot->max_num_of_symbol_per_slot_list[i].slot_config != 0) {
      return NR_MIXED_SLOT;
    }
  }

  // if here, all the symbols where DL
  return NR_DOWNLINK_SLOT;
}

/*
 * This function determines if the mixed slot is a Sidelink slot
 */
uint8_t sl_determine_if_sidelink_slot(uint8_t sl_startsym, uint8_t sl_lensym, uint8_t num_ulsym)
{
  uint8_t ul_startsym = NR_NUMBER_OF_SYMBOLS_PER_SLOT - num_ulsym;

  if ((sl_startsym >= ul_startsym) && (sl_lensym <= NR_NUMBER_OF_SYMBOLS_PER_SLOT)) {
    LOG_D(MAC,
          "MIXED SLOT is a SIDELINK SLOT. Sidelink Symbols: %d-%d, Uplink Symbols: %d-%d\n",
          sl_startsym,
          sl_lensym - 1,
          ul_startsym,
          ul_startsym + num_ulsym - 1);
    return NR_SIDELINK_SLOT;
  } else {
    LOG_D(MAC,
          "MIXED SLOT is NOT SIDELINK SLOT. Sidelink Symbols: %d-%d, Uplink Symbols: %d-%d\n",
          sl_startsym,
          sl_lensym - 1,
          ul_startsym,
          ul_startsym + num_ulsym - 1);
    return 0;
  }
}

/*
 * This function determines if the Slot is a SIDELINK SLOT
 * Every Uplink Slot is a Sidelink slot
 * Mixed Slot is a sidelink slot if the uplink symbols in Mixed slot
 * overlaps with Sidelink start symbol and number of symbols.
 */
int sl_nr_ue_slot_select(sl_nr_phy_config_request_t *cfg, int slot, uint8_t frame_duplex_type)
{
  int ul_sym = 0, slot_type = 0;

  // All PC5 bands are TDD bands , hence handling only TDD in this function.
  AssertFatal(frame_duplex_type == TDD, "No Sidelink operation defined for FDD in 3GPP rel16\n");

  if (cfg->tdd_table.max_tdd_periodicity_list == NULL) { // this happens before receiving TDD configuration
    return slot_type;
  }

  int period = cfg->tdd_table.tdd_period_in_slots;
  int rel_slot = slot % period;
  fapi_nr_tdd_table_t *tdd_table = &cfg->tdd_table;

  fapi_nr_max_tdd_periodicity_t *current_slot = &tdd_table->max_tdd_periodicity_list[rel_slot];

  for (int symbol_count = 0; symbol_count < NR_NUMBER_OF_SYMBOLS_PER_SLOT; symbol_count++) {
    if (current_slot->max_num_of_symbol_per_slot_list[symbol_count].slot_config == 1) {
      ul_sym++;
    }
  }

  if (ul_sym == NR_NUMBER_OF_SYMBOLS_PER_SLOT) {
    slot_type = NR_SIDELINK_SLOT;
  } else if (ul_sym) {
    slot_type = sl_determine_if_sidelink_slot(cfg->sl_bwp_config.sl_start_symbol, cfg->sl_bwp_config.sl_num_symbols, ul_sym);
  }

  return slot_type;
}