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

#ifndef ACTOR_H
#define ACTOR_H
#include "notified_fifo.h"

#define INIT_ACTOR(ptr, name, core_affinity) init_actor((Actor_t *)ptr, name, core_affinity);

#define DESTROY_ACTOR(ptr) destroy_actor((Actor_t *)ptr);

#define SHUTDOWN_ACTOR(ptr) shutdown_actor((Actor_t *)ptr);

typedef struct Actor_t {
  notifiedFIFO_t fifo;
  bool terminate;
  pthread_t thread;
} Actor_t;

/// @brief Initialize the actor. Starts actor thread
/// @param actor
/// @param name Actor name. Thread name will be derived from it
/// @param core_affinity Core affinity. Specify -1 for no affinity
void init_actor(Actor_t *actor, const char *name, int core_affinity);

/// @brief Destroy the actor. Free the memory, stop the thread.
/// @param actor
void destroy_actor(Actor_t *actor);

/// @brief Gracefully shutdown the actor, letting all tasks previously started finish
/// @param actor
void shutdown_actor(Actor_t *actor);

/// @brief This function will return when all current jobs in the queue are finished.
/// The caller should make sure no new jobs are added to the queue between this function call and return.
/// @param actor
void flush_actor(Actor_t *actor);

#endif
