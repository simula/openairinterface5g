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

#ifndef _BARRIER_H_
#define _BARRIER_H_
#include <stdint.h>
#include <pthread.h>

typedef void (*callback_t)(void*);

/// @brief This thread barrier allows modifying the maximum joins after it is initialized
typedef struct dynamic_barrier_t {
  pthread_mutex_t barrier_lock;
  int completed_jobs;
  int max_joins;
  callback_t callback;
  void* callback_arg;
} dynamic_barrier_t;

/// @brief Initialize the barrier
/// @param barrier
void dynamic_barrier_init(dynamic_barrier_t* barrier);

/// @brief Reset the barrier
/// @param barrier
void dynamic_barrier_reset(dynamic_barrier_t* barrier);

/// @brief Perform join on the barrier. May run callback if it is already set
/// @param barrier
void dynamic_barrier_join(dynamic_barrier_t* barrier);

/// @brief Set callback and max_joins. May run callback if the number of joins is already equal to max_joins
/// @param barrier
/// @param max_joins maximum number of joins before barrier triggers
/// @param callback callback function
/// @param arg callback function argument
void dynamic_barrier_update(dynamic_barrier_t* barrier, int max_joins, callback_t callback, void* arg);

#endif
