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

#include <stdlib.h>
#include <string.h>

#include "assertions.h"
#include "nas_stream_eea1.h"
#include "snow3g.h"

void nas_stream_encrypt_eea1(nas_stream_cipher_t const *stream_cipher, uint8_t *out)
{
  uint8_t *key = (uint8_t *)stream_cipher->context;
  snow3g_ciphering(stream_cipher->count,
                   stream_cipher->bearer,
                   stream_cipher->direction,
                   key,
                   stream_cipher->blength / 8,
                   stream_cipher->message,
                   out);
}

stream_security_context_t *stream_ciphering_init_eea1(const uint8_t *ciphering_key)
{
  void *ret = calloc(1, 16);
  AssertFatal(ret != NULL, "out of memory\n");
  memcpy(ret, ciphering_key, 16);
  return (stream_security_context_t *)ret;
}

void stream_ciphering_free_eea1(stream_security_context_t *ciphering_context)
{
  free(ciphering_context);
}
