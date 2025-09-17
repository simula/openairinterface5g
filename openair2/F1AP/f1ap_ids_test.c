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

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "common/utils/assertions.h"
#include "f1ap_ids.h"

void du_data_test()
{
  du_init_f1_ue_data();
  int rnti = 13;
  f1_ue_data_t data = {.secondary_ue = 1};
  bool ret = du_add_f1_ue_data(rnti, &data);
  DevAssert(ret);
  ret = du_add_f1_ue_data(rnti, &data);
  DevAssert(!ret);
  bool exists = du_exists_f1_ue_data(rnti);
  DevAssert(exists);
  f1_ue_data_t rdata = du_get_f1_ue_data(rnti);
  DevAssert(rdata.secondary_ue == data.secondary_ue);
}

void cu_data_test()
{
  cu_init_f1_ue_data();
  int ue_id = 13;
  f1_ue_data_t data = {.secondary_ue = 1};
  bool ret = cu_add_f1_ue_data(ue_id, &data);
  DevAssert(ret);
  ret = cu_add_f1_ue_data(ue_id, &data);
  DevAssert(!ret);
  data.secondary_ue = 2;
  ret = cu_update_f1_ue_data(ue_id, &data);
  bool exists = cu_exists_f1_ue_data(ue_id);
  DevAssert(exists);
  f1_ue_data_t rdata = cu_get_f1_ue_data(ue_id);
  DevAssert(rdata.secondary_ue == data.secondary_ue);

  rdata.secondary_ue = 3;
  // overwrite once more
  ret = cu_update_f1_ue_data(ue_id, &rdata);
  DevAssert(ret);
  exists = cu_exists_f1_ue_data(ue_id);
  DevAssert(exists);
  f1_ue_data_t r2data = cu_get_f1_ue_data(ue_id);
  DevAssert(r2data.secondary_ue == rdata.secondary_ue);
}

int main()
{
  du_data_test();
  cu_data_test();
  return 0;
}
