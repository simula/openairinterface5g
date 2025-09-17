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

#include "gtest/gtest.h"
extern "C" {
#include "openair2/LAYER2/NR_MAC_UE/mac_proto.h"
#include "executables/softmodem-common.h"

static softmodem_params_t softmodem_params;
softmodem_params_t *get_softmodem_params(void)
{
  return &softmodem_params;
}
void nr_mac_rrc_ra_ind(const module_id_t mod_id, bool success)
{
}
void nr_mac_rrc_msg3_ind(const module_id_t mod_id, const int rnti, bool prepare_payload)
{
}
tbs_size_t nr_mac_rlc_data_req(const module_id_t  module_idP,
                               const uint16_t ue_id,
                               const bool gnb_flagP,
                               const logical_chan_id_t channel_idP,
                               const tb_size_t tb_sizeP,
                               char *buffer_pP)
{
  return 0;
}
fapi_nr_ul_config_request_pdu_t *lockGet_ul_config(NR_UE_MAC_INST_t *mac, frame_t frame_tx, int slot_tx, uint8_t pdu_type)
{
  return nullptr;
}
void release_ul_config(fapi_nr_ul_config_request_pdu_t *configPerSlot, bool clearIt)
{
}
void remove_ul_config_last_item(fapi_nr_ul_config_request_pdu_t *pdu)
{
}
int nr_ue_configure_pucch(NR_UE_MAC_INST_t *mac,
                          int slot,
                          frame_t frame,
                          uint16_t rnti,
                          PUCCH_sched_t *pucch,
                          fapi_nr_ul_config_pucch_pdu *pucch_pdu)
{
  return 0;
}
NR_UE_DL_BWP_t *get_dl_bwp_structure(NR_UE_MAC_INST_t *mac, int bwp_id, bool setup)
{
  return NULL;
}
NR_UE_UL_BWP_t *get_ul_bwp_structure(NR_UE_MAC_INST_t *mac, int bwp_id, bool setup)
{
  return NULL;
}
}
#include <cstdio>
#include "common/utils/LOG/log.h"

TEST(test_init_ra, four_step_cbra)
{
  NR_UE_MAC_INST_t mac = {0};
  RA_config_t *ra = &mac.ra;
  NR_RACH_ConfigCommon_t nr_rach_ConfigCommon = {0};
  NR_RACH_ConfigGeneric_t rach_ConfigGeneric = {0};
  NR_RACH_ConfigDedicated_t rach_ConfigDedicated = {0};
  NR_UE_UL_BWP_t current_bwp;
  NR_UE_DL_BWP_t dl_bwp;
  mac.current_UL_BWP = &current_bwp;
  mac.current_DL_BWP = &dl_bwp;
  mac.mib_ssb = 0;
  long scs = 1;
  current_bwp.scs = scs;
  current_bwp.bwp_id = 0;
  dl_bwp.bwp_id = 0;
  current_bwp.channel_bandwidth = 40;
  nr_rach_ConfigCommon.msg1_SubcarrierSpacing = &scs;
  nr_rach_ConfigCommon.rach_ConfigGeneric = rach_ConfigGeneric;
  current_bwp.rach_ConfigCommon = &nr_rach_ConfigCommon;
  ra->rach_ConfigDedicated = &rach_ConfigDedicated;
  mac.p_Max = 23;
  mac.nr_band = 78;
  mac.frame_structure.frame_type = TDD;
  mac.frame_structure.numb_slots_frame = 20;
  mac.frequency_range = FR1;
  int frame = 151;

  init_RA(&mac, frame);

  EXPECT_EQ(mac.ra.ra_type, RA_4_STEP);
  EXPECT_EQ(mac.state, UE_PERFORMING_RA);
  EXPECT_EQ(mac.ra.RA_active, true);
  EXPECT_EQ(mac.ra.cfra, 0);
}

TEST(test_init_ra, four_step_cfra)
{
  NR_UE_MAC_INST_t mac = {0};
  RA_config_t *ra = &mac.ra;
  NR_RACH_ConfigCommon_t nr_rach_ConfigCommon = {0};
  NR_RACH_ConfigGeneric_t rach_ConfigGeneric = {0};
  NR_UE_UL_BWP_t current_bwp;
  NR_UE_DL_BWP_t dl_bwp;
  mac.current_UL_BWP = &current_bwp;
  mac.current_DL_BWP = &dl_bwp;
  mac.mib_ssb = 0;
  long scs = 1;
  current_bwp.scs = scs;
  current_bwp.bwp_id = 0;
  dl_bwp.bwp_id = 0;
  current_bwp.channel_bandwidth = 40;
  nr_rach_ConfigCommon.msg1_SubcarrierSpacing = &scs;
  nr_rach_ConfigCommon.rach_ConfigGeneric = rach_ConfigGeneric;
  current_bwp.rach_ConfigCommon = &nr_rach_ConfigCommon;
  mac.p_Max = 23;
  mac.nr_band = 78;
  mac.frame_structure.frame_type = TDD;
  mac.frame_structure.numb_slots_frame = 20;
  mac.frequency_range = FR1;
  int frame = 151;

  NR_RACH_ConfigDedicated_t rach_ConfigDedicated = {0};
  NR_CFRA_t cfra;
  rach_ConfigDedicated.cfra = &cfra;
  ra->rach_ConfigDedicated = &rach_ConfigDedicated;

  init_RA(&mac, frame);

  EXPECT_EQ(mac.ra.ra_type, RA_4_STEP);
  EXPECT_EQ(mac.state, UE_PERFORMING_RA);
  EXPECT_EQ(mac.ra.RA_active, true);
  EXPECT_EQ(mac.ra.cfra, 1);
}

int main(int argc, char **argv)
{
  logInit();
  uniqCfg = load_configmodule(argc, argv, CONFIG_ENABLECMDLINEONLY);
  g_log->log_component[MAC].level = OAILOG_DEBUG;
  g_log->log_component[NR_MAC].level = OAILOG_DEBUG;
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
