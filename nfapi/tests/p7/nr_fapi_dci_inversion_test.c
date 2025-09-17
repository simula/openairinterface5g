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
#include "nr_fapi.h"

void test_pack_payload(uint8_t payloadSizeBits, uint8_t payload[])
{
  uint8_t msg_buf[8192] = {0};
  uint8_t payloadSizeBytes = (payloadSizeBits + 7) / 8;
  uint8_t *pWritePackedMessage = msg_buf;
  uint8_t *pPackMessageEnd = msg_buf + sizeof(msg_buf);

  pack_dci_payload(payload, payloadSizeBits, &pWritePackedMessage, pPackMessageEnd);

  uint8_t unpack_buf[payloadSizeBytes];
  pWritePackedMessage = msg_buf;
  unpack_dci_payload(unpack_buf, payloadSizeBits, &pWritePackedMessage, pPackMessageEnd);

  printf("\nOriginal:\t");
  for (int j = payloadSizeBytes - 1; j >= 0; j--) {
    printbits(payload[j], 1);
    printf(" ");
  }
  printf("\n");

  printf("Unpacked:\t");
  for (int j = payloadSizeBytes - 1; j >= 0; j--) {
    printbits(unpack_buf[j], 1);
    printf(" ");
  }
  printf("\n");

  DevAssert(memcmp(payload, unpack_buf, payloadSizeBytes) == 0);
  // All tests successful!
}

int main(int n, char *v[])
{
  fapi_test_init();
  uint8_t upper = 8 * DCI_PAYLOAD_BYTE_LEN;
  uint8_t lower = 1;
  uint8_t payloadSizeBits = (rand() % (upper - lower + 1)) + lower; // from 1 bit to DCI_PAYLOAD_BYTE_LEN, in bits
  printf("payloadSizeBits:%d\n", payloadSizeBits);
  uint8_t payload[(payloadSizeBits + 7) / 8];

  generate_payload(payloadSizeBits, payload);
  test_pack_payload(payloadSizeBits, payload);
  return 0;
}
