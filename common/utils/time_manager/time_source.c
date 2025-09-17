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

#include "time_source.h"

#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>

#include "common/utils/assertions.h"
#include "common/utils/system.h"
#include "common/utils/utils.h"

typedef void (*callback_function_t)(void *callback_data);
typedef void * callback_data_t;

typedef struct {
  time_source_type_t type;
  volatile callback_function_t callback;
  volatile callback_data_t callback_data;
  _Atomic bool exit;
  void (*terminate)(void *this);
  pthread_t thread_id;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} time_source_common_t;

typedef struct {
  time_source_common_t common;
} time_source_realtime_t;

typedef struct {
  time_source_common_t common;
  volatile uint64_t iq_samples_count;
  volatile uint64_t iq_samples_per_second;
} time_source_iq_t;

static void init_mutex(time_source_common_t *t)
{
  int ret = pthread_mutex_init(&t->mutex, NULL);
  DevAssert(ret == 0);
}

static void init_cond(time_source_common_t *t)
{
  int ret = pthread_cond_init(&t->cond, NULL);
  DevAssert(ret == 0);
}

static void lock(time_source_common_t *t)
{
  int ret = pthread_mutex_lock(&t->mutex);
  DevAssert(ret == 0);
}

static void unlock(time_source_common_t *t)
{
  int ret = pthread_mutex_unlock(&t->mutex);
  DevAssert(ret == 0);
}

static void condwait(time_source_common_t *t)
{
  int ret = pthread_cond_wait(&t->cond, &t->mutex);
  DevAssert(ret == 0);
}

static void condsignal(time_source_common_t *t)
{
  int ret = pthread_cond_signal(&t->cond);
  DevAssert(ret == 0);
}

static void wait_thread_termination(time_source_common_t *t)
{
  void *retval;
  int ret = pthread_join(t->thread_id, &retval);
  DevAssert(ret == 0);
}

static struct timespec next_ms(struct timespec t)
{
  /* go to next millisecond */
  t.tv_nsec += 1000000;
  if (t.tv_nsec > 999999999) {
    t.tv_nsec -= 1000000000;
    t.tv_sec++;
  }

  return t;
}

static void *time_source_realtime_thread(void *ts)
{
  time_source_realtime_t *time_source = ts;

  struct timespec now;
  if (clock_gettime(CLOCK_MONOTONIC, &now) == -1)
    AssertFatal(0, "clock_gettime failed\n");

  /* go to next millisecond */
  now = next_ms(now);

  while (!time_source->common.exit) {
    /* sleep */
    if (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &now, NULL) == -1) {
      /* accept interruption by signals and then simply restart the sleep */
      if (errno == EINTR)
        continue;
      AssertFatal(0, "clock_nanosleep failed\n");
    }

    if (time_source->common.exit)
      break;

    /* tick */
    lock(&time_source->common);
    void (*callback)(void *callback_data) = time_source->common.callback;
    void *callback_data = time_source->common.callback_data;
    unlock(&time_source->common);
    if (callback != NULL)
      callback(callback_data);

    /* go to next millisecond */
    now = next_ms(now);
  }

  return (void *)0;
}

static void *time_source_iq_samples_thread(void *ts)
{
  time_source_iq_t *time_source = ts;

  while (!time_source->common.exit) {
    /* wait until we have at least 1ms of IQ samples */
    lock(&time_source->common);
    while (!time_source->common.exit
           && (time_source->iq_samples_per_second == 0
              || time_source->iq_samples_count < time_source->iq_samples_per_second / 1000))
      condwait(&time_source->common);
    time_source->iq_samples_count -= time_source->iq_samples_per_second / 1000;
    unlock(&time_source->common);

    if (time_source->common.exit)
      break;

    /* tick */
    lock(&time_source->common);
    void (*callback)(void *callback_data) = time_source->common.callback;
    void *callback_data = time_source->common.callback_data;
    unlock(&time_source->common);
    if (callback != NULL)
      callback(callback_data);
  }

  return (void *)0;
}

static void time_source_iq_samples_terminate(void *ts)
{
  time_source_iq_t *time_source = ts;
  condsignal(&time_source->common);
}

time_source_t *new_time_source(time_source_type_t type)
{
  time_source_t *ret = NULL;
  void *(*thread_function)(void *) = NULL;
  void (*terminate_function)(void *) = NULL;
  char *thread_function_name = NULL;

  switch (type) {
    case TIME_SOURCE_REALTIME: {
      time_source_realtime_t *ts = calloc_or_fail(1, sizeof(*ts));
      thread_function = time_source_realtime_thread;
      thread_function_name = "time source realtime";
      ret = ts;
      break;
    }
    case TIME_SOURCE_IQ_SAMPLES: {
      time_source_iq_t *ts = calloc_or_fail(1, sizeof(*ts));
      thread_function = time_source_iq_samples_thread;
      terminate_function = time_source_iq_samples_terminate;
      thread_function_name = "time source iq samples";
      ret = ts;
      break;
    }
  }

  time_source_common_t *c = ret;
  c->type = type;
  init_mutex(c);
  init_cond(c);
  c->terminate = terminate_function;
  threadCreate(&c->thread_id, thread_function, c, thread_function_name, -1, SCHED_OAI);

  return ret;
}

void free_time_source(time_source_t *ts)
{
  time_source_common_t *time_source = ts;
  time_source->exit = true;
  if (time_source->terminate != NULL)
    time_source->terminate(time_source);
  wait_thread_termination(time_source);
  free(time_source);
}

void time_source_set_callback(time_source_t *ts,
                              void (*callback)(void *),
                              void *callback_data)
{
  time_source_common_t *time_source = ts;
  lock(time_source);
  time_source->callback = callback;
  time_source->callback_data = callback_data;
  unlock(time_source);
}

void time_source_iq_add(time_source_t *ts,
                        uint64_t iq_samples_count,
                        uint64_t iq_samples_per_second)
{
  if (ts == NULL)
    return;

  time_source_iq_t *time_source = ts;

  if (time_source->common.type != TIME_SOURCE_IQ_SAMPLES)
    return;

  lock(&time_source->common);

  time_source->iq_samples_count += iq_samples_count;

  if (time_source->iq_samples_per_second == 0)
    time_source->iq_samples_per_second = iq_samples_per_second;
  AssertFatal(time_source->iq_samples_per_second == iq_samples_per_second, "unsupported change of value 'IQ samples per second'\n");

  condsignal(&time_source->common);
  unlock(&time_source->common);
}
