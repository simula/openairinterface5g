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

#include <string.h>
#include "nr_ue_phy_meas.h"
#include <stdbool.h>

void init_nr_ue_phy_cpu_stats(nr_ue_phy_cpu_stat_t *ue_phy_cpu_stats) {
  reset_nr_ue_phy_cpu_stats(ue_phy_cpu_stats);
  const char* cpu_stats_enum_to_string_table[] = {
    FOREACH_NR_PHY_CPU_MEAS(TO_STRING)
  };
  for (int i = 0; i < MAX_CPU_STAT_TYPE; i++) {
    ue_phy_cpu_stats->cpu_time_stats[i].meas_name = strdup(cpu_stats_enum_to_string_table[i]);
  }
}

void reset_nr_ue_phy_cpu_stats(nr_ue_phy_cpu_stat_t *ue_phy_cpu_stats) {
  for (int i = 0; i < MAX_CPU_STAT_TYPE; i++) {
    reset_meas(&ue_phy_cpu_stats->cpu_time_stats[i]);
  }
}
