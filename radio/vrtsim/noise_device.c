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
#include "assertions.h"
#include <pthread.h>
#include <errno.h>
#include "noise_device.h"

// Use block size equal to cache size
#define BLOCK_SIZE (64 / sizeof(float)) // 4 bytes per float
// Use a power of 2 for block size length
#define NUM_BLOCKS (8192)
#define NOISE_VECTOR_LENGTH (BLOCK_SIZE * NUM_BLOCKS)

static struct {
  pthread_t noise_device_thread;
  float noise[NOISE_VECTOR_LENGTH] __attribute__((aligned(64)));
  float noise_power;
  bool running;
} state;

static uint32_t jz, jsr = 123456789;
static uint32_t random_uint(void)
{
  return SHR3;
}

static void generate_one_block_noise(int write_block_index, float noise_power)
{
  // Generate noise vector
  float block[BLOCK_SIZE];
  for (int i = 0; i < BLOCK_SIZE; i++) {
    block[i] = noise_power * gaussZiggurat(0, 1);
  }
  // Write block to noise vector
  memcpy(state.noise + write_block_index * BLOCK_SIZE, block, sizeof(block));
}

void *noise_device_thread_function(void *arg)
{
  while (state.running) {
    // Generate noise vector
    generate_one_block_noise(random_uint() % NUM_BLOCKS, state.noise_power);
    usleep(10000);
  }
  return NULL;
}

void init_noise_device(float noise_power)
{
  for (int i = 0; i < NUM_BLOCKS; i++) {
    generate_one_block_noise(i, noise_power);
  }
  int ret = pthread_create(&state.noise_device_thread, NULL, noise_device_thread_function, NULL);
  AssertFatal(ret == 0, "pthread_create failed: %d, errno %d (%s)\n", ret, errno, strerror(errno));
}

void free_noise_device(void)
{
  state.running = false;
  int ret = pthread_join(state.noise_device_thread, NULL);
  AssertFatal(ret == 0, "pthread_join failed: %d, errno %d (%s)\n", ret, errno, strerror(errno));
}

void get_noise_vector(float *noise_vector, int length)
{
  int start_block = random_uint() % NUM_BLOCKS;
  while (length > 0) {
    int copy_size = min(length, (NUM_BLOCKS - start_block) * BLOCK_SIZE);
    memcpy(noise_vector, &state.noise[start_block * BLOCK_SIZE], copy_size * sizeof(float));
    start_block = 0;
    length -= copy_size;
    noise_vector += copy_size;
  }
}
