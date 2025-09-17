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

#include "task_ans.h"
#include "assertions.h"
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include "pthread_utils.h"
#include "errno.h"
#include <string.h>

#define seminit(sem)                                                                           \
  {                                                                                            \
    int ret = sem_init(&sem, 0, 0);                                                            \
    AssertFatal(ret == 0, "sem_init(): ret=%d, errno=%d (%s)\n", ret, errno, strerror(errno)); \
  }
#define sempost(sem)                                                                           \
  {                                                                                            \
    int ret = sem_post(&sem);                                                                  \
    AssertFatal(ret == 0, "sem_post(): ret=%d, errno=%d (%s)\n", ret, errno, strerror(errno)); \
  }
#define semwait(sem)                                                                           \
  {                                                                                            \
    int ret = sem_wait(&sem);                                                                  \
    AssertFatal(ret == 0, "sem_wait(): ret=%d, errno=%d (%s)\n", ret, errno, strerror(errno)); \
  }
#define semdestroy(sem)                                                                           \
  {                                                                                               \
    int ret = sem_destroy(&sem);                                                                  \
    AssertFatal(ret == 0, "sem_destroy(): ret=%d, errno=%d (%s)\n", ret, errno, strerror(errno)); \
  }

void init_task_ans(task_ans_t* ans, uint num_jobs)
{
  ans->counter = num_jobs;
  seminit(ans->sem);
}

void completed_many_task_ans(task_ans_t* ans, uint num_completed_jobs)
{
  DevAssert(ans != NULL);
  // Using atomic counter in contention scenario to avoid locking in producers
  int num_jobs = atomic_fetch_sub_explicit(&ans->counter, num_completed_jobs, memory_order_relaxed);
  if (num_jobs == num_completed_jobs) {
    // Using semaphore to enable blocking call in join_task_ans
    sempost(ans->sem);
  }
}

void join_task_ans(task_ans_t* ans)
{
  semwait(ans->sem);
  semdestroy(ans->sem);
}
