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

#ifndef BYTE_ARRAY_PRODUCER_H
#define BYTE_ARRAY_PRODUCER_H

#include <stdint.h>

#include "byte_array.h"

typedef struct {
  byte_array_t b;
  size_t pos;
} byte_array_producer_t;

byte_array_producer_t byte_array_producer_from_buffer(uint8_t *buffer, int len);

int byte_array_producer_put_byte(byte_array_producer_t *b, uint8_t v);
int byte_array_producer_put_u32_be(byte_array_producer_t *b, uint32_t v);
int byte_array_producer_put_u24_be(byte_array_producer_t *b, uint32_t v);

#endif
