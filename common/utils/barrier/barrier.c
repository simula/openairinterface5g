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

#include "barrier.h"
#include "assertions.h"

void dynamic_barrier_init(dynamic_barrier_t* barrier)
{
  int ret = pthread_mutex_init(&barrier->barrier_lock, NULL);
  AssertFatal(ret == 0, "Mutex error %d", ret);
  barrier->callback = NULL;
  barrier->callback_arg = NULL;
  barrier->completed_jobs = 0;
}

void dynamic_barrier_reset(dynamic_barrier_t* barrier)
{
  barrier->callback = NULL;
  if (barrier->callback_arg != NULL) {
    free(barrier->callback_arg);
  }
  barrier->callback_arg = NULL;
  barrier->completed_jobs = 0;
}

void dynamic_barrier_join(dynamic_barrier_t* barrier)
{
  int ret = pthread_mutex_lock(&barrier->barrier_lock);
  AssertFatal(ret == 0, "Mutex error %d", ret);
  barrier->completed_jobs++;
  if (barrier->callback) {
    if (barrier->completed_jobs == barrier->max_joins) {
      barrier->completed_jobs = 0;
      barrier->callback(barrier->callback_arg);
      barrier->callback = NULL;
      barrier->callback_arg = NULL;
    }
  }
  ret = pthread_mutex_unlock(&barrier->barrier_lock);
  AssertFatal(ret == 0, "Mutex error %d", ret);
}

void dynamic_barrier_update(dynamic_barrier_t* barrier, int max_joins, callback_t callback, void* arg)
{
  int ret = pthread_mutex_lock(&barrier->barrier_lock);
  AssertFatal(ret == 0, "Mutex error %d", ret);
  if (barrier->completed_jobs == max_joins) {
    barrier->completed_jobs = 0;
    callback(arg);
  } else {
    barrier->max_joins = max_joins;
    barrier->callback = callback;
    barrier->callback_arg = arg;
  }
  ret = pthread_mutex_unlock(&barrier->barrier_lock);
  AssertFatal(ret == 0, "Mutex error %d", ret);
}
