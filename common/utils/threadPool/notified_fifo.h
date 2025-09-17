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

#ifndef NOTIFIED_FIFO_H
#define NOTIFIED_FIFO_H
#include "pthread_utils.h"
#include <stdint.h>
#include <malloc.h>
#include <pthread.h>
#include "time_meas.h"
#include <memory.h>
#include <stdalign.h>
#include "assertions.h"

/// @brief Element on the notifiedFifo_t
/// next: internal FIFO chain, do not set it
/// key: a long int that the client can use to identify a job or a group of messages
/// ResponseFifo: if the client defines a response FIFO, the job will be posted back after processing
/// processingFunc: any function (of type void processingFunc(void *)) that a worker will process
/// msgData: the data passed to `processingFunc`. It can be added automatically, or you can set it to a buffer you are managing
/// malloced: a boolean that enables internal free in the case of no return FIFO or abort feature
typedef struct notifiedFIFO_elt_s {
  struct notifiedFIFO_elt_s *next;
  uint64_t key; // To filter out elements
  struct notifiedFIFO_s *reponseFifo;
  void (*processingFunc)(void *);
  bool malloced;
  oai_cputime_t creationTime;
  oai_cputime_t startProcessingTime;
  oai_cputime_t endProcessingTime;
  oai_cputime_t returnTime;
  // use alignas(32) to align msgData to 32b
  // user data behind it will be aligned to 32b as well
  // important! this needs to be the last member in the struct
  alignas(32) void *msgData;
} notifiedFIFO_elt_t;

typedef struct notifiedFIFO_s {
  notifiedFIFO_elt_t *outF;
  notifiedFIFO_elt_t *inF;
  pthread_mutex_t lockF;
  pthread_cond_t notifF;
  bool abortFIFO; // if set, the FIFO always returns NULL -> abort condition
} notifiedFIFO_t;

/// @brief Creates a new job.
/// @param size The data part of the job will have this size
/// @param key the job can be identified
/// @param reponseFifo response fifo
/// @param processingFunc function to call
/// @return new notifiedFIFO_elt_t element with extra memory allocated at the end equal to size.
static inline notifiedFIFO_elt_t *newNotifiedFIFO_elt(int size,
                                                      uint64_t key,
                                                      notifiedFIFO_t *reponseFifo,
                                                      void (*processingFunc)(void *))
{
  notifiedFIFO_elt_t *ret = (notifiedFIFO_elt_t *)memalign(32, sizeof(notifiedFIFO_elt_t) + size);
  AssertFatal(NULL != ret, "out of memory\n");
  ret->next = NULL;
  ret->key = key;
  ret->reponseFifo = reponseFifo;
  ret->processingFunc = processingFunc;
  // We set user data piece aligend 32 bytes to be able to process it with SIMD
  // msgData is aligned to 32bytes, so everything after will be as well
  ret->msgData = ((uint8_t *)ret) + sizeof(notifiedFIFO_elt_t);
  ret->malloced = true;
  return ret;
}

/// @brief Get pointer to the data carried by notifiedFIFO_elt_t
/// @param elt
/// @return void pointer to the data allocated with the message
static inline void *NotifiedFifoData(notifiedFIFO_elt_t *elt)
{
  return elt->msgData;
}

/// @brief Delete a notifiedFIFO_elt_t if its allocated
/// @param elt
static inline void delNotifiedFIFO_elt(notifiedFIFO_elt_t *elt)
{
  if (elt->malloced) {
    elt->malloced = false;
    free(elt);
  }
}

static inline void initNotifiedFIFO_nothreadSafe(notifiedFIFO_t *nf)
{
  nf->inF = NULL;
  nf->outF = NULL;
  nf->abortFIFO = false;
}
static inline void initNotifiedFIFO(notifiedFIFO_t *nf)
{
  mutexinit(nf->lockF);
  condinit(nf->notifF);
  initNotifiedFIFO_nothreadSafe(nf);
  // No delete function: the creator has only to free the memory
}

static inline void pushNotifiedFIFO_nothreadSafe(notifiedFIFO_t *nf, notifiedFIFO_elt_t *msg)
{
  msg->next = NULL;

  if (nf->outF == NULL)
    nf->outF = msg;

  if (nf->inF != NULL)
    nf->inF->next = msg;

  nf->inF = msg;
}

static inline void pushNotifiedFIFO(notifiedFIFO_t *nf, notifiedFIFO_elt_t *msg)
{
  mutexlock(nf->lockF);
  if (!nf->abortFIFO) {
    pushNotifiedFIFO_nothreadSafe(nf, msg);
    condsignal(nf->notifF);
  }
  mutexunlock(nf->lockF);
}

static inline notifiedFIFO_elt_t *pullNotifiedFIFO_nothreadSafe(notifiedFIFO_t *nf)
{
  if (nf->outF == NULL)
    return NULL;
  if (nf->abortFIFO)
    return NULL;

  notifiedFIFO_elt_t *ret = nf->outF;

  AssertFatal(nf->outF != nf->outF->next, "Circular list in thread pool: push several times the same buffer is forbidden\n");

  nf->outF = nf->outF->next;

  if (nf->outF == NULL)
    nf->inF = NULL;

  return ret;
}

static inline notifiedFIFO_elt_t *pullNotifiedFIFO(notifiedFIFO_t *nf)
{
  mutexlock(nf->lockF);
  notifiedFIFO_elt_t *ret = NULL;

  while ((ret = pullNotifiedFIFO_nothreadSafe(nf)) == NULL && !nf->abortFIFO)
    condwait(nf->notifF, nf->lockF);

  mutexunlock(nf->lockF);
  return ret;
}

static inline notifiedFIFO_elt_t *pollNotifiedFIFO(notifiedFIFO_t *nf)
{
  int tmp = mutextrylock(nf->lockF);

  if (tmp != 0)
    return NULL;

  if (nf->abortFIFO) {
    mutexunlock(nf->lockF);
    return NULL;
  }

  notifiedFIFO_elt_t *ret = pullNotifiedFIFO_nothreadSafe(nf);
  mutexunlock(nf->lockF);
  return ret;
}

static inline time_stats_t exec_time_stats_NotifiedFIFO(const notifiedFIFO_elt_t *elt)
{
  time_stats_t ts = {0};
  if (elt->startProcessingTime == 0 && elt->endProcessingTime == 0)
    return ts; /* no measurements done */
  ts.in = elt->startProcessingTime;
  ts.diff = elt->endProcessingTime - ts.in;
  ts.p_time = ts.diff;
  ts.diff_square = ts.diff * ts.diff;
  ts.max = ts.diff;
  ts.trials = 1;
  return ts;
}

// This functions aborts all messages in the queue, and marks the queue as
// "aborted", such that every call to it will return NULL
static inline void abortNotifiedFIFO(notifiedFIFO_t *nf)
{
  mutexlock(nf->lockF);
  nf->abortFIFO = true;
  notifiedFIFO_elt_t **elt = &nf->outF;
  while (*elt != NULL) {
    notifiedFIFO_elt_t *p = *elt;
    *elt = (*elt)->next;
    delNotifiedFIFO_elt(p);
  }

  if (nf->outF == NULL)
    nf->inF = NULL;
  condbroadcast(nf->notifF);
  mutexunlock(nf->lockF);
}

#endif
