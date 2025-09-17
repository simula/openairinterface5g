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

#include "subscribe-mplane.h"
#include "rpc-send-recv.h"
#include "common/utils/assertions.h"

#include <libyang/libyang.h>
#include <nc_client.h>

#ifdef MPLANE_V1
static void recv_notif_v1(const struct nc_notif *notif, ru_notif_t *answer)
{
  const char *node_name = notif->tree->child->attr->name;
  const char *value = notif->tree->child->attr->value_str;
  if (strcmp(node_name, "sync-state")) {
    if (strcmp(value, "LOCKED") == 0) {
      answer->ptp_state = true;
    } else {
      answer->ptp_state = false;
    }
  }

  // carriers state - to be filled
}

static void notif_clb_v1(struct nc_session *session, const struct nc_notif *notif)
{
  ru_notif_t *answer = nc_session_get_data(session);
  LYD_FORMAT output_format = LYD_JSON;

  char *subs_reply = NULL;
  lyd_print_mem(&subs_reply, notif->tree, output_format, LYP_WITHSIBLINGS);
  MP_LOG_I("\nReceived notification at (%s)\n%s\n", notif->datetime, subs_reply);

  recv_notif_v1(notif, answer);
}
#elif MPLANE_V2
static void log_v2_pm_info(const char *ru_ip_add, struct lyd_node_inner *stats)
{
  struct lyd_node *child, *field = NULL;
  size_t len = 0;
  char *meas_list[8], *count_list[8] = {};

  LY_LIST_FOR(stats->child, child) {
    // const char *type = child->schema->name; // e.g. "rx-window-stats", "tx-stats"
    struct lyd_node_inner *stats_inner = (struct lyd_node_inner *)child;
    LY_LIST_FOR(stats_inner->child, field) {
      if (strcmp(field->schema->name, "measurement-object") == 0) {
        meas_list[len] = (char *)lyd_get_value(field);
      } else if (strcmp(field->schema->name, "count") == 0) {
        count_list[len] = (char *)lyd_get_value(field);
        len++;
      }
    }
  }

  MP_LOG_I("[PM: \"%s\"][%s %7s][%s %7s][%s %7s][%s %7s][%s %7s][%s %7s][%s %7s][%s %7s]\n",
		  ru_ip_add,
		  meas_list[0], count_list[0],
		  meas_list[1], count_list[1],
		  meas_list[2], count_list[2],
		  meas_list[3], count_list[3],
		  meas_list[4], count_list[4],
		  meas_list[5], count_list[5],
		  meas_list[6], count_list[6],
		  meas_list[7], count_list[7]);
}

static void recv_notif_v2(struct lyd_node_inner *op, ru_notif_t *answer)
{
  const char *notif = op->schema->name;

  if (strcmp(notif, "synchronization-state-change") == 0) {
    const char *value = lyd_get_value(op->child);
    if (strcmp(value, "LOCKED") == 0) {
      answer->ptp_state = true;
    } else { // "FREERUN" or "HOLDOVER"
      answer->ptp_state = false;
    }
  } else if (strcmp(notif, "rx-array-carriers-state-change") == 0) {
    const char *value = lyd_get_value(lyd_child(op->child)->next);
    if (strcmp(value, "READY") == 0) {
      answer->rx_carrier_state = true;
    } else { // "DISABLED" or "BUSY"
      answer->rx_carrier_state = false;
    }
  } else if (strcmp(notif, "tx-array-carriers-state-change") == 0) {
    const char *value = lyd_get_value(lyd_child(op->child)->next);
    if (strcmp(value, "READY") == 0) {
      answer->tx_carrier_state = true;
    } else { // "DISABLED" or "BUSY"
      answer->tx_carrier_state = false;
    }
  } else if (strcmp(notif, "netconf-config-change") == 0) {
    answer->config_change = true;
  }
}

static void notif_clb_v2(struct nc_session *session, const struct lyd_node *envp, const struct lyd_node *op, void *user_data)
{
  ru_notif_t *answer = (ru_notif_t *)user_data;
  LYD_FORMAT output_format = LYD_JSON;

  char *subs_reply = NULL;
  lyd_print_mem(&subs_reply, op, output_format, LYD_PRINT_WITHSIBLINGS);
  const char *ru_ip_add = nc_session_get_host(session);
  struct lyd_node_inner *op_inner = (struct lyd_node_inner *)op;
  if (strcmp(op_inner->schema->name, "measurement-result-stats") == 0) {
    log_v2_pm_info(ru_ip_add, op_inner);
    return;
  }
  MP_LOG_I("Received notification from RU \"%s\" at (%s)\n%s\n", ru_ip_add, ((struct lyd_node_opaq *)lyd_child(envp))->value, subs_reply);

  recv_notif_v2(op_inner, answer);
}
#endif

bool subscribe_mplane(ru_session_t *ru_session, const char *stream, const char *filter, void *answer)
{
  int timeout = CLI_RPC_REPLY_TIMEOUT;
  struct nc_rpc *rpc;
  NC_WD_MODE wd = NC_WD_ALL;
  NC_PARAMTYPE param = NC_PARAMTYPE_CONST;
  char *start_time = NULL, *stop_time = NULL;

  MP_LOG_I("RPC request to RU \"%s\" = <subscribe> with stream \"%s\" and filter \"%s\".\n", ru_session->ru_ip_add, stream, filter);
  rpc = nc_rpc_subscribe(stream, NULL, start_time, stop_time, param);
  AssertError(rpc != NULL, return false, "[MPLANE] <subscribe> RPC creation failed.\n");

  /* create notification thread so that notifications can immediately be received */
#ifdef MPLANE_V1
  if (!nc_session_ntf_thread_running(ru_session->session)) {
    nc_session_set_data(ru_session->session, answer);
    int ret = nc_recv_notif_dispatch(ru_session->session, notif_clb_v1);
    AssertError(ret == 0, return false, "[MPLANE] Failed to create notification thread.\n");
  } else {
    MP_LOG_I("Notification thread is already running for RU %s.\n", ru_session->ru_ip_add);
  }
#elif MPLANE_V2
  int ret = nc_recv_notif_dispatch_data(ru_session->session, notif_clb_v2, answer, NULL);
  AssertError(ret == 0, return false, "[MPLANE] Failed to create notification thread.\n");
#endif

  bool success = rpc_send_recv((struct nc_session *)ru_session->session, rpc, wd, timeout, NULL);
  AssertError(success, return false, "[MPLANE] Failed to subscribe to: stream \"%s\", filter \"%s\".\n", stream, filter);

  MP_LOG_I("Successfully subscribed to all notifications from RU \"%s\".\n", ru_session->ru_ip_add);

  nc_rpc_free(rpc);

  return true;
}

bool update_timer_mplane(ru_session_t *ru_session, char **answer)
{
  int timeout = CLI_RPC_REPLY_TIMEOUT;
  struct nc_rpc *rpc;
  NC_WD_MODE wd = NC_WD_ALL;
  NC_PARAMTYPE param = NC_PARAMTYPE_CONST;
  const char *content = "<supervision-watchdog-reset xmlns=\"urn:o-ran:supervision:1.0\">\n\
<supervision-notification-interval>65535</supervision-notification-interval>\n\
<guard-timer-overhead>65535</guard-timer-overhead>\n\
</supervision-watchdog-reset>";

  MP_LOG_I("RPC request to RU \"%s\" = \"%s\".\n", ru_session->ru_ip_add, content);
  rpc = nc_rpc_act_generic_xml(content, param);
  AssertError(rpc != NULL, return false, "[MPLANE] <supervision-watchdog-reset> RPC creation failed.\n");

  bool success = rpc_send_recv((struct nc_session *)ru_session->session, rpc, wd, timeout, answer);
  AssertError(success, return false, "[MPLANE] Failed to update the watchdog timer.\n");

  MP_LOG_I("Successfully updated supervision timer to (65535+65535)[s] for RU \"%s\".\n", ru_session->ru_ip_add);

  nc_rpc_free(rpc);

  return true;
}
