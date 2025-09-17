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
#include "nr_common.h"
#include "common/utils/LOG/log.h"
}

TEST(nr_common, nr_timer) {
  NR_timer_t timer;
  nr_timer_setup(&timer, 10, 1);
  nr_timer_start(&timer);
  EXPECT_TRUE(nr_timer_is_active(&timer));
  EXPECT_FALSE(nr_timer_expired(&timer));
  for (auto i = 0; i < 10; i++) {
    nr_timer_tick(&timer);
  }
  EXPECT_FALSE(nr_timer_is_active(&timer));
  EXPECT_TRUE(nr_timer_expired(&timer));
}


int main(int argc, char **argv)
{
  logInit();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
