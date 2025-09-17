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
extern "C" {
#include "openair2/RRC/NR/MESSAGES/asn1_msg.h"
#include "common/ran_context.h"
#include <stdbool.h>
#include "common/utils/assertions.h"
#include "common/utils/LOG/log.h"
RAN_CONTEXT_t RC;
}

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
  rrc_gNB_ue_context_t ue_context_pP;
  memset(&ue_context_pP, 0, sizeof(ue_context_pP));
  unsigned char buf[1000];
  const uint32_t physical_cell_id = 177;
  NR_ARFCN_ValueNR_t absoluteFrequencySSB = 2700000;
  EXPECT_GT(do_RRCReestablishment(&ue_context_pP, buf, 1000, 0, physical_cell_id, absoluteFrequencySSB), 0);
}

TEST(nr_asn1, paging)
{
  unsigned char buf[1000];
  EXPECT_GT(do_NR_Paging(0, buf, 0), 0);
}

int main(int argc, char **argv)
{
  logInit();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
