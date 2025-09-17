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

#include "get-mplane.h"
#include "rpc-send-recv.h"

#include "common/utils/assertions.h"

bool get_mplane(ru_session_t *ru_session, char **answer)
{
  int timeout = CLI_RPC_REPLY_TIMEOUT;
  struct nc_rpc *rpc;
  char *filter = NULL; // e.g. "/o-ran-delay-management:delay-management";
  NC_WD_MODE wd = NC_WD_ALL;
  NC_PARAMTYPE param = NC_PARAMTYPE_CONST;

  MP_LOG_I("RPC request to RU \"%s\" = <get> operational datastore.\n", ru_session->ru_ip_add);
  rpc = nc_rpc_get(filter, wd, param);
  AssertError(rpc != NULL, return false, "[MPLANE] <get> RPC creation failed.\n");

  bool success = rpc_send_recv((struct nc_session *)ru_session->session, rpc, wd, timeout, answer);
  AssertError(success, return false, "[MPLANE] Unable to retrieve operational datastore.\n");

  MP_LOG_I("Successfully retrieved operational datastore from RU \"%s\".\n", ru_session->ru_ip_add);

  nc_rpc_free(rpc);

  return true;
}
