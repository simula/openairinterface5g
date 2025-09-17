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
/*! \file nfapi/tests/p5/nr_fapi_param_request_test.c
 * \brief
 * \author Ruben S. Silva
 * \date 2024
 * \version 0.1
 * \company OpenAirInterface Software Alliance
 * \email: contact@openairinterface.org, rsilva@allbesmart.pt
 * \note
 * \warning
 */
#include "nfapi/tests/nr_fapi_test.h"
#include "nr_fapi_p5.h"
#include "nr_fapi.h"
void printbits(uint64_t n, uint8_t numBytesToPrint)
{
  uint64_t i;
  uint8_t counter = 0;
  if (numBytesToPrint == 0) {
    i = 1UL << (sizeof(n) * 8 - 1);
  } else {
    i = 1UL << (numBytesToPrint * 8 - 1);
  }
  while (i > 0) {
    if (n & i)
      printf("1");
    else
      printf("0");
    i >>= 1;
    counter++;
    if (counter % 8 == 0) {
      printf(" ");
    }
  }
}

/*! \brief Drops unwanted bits from a byte array, leaving only a specified number of payload bits
 *
 *  \param payloadSizeBits How many bits the payload is to have, from 1 to 64 (8 * DCI_PAYLOAD_BYTE_LEN)
 *  \param payload[] A uint8_t array containing the payload bytes with random data
 *  \details This function creates a bitmask of payloadSizeBits width to truncate the data in payload[] to only have the specified
 *  number of payload bits\n
 *  For example, a payload of 39 bits needs 5 bytes, but on the last byte, the last bit is unused, this function makes sure that
 *  last bit is set to 0, allowing the payload to be then packed and unpacked and successfully compared with the original payload
 */
void truncate_unwanted_bits(uint8_t payloadSizeBits, uint8_t payload[])
{
  uint8_t payloadSizeBytes = (payloadSizeBits + 7) / 8;
  printf("Original Value:\t");
  uint64_t t = 0;
  memcpy(&t, payload, payloadSizeBytes);
  printbits(t, payloadSizeBytes);
  printf("\n");
  uint64_t bitmask = 1;
  for (int i = 0; i < payloadSizeBits - 1; i++) {
    bitmask = bitmask << 1 | 1;
  }
  printf("Calculated Bitmask:\t");
  printbits(bitmask, payloadSizeBytes);
  printf("\n");
  t = t & bitmask;
  printf("Truncated Value:\t");
  printbits(t, payloadSizeBytes);
  printf("\n");
  memcpy(payload, &t, payloadSizeBytes);
}

/*! \brief Generates a random payload payloadSizeBits long into payload[]
 *
 *  \param payloadSizeBits How many bits the payload is to have, from 1 to 64 (8 * DCI_PAYLOAD_BYTE_LEN)
 *  \param payload[] A uint8_t array to contain the generated payload
 *  \details This function fills a uint8_t array with payloadSizeBits of random data, by first filling however many bytes are needed
 *  to contain payloadSizeBits with random data, and then truncating the excess bits
 */
void generate_payload(uint8_t payloadSizeBits, uint8_t payload[])
{
  for (int i = 0; i < (payloadSizeBits + 7) / 8; ++i) {
    payload[i] = rand8();
  }
  truncate_unwanted_bits(payloadSizeBits, payload);
}

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
