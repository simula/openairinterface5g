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

#include "position_interface.h"

static void read_position_coordinates(char *sectionName, position_t *position)
{
  paramdef_t position_params[] = POSITION_CONFIG_PARAMS_DEF;
  int ret = config_get(config_get_if(), position_params, sizeofArray(position_params), sectionName);
  AssertFatal(ret >= 0, "configuration couldn't be performed for position name: %s", sectionName);
}

void get_position_coordinates(int Mod_id, position_t *position)
{
  AssertFatal(Mod_id < NB_UE_INST, "Mod_id must be less than NB_UE_INST. Mod_id:%d NB_UE_INST:%d", Mod_id, NB_UE_INST);
  char positionName[64];
  snprintf(positionName, sizeof(positionName), "position%d", Mod_id);
  read_position_coordinates(positionName, position);
}
