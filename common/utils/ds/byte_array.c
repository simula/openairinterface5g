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

#include "byte_array.h"

#include <assert.h>
#include <string.h>
#include "common/utils/utils.h" // for malloc_or_fail

byte_array_t create_byte_array(const size_t len, const uint8_t* buffer)
{
  byte_array_t result = {.len = len};
  if (len) {
    result.buf = malloc_or_fail(len);
    memcpy(result.buf, buffer, len);
  }
  return result;
}

byte_array_t copy_byte_array(byte_array_t src)
{
  return create_byte_array(src.len, src.buf);
}

void free_byte_array(byte_array_t ba)
{
  free(ba.buf);
}

bool eq_byte_array(const byte_array_t* m0, const byte_array_t* m1)
{
  if (m0 == m1)
    return true;

  if (m0 == NULL || m1 == NULL)
    return false;

  if (m0->len != m1->len)
    return false;

  const int rc = memcmp(m0->buf, m1->buf, m0->len);
  if (rc != 0)
    return false;

  return true;
}

byte_array_t cp_str_to_ba(const char* str)
{
  assert(str != NULL);
  
  const size_t sz = strlen(str);

  byte_array_t dst = {.len = sz};

  dst.buf = calloc(sz,sizeof(uint8_t));
  assert(dst.buf != NULL && "Memory exhausted");

  memcpy(dst.buf, str, sz);

  return dst;
}

char* cp_ba_to_str(const byte_array_t ba)
{
  assert(ba.len > 0);

  const size_t sz = ba.len;
  char* str = calloc(sz+1, sizeof(char));
  assert(str != NULL && "Memory exhausted");

  memcpy(str, ba.buf, sz);
  str[sz] = '\0';

  return str;
}
