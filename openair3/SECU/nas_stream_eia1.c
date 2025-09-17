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

#include <string.h>

#include "nas_stream_eia1.h"
#include "assertions.h"
#include "snow3g.h"

/*!
 * @brief Create integrity cmac t for a given message.
 * @param[in] stream_cipher Structure containing various variables to setup encoding
 * @param[out] out For EIA1 the output string is 32 bits long
 */
void nas_stream_encrypt_eia1(nas_stream_cipher_t const *stream_cipher, uint8_t out[4])
{
  uint8_t *key = (uint8_t *)stream_cipher->context;
  snow3g_integrity(stream_cipher->count,
                   stream_cipher->bearer,
                   stream_cipher->direction,
                   key,
                   stream_cipher->blength / 8,
                   stream_cipher->message,
                   out);
}

stream_security_context_t *stream_integrity_init_eia1(const uint8_t *integrity_key)
{
  void *ret = calloc(1, 16);
  AssertFatal(ret != NULL, "out of memory\n");
  memcpy(ret, integrity_key, 16);
  return (stream_security_context_t *)ret;
}

void stream_integrity_free_eia1(stream_security_context_t *integrity_context)
{
  free(integrity_context);
}
