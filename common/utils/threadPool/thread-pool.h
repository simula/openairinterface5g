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

#ifndef THREAD_POOL_H
#define THREAD_POOL_H
#include <stdbool.h>
#include <stdint.h>
#include <malloc.h>
#include <stdalign.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>
#include "assertions.h"
#include "common/utils/time_meas.h"
#include "common/utils/system.h"
#include "task.h"
#include "pthread_utils.h"

#ifdef DEBUG
  #define THREADINIT   PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
#else
  #define THREADINIT   PTHREAD_MUTEX_INITIALIZER
#endif

typedef struct {
  pthread_t* t_arr;
  size_t len_thr;

  _Atomic(uint64_t) index;

  void* q_arr;

  pthread_barrier_t barrier;
} tpool_t;

/// @brief Push job to threadpool. May run task inline in case there are no worker threads
///        defined for threadpool
/// @param tpool threadpool to use
/// @param task task description
void pushTpool(tpool_t *tpool, task_t task);

/// @brief Abort tpool, stop all threads and return
/// @param t 
void abortTpool(tpool_t *t);

/// @brief Initialize a threadPool.
/// @param params A string in a form of "<int>,<int>,<int>,...,<int>" or "n"
///               if params is "n" threadpool is disabled and all functions are called inline.
///               else if params is a list of comma separated integers, the number of threads
///               is equal to the list length and each thread is pinned to a core indicated by
///               the integer value. If -1 is specified thread is not pinned to any core.
/// @param pool Theadpool
/// @param performanceMeas unused
/// @param name Name of the theadpool, will be used as pthread names for the pool worker threads
void initNamedTpool(char *params,tpool_t *pool, bool performanceMeas, char *name);

/// @brief Initialize a threadpool with floating core worker threads only
/// @param nbThreads number of floating core worker threads
/// @param pool Threadpool
/// @param performanceMeas unused 
/// @param name Name of the theadpool, will be used as pthread names for the pool worker threads
void initFloatingCoresTpool(int nbThreads,tpool_t *pool, bool performanceMeas, char *name);

/// Convenience macro
#define  initTpool(PARAMPTR,TPOOLPTR, MEASURFLAG) initNamedTpool(PARAMPTR,TPOOLPTR, MEASURFLAG, NULL)
#endif
