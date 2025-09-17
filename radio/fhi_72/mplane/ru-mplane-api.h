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

#ifndef RU_MPLANE_API_H
#define RU_MPLANE_API_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "common/utils/LOG/log.h"

#define MP_LOG_I(x, args...) LOG_I(HW, "[MPLANE] " x, ##args)
#define MP_LOG_W(x, args...) LOG_W(HW, "[MPLANE] " x, ##args)

typedef struct {
  bool ptp_state;
  bool rx_carrier_state;
  bool tx_carrier_state;
  bool config_change;
  // to be extended with any notification callback

} ru_notif_t;

typedef struct {
  char *ru_mac_addr;
  uint32_t mtu;
  int16_t iq_width;
  uint8_t prach_offset;

  // DU sends to RU and xran
  uint16_t du_port_bitmask;
  uint16_t band_sector_bitmask;
  uint16_t ccid_bitmask;
  uint16_t ru_port_bitmask;

  // DU retrieves from RU, and sends to xran
  uint8_t du_port;
  uint8_t band_sector;
  uint8_t ccid;
  uint8_t ru_port;
  int16_t frame_str; // might be needed in the future xran releases
  bool managed_delay;

  double max_tx_gain;

} xran_mplane_t;

typedef struct {
  size_t num;
  char **name;
} uplane_info_t;

typedef struct {
  size_t num_cu_planes;
  char **du_mac_addr; // one or two VF(s) for CU-planes
  int32_t *vlan_tag;  // one or two VF(s) for CU-planes
  char *interface_name;
  uplane_info_t tx_endpoints;
  uplane_info_t rx_endpoints;
  uplane_info_t tx_carriers;
  uplane_info_t rx_carriers;

} ru_mplane_config_t;

typedef struct {
  // activation if required at start-up timing
  bool start_up_timing;

  size_t rx_num;
  char **rx_window_meas;

  size_t tx_num;
  char **tx_meas;

} pm_stats_t;

typedef struct {
  char *username;
  char *ru_ip_add;
  ru_mplane_config_t ru_mplane_config;
  void *session;
  void *ctx;
  xran_mplane_t xran_mplane;
  ru_notif_t ru_notif;
  pm_stats_t pm_stats;

} ru_session_t;

typedef struct {
  size_t num_rus;
  ru_session_t *ru_session;
  char **du_key_pair;

} ru_session_list_t;

bool get_config_for_xran(const char *buffer, const int max_num_ant, xran_mplane_t *xran_mplane);

bool get_uplane_info(const char *buffer, ru_mplane_config_t *ru_mplane_config);

bool get_pm_object_list(const char *buffer, pm_stats_t *pm_stats);

#endif /* RU_MPLANE_API_H */
