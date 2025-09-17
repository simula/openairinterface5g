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

#include "nr_pdcp_security_nea1.h"

#include <stdlib.h>
#include <string.h>

#include "assertions.h"

stream_security_context_t *nr_pdcp_security_nea1_init(unsigned char *ciphering_key)
{
  nas_stream_cipher_t *ret;

  ret = calloc(1, sizeof(*ret));
  AssertFatal(ret != NULL, "out of memory\n");
  ret->context = malloc(16);
  AssertFatal(ret->context != NULL, "out of memory\n");
  memcpy(ret->context, ciphering_key, 16);

  return (stream_security_context_t *)ret;
}

void nr_pdcp_security_nea1_cipher(stream_security_context_t *security_context,
                                  unsigned char *buffer,
                                  int length,
                                  int bearer,
                                  int count,
                                  int direction)
{
  nas_stream_cipher_t *ctx = (nas_stream_cipher_t *)security_context;

  ctx->message = buffer;
  ctx->count = count;
  ctx->bearer = bearer - 1;
  ctx->direction = direction;
  ctx->blength = length * 8;

  uint8_t out[length];

  stream_compute_encrypt(EEA1_128_ALG_ID, ctx, out);

  memcpy(buffer, out, length);
}

void nr_pdcp_security_nea1_free_security(stream_security_context_t *security_context)
{
  nas_stream_cipher_t *ctx = (nas_stream_cipher_t *)security_context;
  free(ctx->context);
  free(ctx);
}
