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

#include "secu_defs.h"

#include "common/utils/assertions.h"
#include "common/utils/LOG/log.h"

#include "nas_stream_eea0.h"
#include "nas_stream_eea1.h"
#include "nas_stream_eea2.h"

#include "nas_stream_eia1.h"
#include "nas_stream_eia2.h"

void stream_compute_integrity(eia_alg_id_e alg, nas_stream_cipher_t const* stream_cipher, uint8_t out[4])
{
  if (alg == EIA0_ALG_ID) {
    LOG_E(OSA, "Provided integrity algorithm is currently not supported = %u\n", alg);
  } else if (alg == EIA1_128_ALG_ID) {
    LOG_D(OSA, "EIA1 algorithm applied for integrity\n");
    nas_stream_encrypt_eia1(stream_cipher, out);
  } else if (alg == EIA2_128_ALG_ID) {
    LOG_D(OSA, "EIA2 algorithm applied for integrity\n");
    nas_stream_encrypt_eia2(stream_cipher, out);
  } else {
    LOG_E(OSA, "Provided integrity algorithm is currently not supported = %u\n", alg);
    AssertFatal(0 != 0, "Unknown Algorithm type");
  }
}

void stream_compute_encrypt(eea_alg_id_e alg, nas_stream_cipher_t const* stream_cipher, uint8_t* out)
{
  if (alg == EEA0_ALG_ID) {
    LOG_D(OSA, "EEA0 algorithm applied for encryption\n");
    nas_stream_encrypt_eea0(stream_cipher, out);
  } else if (alg == EEA1_128_ALG_ID) {
    LOG_D(OSA, "EEA1 algorithm applied for encryption\n");
    nas_stream_encrypt_eea1(stream_cipher, out);
  } else if (alg == EEA2_128_ALG_ID) {
    LOG_D(OSA, "EEA2 algorithm applied for  encryption\n");
    nas_stream_encrypt_eea2(stream_cipher, out);
  } else {
    LOG_E(OSA, "Provided encrypt algorithm is currently not supported = %u\n", alg);
    AssertFatal(0 != 0, "Unknown Algorithm type");
  }
}

stream_security_context_t *stream_integrity_init(int integrity_algorithm, const uint8_t *integrity_key)
{
  switch (integrity_algorithm) {
    case EEA0_ALG_ID: return NULL;
    case EEA1_128_ALG_ID: return stream_integrity_init_eia1(integrity_key);
    case EEA2_128_ALG_ID: return stream_integrity_init_eia2(integrity_key);
    default: AssertFatal(0, "unsupported integrity algorithm\n");
  }
}

void stream_integrity_free(int integrity_algorithm, stream_security_context_t *integrity_context)
{
  switch (integrity_algorithm) {
    case EEA0_ALG_ID: return;
    case EEA1_128_ALG_ID: return stream_integrity_free_eia1(integrity_context);
    case EEA2_128_ALG_ID: return stream_integrity_free_eia2(integrity_context);
    default: AssertFatal(0, "unsupported integrity algorithm\n");
  }
}

stream_security_context_t *stream_ciphering_init(int ciphering_algorithm, const uint8_t *ciphering_key)
{
  switch (ciphering_algorithm) {
    case EEA0_ALG_ID: return NULL;
    case EEA1_128_ALG_ID: return stream_ciphering_init_eea1(ciphering_key);
    case EEA2_128_ALG_ID: return stream_ciphering_init_eea2(ciphering_key);
    default: AssertFatal(0, "unsupported ciphering algorithm\n");
  }
}

void stream_ciphering_free(int ciphering_algorithm, stream_security_context_t *ciphering_context)
{
  switch (ciphering_algorithm) {
    case EEA0_ALG_ID: return;
    case EEA1_128_ALG_ID: return stream_ciphering_free_eea1(ciphering_context);
    case EEA2_128_ALG_ID: return stream_ciphering_free_eea2(ciphering_context);
    default: AssertFatal(0, "unsupported ciphering algorithm\n");
  }
}

stream_security_container_t *stream_security_container_init(int ciphering_algorithm,
                                                            int integrity_algorithm,
                                                            const uint8_t *ciphering_key,
                                                            const uint8_t *integrity_key)
{
  stream_security_container_t *container = calloc(1, sizeof(*container));
  AssertFatal(container != NULL, "out of memory\n");

  container->integrity_algorithm = integrity_algorithm;
  container->ciphering_algorithm = ciphering_algorithm;

  container->integrity_context = stream_integrity_init(integrity_algorithm, integrity_key);
  container->ciphering_context = stream_ciphering_init(ciphering_algorithm, ciphering_key);

  return container;
}

void stream_security_container_delete(stream_security_container_t *container)
{
  /* passing NULL is accepted */
  if (container == NULL)
    return;

  stream_integrity_free(container->integrity_algorithm, container->integrity_context);
  stream_ciphering_free(container->ciphering_algorithm, container->ciphering_context);
  free(container);
}
