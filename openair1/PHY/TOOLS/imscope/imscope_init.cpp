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

#include "imscope_internal.h"

extern "C" void imscope_autoinit(void *dataptr)
{
  AssertFatal(IS_SOFTMODEM_5GUE || IS_SOFTMODEM_GNB,
              "Scope cannot find NRUE or GNB context");

  for (auto i = 0U; i < EXTRA_SCOPE_TYPES; i++) {
    scope_array[i].is_data_ready = false;
    scope_array[i].data.scope_graph_data = nullptr;
    scope_array[i].data.meta = {-1, -1};
  }

  scopeData_t *scope = (scopeData_t *)calloc(1, sizeof(scopeData_t));
  scope->copyData = copyDataThreadSafe;
  scope->tryLockScopeData = tryLockScopeData;
  scope->copyDataUnsafeWithOffset = copyDataUnsafeWithOffset;
  scope->unlockScopeData = unlockScopeData;
  scope->copyData = copyDataThreadSafe;
  if (IS_SOFTMODEM_GNB) {
    scopeParms_t *scope_params = (scopeParms_t *)dataptr;
    scope_params->gNB->scopeData = scope;
    scope_params->ru->scopeData = scope;
  } else {
    PHY_VARS_NR_UE *ue = (PHY_VARS_NR_UE *)dataptr;
    ue->scopeData = scope;
  }

  pthread_t thread;
  threadCreate(&thread, imscope_thread, dataptr, (char *)"imscope", -1, sched_get_priority_min(SCHED_RR));
}
