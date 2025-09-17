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

#include "byte_array_producer.h"

/* create a byte array producer from an existing buffer */
byte_array_producer_t byte_array_producer_from_buffer(uint8_t *buffer, int len)
{
  byte_array_producer_t result = { .b.len = len, .b.buf = buffer, .pos = 0 };
  return result;
}

/* put a byte in a byte array if there is space
 * returns 1 on success, 0 on error
 */
int byte_array_producer_put_byte(byte_array_producer_t *b, uint8_t byte)
{
  if (b->pos == b->b.len)
    return 0;

  b->b.buf[b->pos] = byte;
  b->pos++;

  return 1;
}

/* put an u32 number as big endian in a byte array if there is space
 * returns 1 on success, 0 on error
 */
int byte_array_producer_put_u32_be(byte_array_producer_t *b, uint32_t v)
{
  return byte_array_producer_put_byte(b, (v >> 24) & 0xff)
         && byte_array_producer_put_byte(b, (v >> 16) & 0xff)
         && byte_array_producer_put_byte(b, (v >> 8) & 0xff)
         && byte_array_producer_put_byte(b, v & 0xff);
}

/* put an u24 number as big endian in a byte array if there is space
 * returns 1 on success, 0 on error
 */
int byte_array_producer_put_u24_be(byte_array_producer_t *b, uint32_t v)
{
  return byte_array_producer_put_byte(b, (v >> 16) & 0xff)
         && byte_array_producer_put_byte(b, (v >> 8) & 0xff)
         && byte_array_producer_put_byte(b, v & 0xff);
}
