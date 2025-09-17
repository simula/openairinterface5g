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

#ifndef COMMON_UTIL_TIME_MANAGER_TIME_SOURCE
#define COMMON_UTIL_TIME_MANAGER_TIME_SOURCE

#include <stdint.h>

/* opaque data type */
typedef void time_source_t;

typedef enum {
  TIME_SOURCE_REALTIME,
  TIME_SOURCE_IQ_SAMPLES
} time_source_type_t;

time_source_t *new_time_source(time_source_type_t type);

void free_time_source(time_source_t *time_source);

void time_source_set_callback(time_source_t *time_source,
                              void (*callback)(void *),
                              void *callback_data);

void time_source_iq_add(time_source_t *time_source,
                        uint64_t iq_samples_count,
                        uint64_t iq_samples_per_second);

#endif /* COMMON_UTIL_TIME_MANAGER_TIME_SOURCE */
