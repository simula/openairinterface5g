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

#include "thread-pool.h"
#include "actor.h"
#include "system.h"
#include "assertions.h"

void *actor_thread(void *arg);

void init_actor(Actor_t *actor, const char *name, int core_affinity)
{
  actor->terminate = false;
  initNotifiedFIFO(&actor->fifo);
  char actor_name[16];
  snprintf(actor_name, sizeof(actor_name), "%s%s", name, "_actor");
  threadCreate(&actor->thread, actor_thread, (void *)actor, actor_name, core_affinity, OAI_PRIORITY_RT_MAX);
}

/// @brief Main actor thread
/// @param arg actor pointer
/// @return NULL
void *actor_thread(void *arg)
{
  Actor_t *actor = arg;

  // Infinite loop to process requests
  do {
    notifiedFIFO_elt_t *elt = pullNotifiedFIFO(&actor->fifo);
    if (elt == NULL) {
      AssertFatal(actor->terminate, "pullNotifiedFIFO() returned NULL\n");
      break;
    }

    if (elt->processingFunc) // processing function can be NULL
      elt->processingFunc(NotifiedFifoData(elt));
    if (elt->reponseFifo) {
      pushNotifiedFIFO(elt->reponseFifo, elt);
    } else
      delNotifiedFIFO_elt(elt);
  } while (!actor->terminate);

  return NULL;
}

void destroy_actor(Actor_t *actor)
{
  actor->terminate = true;
  abortNotifiedFIFO(&actor->fifo);
  pthread_join(actor->thread, NULL);
}

static void self_destruct_fun(void *arg) {
  Actor_t *actor = arg;
  actor->terminate = true;
  abortNotifiedFIFO(&actor->fifo);
}

void shutdown_actor(Actor_t *actor)
{
  notifiedFIFO_t response_fifo;
  initNotifiedFIFO(&response_fifo);
  notifiedFIFO_elt_t *elt = newNotifiedFIFO_elt(0, 0, &response_fifo, self_destruct_fun);
  elt->msgData = actor;
  pushNotifiedFIFO(&actor->fifo, elt);
  elt = pullNotifiedFIFO(&response_fifo);
  delNotifiedFIFO_elt(elt);
  abortNotifiedFIFO(&response_fifo);
  pthread_join(actor->thread, NULL);
}

void flush_actor(Actor_t *actor)
{
  notifiedFIFO_t response_fifo;
  initNotifiedFIFO(&response_fifo);
  notifiedFIFO_elt_t *elt = newNotifiedFIFO_elt(0, 0, &response_fifo, NULL);
  pushNotifiedFIFO(&actor->fifo, elt);
  elt = pullNotifiedFIFO(&response_fifo);
  delNotifiedFIFO_elt(elt);
  abortNotifiedFIFO(&response_fifo);
}
