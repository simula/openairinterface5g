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

#include "init-mplane.h"
#include "radio/fhi_72/oran-params.h"
#include "get-mplane.h"
#include "subscribe-mplane.h"
#include "config-mplane.h"
#include "xml/get-xml.h"
#include "yang/get-yang.h"
#include "yang/create-yang-config.h"

#include <libyang/libyang.h>
#include <nc_client.h>

static void lnc2_print_clb(NC_VERB_LEVEL level, const char *msg)
{
  switch (level) {
    case NC_VERB_ERROR:
      MP_LOG_I("[LIBNETCONF2] ERROR: %s.\n", msg);
      break;
    case NC_VERB_WARNING:
      MP_LOG_I("[LIBNETCONF2] WARNING: %s.\n", msg);
      break;
    case NC_VERB_VERBOSE:
      MP_LOG_I("[LIBNETCONF2] VERBOSE: %s.\n", msg);
      break;
    case NC_VERB_DEBUG:
    case NC_VERB_DEBUG_LOWLVL:
      MP_LOG_I("[LIBNETCONF2] DEBUG: %s.\n", msg);
      break;
    default:
      assert(false && "[LIBNETCONF2] Unknown log level.");
  }
}

static void ly_print_clb(LY_LOG_LEVEL level, const char *msg, const char *path)
{
  switch (level) {
    case LY_LLERR:
      MP_LOG_I("[LIBYANG] ERROR: %s (path: %s).\n", msg, path);
      break;
    case LY_LLWRN:
      MP_LOG_I("[LIBYANG] WARNING: %s (path: %s).\n", msg, path);
      break;
    case LY_LLVRB:
      MP_LOG_I("[LIBYANG] VERBOSE: %s (path: %s).\n", msg, path);
      break;
    case LY_LLDBG:
      MP_LOG_I("[LIBYANG] DEBUG: %s (path: %s).\n", msg, path);
      break;
    default:
      assert(false && "[LIBYANG] Unknown log level.");
  }
}

static const paramdef_t *gpd(const paramdef_t *pd, int num, const char *name)
{
  /* the config module does not know const-correctness... */
  int idx = config_paramidx_fromname((paramdef_t *)pd, num, (char *)name);
  DevAssert(idx >= 0);
  return &pd[idx];
}

bool init_mplane(ru_session_list_t *ru_session_list)
{
  paramdef_t fhip[] = ORAN_GLOBALPARAMS_DESC;
  int nump = sizeofArray(fhip);
  int ret = config_get(config_get_if(), fhip, nump, CONFIG_STRING_ORAN);
  if (ret <= 0) {
    MP_LOG_I("Problem reading section \"%s\".\n", CONFIG_STRING_ORAN);
    return false;
  }

  ru_session_list->du_key_pair = gpd(fhip, nump, ORAN_CONFIG_DU_KEYPAIR)->strlistptr;
  int num_keys = gpd(fhip, nump, ORAN_CONFIG_DU_KEYPAIR)->numelt;
  AssertError(num_keys == 2, return false, "[MPLANE] Expected {pub-key-path, priv-key-path}. Loaded {%s, %s}.\n", ru_session_list->du_key_pair[0], ru_session_list->du_key_pair[1]);

  char **ru_ip_addrs = gpd(fhip, nump, ORAN_CONFIG_RU_IP_ADDR)->strlistptr;
  int num_rus = gpd(fhip, nump, ORAN_CONFIG_RU_IP_ADDR)->numelt;
  char **ru_usernames = gpd(fhip, nump, ORAN_CONFIG_RU_USERNAME)->strlistptr;
  int num_ru_users = gpd(fhip, nump, ORAN_CONFIG_RU_USERNAME)->numelt;
  char **du_mac_addr = gpd(fhip, nump, ORAN_CONFIG_DU_ADDR)->strlistptr;
  int num_dus = gpd(fhip, nump, ORAN_CONFIG_DU_ADDR)->numelt;
  int32_t *vlan_tag = gpd(fhip, nump, ORAN_CONFIG_VLAN_TAG)->iptr;
  int num_vlan_tags = gpd(fhip, nump, ORAN_CONFIG_VLAN_TAG)->numelt;

  AssertError(num_rus == num_ru_users, return false, "[MPLANE] Number of RUs should be equal to the number of users, one for each.\n");
  AssertError(num_dus == num_vlan_tags, return false, "[MPLANE] Number of DU MAC addresses should be equal to the number of VLAN tags.\n");
 
  int num_cu_planes = num_dus / num_rus;

  ru_session_list->num_rus = num_rus;
  ru_session_list->ru_session = calloc(num_rus, sizeof(ru_session_t));
  for (size_t i = 0; i < num_rus; i++) {
    ru_session_t *ru_session = &ru_session_list->ru_session[i];
    ru_session->session = NULL;
    ru_session->ru_ip_add = calloc(strlen(ru_ip_addrs[i]) + 1, sizeof(char));
    memcpy(ru_session->ru_ip_add, ru_ip_addrs[i], strlen(ru_ip_addrs[i]) + 1);
    ru_session->username = calloc(strlen(ru_usernames[i]) + 1, sizeof(char));
    memcpy(ru_session->username, ru_usernames[i], strlen(ru_usernames[i]) + 1);

    // store DU MAC addresses and VLAN tags
    ru_session->ru_mplane_config.num_cu_planes = num_cu_planes;
    ru_session->ru_mplane_config.du_mac_addr = calloc_or_fail(num_cu_planes, sizeof(char*));
    ru_session->ru_mplane_config.vlan_tag = calloc_or_fail(num_cu_planes, sizeof(int32_t));
    for (int j = 0; j < num_cu_planes; j++) {
      const int idx = i*num_cu_planes+j;
      ru_session->ru_mplane_config.du_mac_addr[j] = calloc(1, strlen(du_mac_addr[idx]) + 1);
      memcpy(ru_session->ru_mplane_config.du_mac_addr[j], du_mac_addr[idx], strlen(du_mac_addr[idx]) + 1);
      ru_session->ru_mplane_config.vlan_tag[j] = vlan_tag[idx];
    }
  }

  nc_client_init();
  int keypair_ret = nc_client_ssh_add_keypair(ru_session_list->du_key_pair[0], ru_session_list->du_key_pair[1]);
  AssertError(keypair_ret == 0, return false, "[MPLANE] Unable to add DU ssh key pair.\n");

  // logs for netconf2 and yang libraries
  nc_set_print_clb(lnc2_print_clb); 
  ly_set_log_clb(ly_print_clb, 1);

  return true;
}

bool manage_ru(ru_session_t *ru_session, const openair0_config_t *oai, const size_t num_rus)
{
  bool success = false;

  char *operational_ds = NULL;
  success = get_mplane(ru_session, &operational_ds);
  AssertError(success, return false, "[MPLANE] Unable to continue: could not get RU answer via get_mplane().\n");

  bool ptp_state = false;
  const char *sync_state = get_ru_xml_node(operational_ds, "sync-state");
  if (strcmp(sync_state, "LOCKED") == 0) {
    MP_LOG_I("RU is already PTP synchronized.\n");
    ptp_state = true;
  }

  /* 1) as per M-plane spec, RU must be in supervised mode,
        where stream = NULL && filter = "/o-ran-supervision:supervision-notification";
     2) additionally, we want to subscribe to PTP state change,
        where stream = NULL && filter = "/o-ran-sync:synchronization-state-change";
    => since more than one subscription at the time within one session is not possible, we will subscribe to all notifications */
  const char *stream = "NETCONF";
  const char *filter = NULL;
  ru_session->ru_notif.ptp_state = ptp_state;
  success = subscribe_mplane(ru_session, stream, filter, (void *)&ru_session->ru_notif);
  AssertError(success, return false, "[MPLANE] Unable to continue: could not get RU answer via subscribe_mplane().\n");

  // when subscribed to the supervision notification, the watchdog timer needs to be updated
  char *watchdog_answer = NULL;
  success = update_timer_mplane(ru_session, &watchdog_answer);
  AssertError(success, return false, "[MPLANE] Unable to update the watchdog timer. RU will do a reset after default supervision timer of (60+10)[s] expires.\n");
  MP_LOG_I("Watchdog timer answer: \n\t%s\n", watchdog_answer);

  // save RU info for xran
  const int max_num_ant = max(oai->tx_num_channels/num_rus, oai->rx_num_channels/num_rus);
  success = get_config_for_xran(operational_ds, max_num_ant, &ru_session->xran_mplane);
  AssertError(success, return false, "[MPLANE] Unable to retrieve required info for xran from RU \"%s\".\n", ru_session->ru_ip_add);

  // save the U-plane info
  success = get_uplane_info(operational_ds, &ru_session->ru_mplane_config);
  AssertError(success, return false, "[MPLANE] Unable to get U-plane info from RU operational datastore.\n");

  // Performance Management
  success = get_pm_object_list(operational_ds, &ru_session->pm_stats);
  AssertError(success, return false, "[MPLANE] Unable to retrieve performance measurement names from RU \"%s\".\n", ru_session->ru_ip_add);

  success = load_yang_models(ru_session, operational_ds);
  AssertError(success, return false, "[MPLANE] Unable to load yang models.\n");

  if (ru_session->ru_notif.ptp_state) {
    char *content = NULL;
    success = configure_ru_from_yang(ru_session, oai, num_rus, &content);
    AssertError(success, return false, "[MPLANE] Unable to create content for <edit-config> RPC for start-up procedure.\n");

    success = edit_val_commmit_rpc(ru_session, content);
    AssertError(success, return false, "[MPLANE] Unable to continue.\n");
    free(content);
  }

  const char *usage_state = get_ru_xml_node(operational_ds, "usage-state");
  MP_LOG_I("Usage state = \"%s\" for RU \"%s\".\n", usage_state, ru_session->ru_ip_add);
  if (strcmp(usage_state, "busy") == 0) { // carriers are already activated
    ru_session->ru_notif.rx_carrier_state = true;
    ru_session->ru_notif.tx_carrier_state = true;
    ru_session->ru_notif.config_change = true;
  } else {
    ru_session->ru_notif.rx_carrier_state = false;
    ru_session->ru_notif.tx_carrier_state = false;
    ru_session->ru_notif.config_change = false;
  }

  free(operational_ds);
  free(watchdog_answer);

  return true;
}

bool pm_conf(ru_session_t *ru_session, const char *active)
{
  char *content = get_pm_content(ru_session, active);

  bool success = edit_val_commmit_rpc(ru_session, content);
  AssertError(success, return false, "[MPLANE] Cannot continue.\n");

  free(content);

  return true;
}
