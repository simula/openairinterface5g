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

#include <stdio.h>
#include <assert.h>
#include <threads.h>
#include <stdlib.h>
#include <gtest/gtest.h>

extern "C" {
#include "barrier.h"
void exit_function(const char *file, const char *function, const int line, const char *s, const int assert)
{
  if (assert) {
    abort();
  } else {
    exit(EXIT_SUCCESS);
  }
}
}

static int trigger_count = 0;
static int triggered = 0;
void f(void *arg)
{
  triggered = 1;
  trigger_count++;
}

TEST(dynamic_barrier, trigger_once)
{
  dynamic_barrier_t barrier;
  triggered = 0;
  dynamic_barrier_init(&barrier);
  dynamic_barrier_join(&barrier);
  dynamic_barrier_join(&barrier);
  dynamic_barrier_join(&barrier);
  dynamic_barrier_join(&barrier);
  dynamic_barrier_update(&barrier, 4, f, NULL);
  EXPECT_EQ(triggered, 1);
}

TEST(dynamic_barrier, trigger_twice)
{
  dynamic_barrier_t barrier;
  triggered = 0;
  dynamic_barrier_init(&barrier);
  dynamic_barrier_join(&barrier);
  dynamic_barrier_join(&barrier);
  dynamic_barrier_join(&barrier);
  dynamic_barrier_join(&barrier);
  dynamic_barrier_update(&barrier, 4, f, NULL);
  EXPECT_EQ(triggered, 1);
  triggered = 0;
  dynamic_barrier_init(&barrier);
  dynamic_barrier_join(&barrier);
  dynamic_barrier_join(&barrier);
  dynamic_barrier_join(&barrier);
  dynamic_barrier_join(&barrier);
  dynamic_barrier_update(&barrier, 4, f, NULL);
  EXPECT_EQ(triggered, 1);
}

int dynamic_barrier_thread(void *thr_data)
{
  dynamic_barrier_t *barrier = (dynamic_barrier_t *)thr_data;
  dynamic_barrier_join(barrier);
  return 0;
}

int dynamic_barrier_update_thread(void *thr_data)
{
  dynamic_barrier_t *barrier = (dynamic_barrier_t *)thr_data;
  dynamic_barrier_update(barrier, 3, f, NULL);
  return 0;
}

TEST(dynamic_barrier, multithreaded)
{
  thrd_t thr[3];
  dynamic_barrier_t barrier;
  trigger_count = 0;
  for (int i = 0; i < 300; i++) {
    dynamic_barrier_init(&barrier);
    thrd_t update_thread;
    thrd_create(&update_thread, dynamic_barrier_update_thread, &barrier);
    for (int n = 0; n < 3; ++n)
      thrd_create(&thr[n], dynamic_barrier_thread, &barrier);
    for (int n = 0; n < 3; ++n)
      thrd_join(thr[n], NULL);
    thrd_join(update_thread, NULL);
  }
  assert(trigger_count == 300);
}

int main(int argc, char **argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
