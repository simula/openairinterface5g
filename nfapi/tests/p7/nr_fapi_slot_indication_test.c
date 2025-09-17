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
#include "dci_payload_utils.h"
#include "nr_fapi_p7.h"
#include "nr_fapi_p7_utils.h"

static void fill_slot_indication(nfapi_nr_slot_indication_scf_t *msg)
{
  msg->sfn = rand16_range(0, 1023);
  msg->slot = rand16_range(0, 159);
}

static void test_pack_unpack(nfapi_nr_slot_indication_scf_t *req)
{
  uint8_t msg_buf[1024 * 1024 * 2];
  // first test the packing procedure
  int pack_result = fapi_nr_p7_message_pack(req, msg_buf, sizeof(msg_buf), NULL);

  // Should always return 4 (2 bytes sfn + 2 bytes slot )
  DevAssert(pack_result == 4);
  // update req message_length value with value calculated in message_pack procedure
  // req->header.message_length = pack_result;
  // test the unpacking of the header
  // copy first NFAPI_HEADER_LENGTH bytes into a new buffer, to simulate SCTP PEEK
  fapi_message_header_t header;
  uint32_t header_buffer_size = NFAPI_HEADER_LENGTH;
  uint8_t header_buffer[header_buffer_size];
  for (int idx = 0; idx < header_buffer_size; idx++) {
    header_buffer[idx] = msg_buf[idx];
  }
  uint8_t *pReadPackedMessage = header_buffer;
  int unpack_header_result = fapi_nr_message_header_unpack(pReadPackedMessage, NFAPI_HEADER_LENGTH, &header, sizeof(header), 0);
  DevAssert(unpack_header_result >= 0);
  DevAssert(header.message_id == req->header.message_id);
  DevAssert(header.message_length == req->header.message_length);
  // test the unpacking and compare with initial message
  nfapi_nr_slot_indication_scf_t unpacked_req = {0};
  int unpack_result =
      fapi_nr_p7_message_unpack(msg_buf, header.message_length + NFAPI_HEADER_LENGTH, &unpacked_req, sizeof(unpacked_req), 0);
  DevAssert(unpack_result >= 0);
  DevAssert(eq_slot_indication(&unpacked_req, req));
  free_slot_indication(&unpacked_req);
}

static void test_copy(const nfapi_nr_slot_indication_scf_t *msg)
{
  // Test copy function
  nfapi_nr_slot_indication_scf_t copy = {0};
  copy_slot_indication(msg, &copy);
  DevAssert(eq_slot_indication(msg, &copy));
  free_slot_indication(&copy);
}

int main(int n, char *v[])
{
  fapi_test_init();

  nfapi_nr_slot_indication_scf_t req = {.header.message_id = NFAPI_NR_PHY_MSG_TYPE_SLOT_INDICATION};

  // Fill DL_TTI request
  fill_slot_indication(&req);
  // Perform tests
  test_pack_unpack(&req);
  test_copy(&req);
  // All tests successful!
  free_slot_indication(&req);
  return 0;
}
