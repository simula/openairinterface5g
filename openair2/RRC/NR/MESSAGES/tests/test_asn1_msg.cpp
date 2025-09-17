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

#include <gtest/gtest.h>
#ifdef __cplusplus
extern "C" {
#endif
#include "openair2/RRC/NR/MESSAGES/asn1_msg.h"
#include "common/ran_context.h"
#include <stdbool.h>
#include "common/utils/assertions.h"
#include "common/utils/LOG/log.h"
#include "NR_DRB-ToAddMod.h"
#include "NR_DRB-ToAddModList.h"
#include "NR_SRB-ToAddModList.h"
#include "ds/byte_array.h"
RAN_CONTEXT_t RC;
#ifdef __cplusplus
}
#endif

TEST(nr_asn1, rrc_reject)
{
  unsigned char buf[1000];
  EXPECT_GT(do_RRCReject(buf), 0);
}

TEST(nr_asn1, sa_capability_enquiry)
{
  unsigned char buf[1000];
  EXPECT_GT(do_NR_SA_UECapabilityEnquiry(buf, 0), 0);
}

TEST(nr_asn1, rrc_reconfiguration_complete_for_nsa)
{
  unsigned char buf[1000];
  EXPECT_GT(do_NR_RRCReconfigurationComplete_for_nsa(buf, 1000, 0), 0);
}

TEST(nr_asn1, ul_information_transfer)
{
  unsigned char *buf = NULL;
  unsigned char pdu[20] = {0};
  EXPECT_GT(do_NR_ULInformationTransfer(&buf, 20, pdu), 0);
  EXPECT_NE(buf, nullptr);
  free(buf);
}

TEST(nr_asn1, rrc_reestablishment_request)
{
  unsigned char buf[1000];
  const uint16_t c_rnti = 1;
  const uint32_t cell_id = 177;
  EXPECT_GT(do_RRCReestablishmentRequest(buf, NR_ReestablishmentCause_reconfigurationFailure, cell_id, c_rnti), 0);
}

TEST(nr_asn1, rrc_reestablishment)
{
  unsigned char buf[1000];
  const uint8_t nh_ncc = 0;
  EXPECT_GT(do_RRCReestablishment(nh_ncc, buf, 1000, 0), 0);
}

TEST(nr_asn1, paging)
{
  unsigned char buf[1000];
  EXPECT_GT(do_NR_Paging(0, buf, 0), 0);
}

void free_RRCReconfiguration_params(nr_rrc_reconfig_param_t params)
{
  ASN_STRUCT_FREE(asn_DEF_NR_MeasConfig, params.meas_config);
  ASN_STRUCT_FREE(asn_DEF_NR_DRB_ToReleaseList, params.drb_release_list);
  ASN_STRUCT_FREE(asn_DEF_NR_DRB_ToAddModList, params.drb_config_list);
  ASN_STRUCT_FREE(asn_DEF_NR_SRB_ToAddModList, params.srb_config_list);
  ASN_STRUCT_FREE(asn_DEF_NR_SecurityConfig, params.security_config);
  for (int i = 0; i < params.num_nas_msg; i++)
    FREE_AND_ZERO_BYTE_ARRAY(params.dedicated_NAS_msg_list[i]);
}

TEST(nr_asn1, rrc_reconfiguration)
{
  // SRB Configuration
  NR_SRB_ToAddModList_t *srb_config_list = (NR_SRB_ToAddModList_t *)calloc_or_fail(1, sizeof(*srb_config_list));
  for (int i = 0; i < 4; i++) {
    if (i == 1 || i == 2) {
      NR_SRB_ToAddMod_t *srb = (NR_SRB_ToAddMod_t *)calloc_or_fail(1, sizeof(*srb));
      ASN_SEQUENCE_ADD(&srb_config_list->list, srb);
      srb->srb_Identity = i;
      if (i == 1 || i == 2) {
        srb->reestablishPDCP = (long *)calloc_or_fail(1, sizeof(*srb->reestablishPDCP));
        *srb->reestablishPDCP = 0;
      }
    }
  }

  // DRB Configuration
  NR_DRB_ToAddModList_t *drb_config_list = (NR_DRB_ToAddModList_t *)calloc_or_fail(1, sizeof(*drb_config_list));
  for (int i = 0; i < 32; i++) {
    if (i == 1 || i == 2) {
      NR_DRB_ToAddMod_t *drb = (NR_DRB_ToAddMod_t *)calloc_or_fail(1, sizeof(*drb));
      ASN_SEQUENCE_ADD(&drb_config_list->list, drb);
      drb->drb_Identity = i;
      drb->reestablishPDCP = (long *)calloc_or_fail(1, sizeof(*drb->reestablishPDCP));
      *drb->reestablishPDCP = 0;
    }
  }

  // nr_rrc_reconfig_param_t setup
  nr_rrc_reconfig_param_t params = {};
  params.srb_config_list = srb_config_list;
  params.drb_config_list = drb_config_list;
  params.num_nas_msg = 2;
  params.masterKeyUpdate = false;
  params.nextHopChainingCount = 1;

  byte_array_t nas_pdu_1;
  nas_pdu_1.buf = (uint8_t *)malloc_or_fail(4);
  memcpy(nas_pdu_1.buf, "NAS1", 4);
  nas_pdu_1.len = 4;

  byte_array_t nas_pdu_2;
  nas_pdu_2.buf = (uint8_t *)malloc_or_fail(4);
  memcpy(nas_pdu_2.buf, "NAS2", 4);
  nas_pdu_2.len = 4;

  params.dedicated_NAS_msg_list[0] = nas_pdu_1;
  params.dedicated_NAS_msg_list[1] = nas_pdu_2;

  byte_array_t msg = do_RRCReconfiguration(&params);

  EXPECT_GT(msg.len, 0);
  EXPECT_NE(msg.buf, nullptr);

  LOG_D(NR_RRC, "RRCReconfiguration: Encoded (%ld bytes)\n", msg.len);

  free_byte_array(msg);
  free_RRCReconfiguration_params(params);
}

int main(int argc, char **argv)
{
  logInit();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
