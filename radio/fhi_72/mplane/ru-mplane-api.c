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

#include "ru-mplane-api.h"
#include "xml/get-xml.h"
#include "common/utils/assertions.h"

#include <string.h>

static void free_match_list(char **match_list, size_t count)
{
  for (size_t i = 0; i < count; i++) {
    free(match_list[i]);
  }
  free(match_list);
}

static void fix_benetel_setting(xran_mplane_t *xran_mplane, const uint32_t interface_mtu, const int16_t first_iq_width, const int max_num_ant, const char *model_name)
{
  if (interface_mtu == 1500) {
    MP_LOG_I("Interface MTU %d unreliable/not correctly reported by Benetel O-RU, hardcoding to 9600.\n", interface_mtu);
    xran_mplane->mtu = 9600;
  } else {
    xran_mplane->mtu = interface_mtu;
  }

  if (first_iq_width != 9) {
    MP_LOG_I("IQ bitwidth %d unreliable/not correctly reported by Benetel O-RU, hardcoding to 9.\n", first_iq_width);
    xran_mplane->iq_width = 9;
  } else {
    xran_mplane->iq_width = first_iq_width;
  }

  xran_mplane->prach_offset = max_num_ant;

  if (strcasecmp(model_name, "RAN550") == 0) {
    xran_mplane->max_tx_gain = 24.0;
  } else if (strcasecmp(model_name, "RAN650") == 0) {
    xran_mplane->max_tx_gain = 35.0;
  } else {
    assert(false && "[MPLANE] Unknown Benetel model name.\n");
  }
}

bool get_config_for_xran(const char *buffer, const int max_num_ant, xran_mplane_t *xran_mplane)
{
  /* some O-RU vendors are not fully compliant as per M-plane specifications */
  const char *ru_vendor = get_ru_xml_node(buffer, "mfg-name");

  // RU MAC
  xran_mplane->ru_mac_addr = get_ru_xml_node(buffer, "mac-address"); // TODO: support for VVDN, as it defines multiple MAC addresses

  // MTU
  const uint32_t interface_mtu = (uint32_t)atoi(get_ru_xml_node(buffer, "l2-mtu"));

  // IQ bitwidth
  char **match_list = NULL;
  size_t count = 0;
  get_ru_xml_list(buffer, "iq-bitwidth", &match_list, &count);
  const int16_t first_iq_width = (int16_t)atoi((char *)match_list[0]);
  free_match_list(match_list, count);

  // PRACH offset
  // xran_mplane->prach_offset

  // DU port ID bitmask
  xran_mplane->du_port_bitmask = 0xf000;
  // Band sector bitmask
  xran_mplane->band_sector_bitmask = 0x0f00;
  // CC ID bitmask
  xran_mplane->ccid_bitmask = 0x00f0;
  // RU port ID bitmask
  xran_mplane->ru_port_bitmask = 0x000f;

  // DU port ID
  xran_mplane->du_port = 0;
  // Band sector
  xran_mplane->band_sector = 0;
  // CC ID
  xran_mplane->ccid = 0;
  // RU port ID
  xran_mplane->ru_port = 0;

  // Frame structure
  match_list = NULL;
  count = 0;
  get_ru_xml_list(buffer, "supported-frame-structures", &match_list, &count);
  xran_mplane->frame_str = (int16_t)atoi((char *)match_list[0]);
  free_match_list(match_list, count);

  // Managed delay support
  const char *managed_delay = get_ru_xml_node(buffer, "managed-delay-support");
  xran_mplane->managed_delay = (strcasecmp(managed_delay, "NON_MANAGED") == 0) ? false : true;

  // Store the max gain
  xran_mplane->max_tx_gain = (double)atof(get_ru_xml_node(buffer, "max-gain"));

  // Model name
  const char *model_name = get_ru_xml_node(buffer, "model-name");

  if (strcasecmp(ru_vendor, "BENETEL") == 0 /* || strcmp(ru_vendor, "VVDN-LPRU") == 0 || strcmp(ru_vendor, "Metanoia") == 0 */) {
    fix_benetel_setting(xran_mplane, interface_mtu, first_iq_width, max_num_ant, model_name);
  } else {
    AssertError(false, return false, "[MPLANE] %s RU currently not supported.\n", ru_vendor);
  }

  MP_LOG_I("Storing the following information to forward to xran:\n\
    RU MAC address %s\n\
    MTU %d\n\
    IQ bitwidth %d\n\
    PRACH offset %d\n\
    DU port bitmask %d\n\
    Band sector bitmask %d\n\
    CC ID bitmask %d\n\
    RU port ID bitmask %d\n\
    DU port ID %d\n\
    Band sector ID %d\n\
    CC ID %d\n\
    RU port ID %d\n\
    max Tx gain %.1f\n",
      xran_mplane->ru_mac_addr,
      xran_mplane->mtu,
      xran_mplane->iq_width,
      xran_mplane->prach_offset,
      xran_mplane->du_port_bitmask,
      xran_mplane->band_sector_bitmask,
      xran_mplane->ccid_bitmask,
      xran_mplane->ru_port_bitmask,
      xran_mplane->du_port,
      xran_mplane->band_sector,
      xran_mplane->ccid,
      xran_mplane->ru_port,
      xran_mplane->max_tx_gain);

  return true;
}

bool get_uplane_info(const char *buffer, ru_mplane_config_t *ru_mplane_config)
{
  // Interface name
  ru_mplane_config->interface_name = get_ru_xml_node(buffer, "interface");

  // PDSCH
  uplane_info_t *tx_end = &ru_mplane_config->tx_endpoints;
  get_ru_xml_list(buffer, "static-low-level-tx-endpoints", &tx_end->name, &tx_end->num);
  AssertError(tx_end->name != NULL, return false, "[MPLANE] Cannot get TX endpoint names.\n");

  // TX carriers
  uplane_info_t *tx_carriers = &ru_mplane_config->tx_carriers;
  get_ru_xml_list(buffer, "tx-arrays", &tx_carriers->name, &tx_carriers->num);
  AssertError(tx_carriers->name != NULL, return false, "[MPLANE] Cannot get TX carrier names.\n");

  // PUSCH and PRACH
  uplane_info_t *rx_end = &ru_mplane_config->rx_endpoints;
  get_ru_xml_list(buffer, "static-low-level-rx-endpoints", &rx_end->name, &rx_end->num);
  AssertError(rx_end->name != NULL, return false, "[MPLANE] Cannot get RX endpoint names.\n");

  // RX carriers
  uplane_info_t *rx_carriers = &ru_mplane_config->rx_carriers;
  get_ru_xml_list(buffer, "rx-arrays", &rx_carriers->name, &rx_carriers->num);
  AssertError(rx_carriers->name != NULL, return false, "[MPLANE] Cannot get RX carrier names.\n");

  MP_LOG_I("Successfully retrieved all the U-plane info - interface name, TX/RX carrier names, and TX/RX endpoint names.\n");

  return true;
}

bool get_pm_object_list(const char *buffer, pm_stats_t *pm_stats)
{
  const char *ru_vendor = get_ru_xml_node(buffer, "mfg-name");
  if (strcasecmp(ru_vendor, "BENETEL") == 0) {
    pm_stats->start_up_timing = false;
  } else {
    pm_stats->start_up_timing = true;
  }

  // Rx window
  get_ru_xml_list(buffer, "rx-window-objects", &pm_stats->rx_window_meas, &pm_stats->rx_num);

  // Tx
  get_ru_xml_list(buffer, "tx-stats-objects", &pm_stats->tx_meas, &pm_stats->tx_num);

  MP_LOG_I("Successfully retreived all performance measurement names.\n");

  return true;
}
