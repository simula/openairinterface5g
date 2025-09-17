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

#ifndef BOUNDED_NOTIFIED_FIFO_H
#define BOUNDED_NOTIFIED_FIFO_H

#include "assertions.h"
#include <stdint.h>
#include <memory.h>
#include "task.h"
#include <pthread.h>
#include "pthread_utils.h"

// For working correctly, maintain the default elements to a 2^N e.g., 2^5=32
#define DEFAULT_ELM 256

typedef struct seq_ring_buf_s {
  task_t* array;
  size_t cap;
  uint32_t head;
  uint32_t tail;
  _Atomic uint64_t sz;
} seq_ring_task_t;


static size_t size_seq_ring_task(seq_ring_task_t* r)
{
  DevAssert(r != NULL);

  return r->head - r->tail;
}

static uint32_t mask(uint32_t cap, uint32_t val)
{
  return val & (cap - 1);
}

static bool full(seq_ring_task_t* r)
{
  return size_seq_ring_task(r) == r->cap - 1;
}

static void enlarge_buffer(seq_ring_task_t* r)
{
  DevAssert(r != NULL);
  DevAssert(full(r));

  const uint32_t factor = 2;
  task_t* tmp_buffer = calloc(r->cap * factor, sizeof(task_t));
  DevAssert(tmp_buffer != NULL);

  const uint32_t head_pos = mask(r->cap, r->head);
  const uint32_t tail_pos = mask(r->cap, r->tail);

  if (head_pos > tail_pos) {
    memcpy(tmp_buffer, r->array + tail_pos, (head_pos - tail_pos) * sizeof(task_t));
  } else {
    memcpy(tmp_buffer, r->array + tail_pos, (r->cap - tail_pos) * sizeof(task_t));
    memcpy(tmp_buffer + (r->cap - tail_pos), r->array, head_pos * sizeof(task_t));
  }
  r->cap *= factor;
  free(r->array);
  r->array = tmp_buffer;
  r->tail = 0;
  r->head = r->cap / 2 - 1;
}

static void init_seq_ring_task(seq_ring_task_t* r)
{
  DevAssert(r != NULL);
  task_t* tmp_buffer = calloc(DEFAULT_ELM, sizeof(task_t));
  DevAssert(tmp_buffer != NULL);
  seq_ring_task_t tmp = {.array = tmp_buffer, .head = 0, .tail = 0, .cap = DEFAULT_ELM};
  memcpy(r, &tmp, sizeof(seq_ring_task_t));
  r->sz = 0;
}

static void free_seq_ring_task(seq_ring_task_t* r)
{
  DevAssert(r != NULL);
  free(r->array);
}

static void push_back_seq_ring_task(seq_ring_task_t* r, task_t t)
{
  DevAssert(r != NULL);

  if (full(r))
    enlarge_buffer(r);

  const uint32_t pos = mask(r->cap, r->head);
  r->array[pos] = t;
  r->head += 1;
  r->sz += 1;
}

static task_t pop_seq_ring_task(seq_ring_task_t* r)
{
  DevAssert(r != NULL);
  DevAssert(size_seq_ring_task(r) > 0);

  const uint32_t pos = mask(r->cap, r->tail);
  task_t t = r->array[pos];
  r->tail += 1;
  r->sz -= 1;
  return t;
}

#undef DEFAULT_ELM

typedef struct {
  pthread_mutex_t mtx;
  pthread_cond_t cv;
  seq_ring_task_t r;
  size_t idx;
} not_q_t;

typedef struct {
  task_t t;
  bool success;
} ret_try_t;

static void init_not_q(not_q_t* q, size_t idx)
{
  DevAssert(q != NULL);

  q->idx = idx;

  init_seq_ring_task(&q->r);

  mutexinit(q->mtx);
  condinit(q->cv);
}

static void free_not_q(not_q_t* q)
{
  DevAssert(q != NULL);

  free_seq_ring_task(&q->r);

  mutexdestroy(q->mtx);
  conddestroy(q->cv);
}

static bool try_push_not_q(not_q_t* q, task_t t)
{
  DevAssert(q != NULL);


  if (mutextrylock(q->mtx) != 0)
    return false;

  push_back_seq_ring_task(&q->r, t);

  const size_t sz = size_seq_ring_task(&q->r);
  DevAssert(sz > 0);

  mutexunlock(q->mtx);

  condsignal(q->cv);

  return true;
}

static void push_not_q(not_q_t* q, task_t t)
{
  DevAssert(q != NULL);
  DevAssert(t.func != NULL);

  mutexlock(q->mtx);

  push_back_seq_ring_task(&q->r, t);

  DevAssert(size_seq_ring_task(&q->r) > 0);

  mutexunlock(q->mtx);

  condsignal(q->cv);
}

static ret_try_t try_pop_not_q(not_q_t* q)
{
  DevAssert(q != NULL);

  ret_try_t ret = {.success = false};

  int rc = mutextrylock(q->mtx);
  DevAssert(rc == 0 || rc == EBUSY);

  if (rc == EBUSY)
    return ret;

  size_t sz = size_seq_ring_task(&q->r);
  if (sz == 0) {
    mutexunlock(q->mtx);

    return ret;
  }

  DevAssert(sz > 0);
  ret.t = pop_seq_ring_task(&q->r);

  mutexunlock(q->mtx);
  ret.success = true;

  return ret;
}

static bool pop_not_q(not_q_t* q, ret_try_t* out)
{
  DevAssert(q != NULL);
  DevAssert(out != NULL);

  mutexlock(q->mtx);

  while (size_seq_ring_task(&q->r) == 0) {
    condwait(q->cv, q->mtx);
  }

  out->t = pop_seq_ring_task(&q->r);

  mutexunlock(q->mtx);

  return true;
}

#endif
