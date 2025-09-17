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

#include "nr_pdcp_integrity_nia2.h"

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "common/utils/assertions.h"
#include "openair3/SECU/aes_128.h"
#include "openair3/SECU/aes_128_cbc_cmac.h"

stream_security_context_t *nr_pdcp_integrity_nia2_init(uint8_t integrity_key[16])
{
  cbc_cmac_ctx_t* ctx = calloc(1, sizeof(cbc_cmac_ctx_t));
  AssertFatal(ctx, "Memory exhausted");

  *ctx = init_aes_128_cbc_cmac(integrity_key);
  return (stream_security_context_t *)ctx;
}

void nr_pdcp_integrity_nia2_integrity(stream_security_context_t *integrity_context, unsigned char *out, unsigned char *buffer, int length, int bearer, int count, int direction)
{
  DevAssert(integrity_context != NULL);
  DevAssert(out != NULL);
  DevAssert(buffer != NULL);
  DevAssert(length > -1);
  DevAssert(bearer > 0 && bearer < 33);
  DevAssert(count > -1);

  cbc_cmac_ctx_t *ctx = (cbc_cmac_ctx_t *)integrity_context;

  aes_128_t k_iv = {0};
  memcpy(&k_iv.key, ctx->key, sizeof(k_iv.key));
  k_iv.type = AES_INITIALIZATION_VECTOR_8;
  k_iv.iv8.d.bearer = bearer -1;
  k_iv.iv8.d.direction = direction;
  k_iv.iv8.d.count = htonl(count);

  uint8_t result[16] = {0};
  byte_array_t msg = {.buf = buffer, .len = length};
  
  cipher_aes_128_cbc_cmac(ctx, &k_iv, msg, sizeof(result), result);

  memcpy(out, result, 4);
}

void nr_pdcp_integrity_nia2_free_integrity(stream_security_context_t *integrity_context)
{
  free_aes_128_cbc_cmac((cbc_cmac_ctx_t *)integrity_context);
  free(integrity_context);
}
