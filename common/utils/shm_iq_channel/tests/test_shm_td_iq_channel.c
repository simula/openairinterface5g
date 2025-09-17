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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include "assertions.h"

#define SHM_CHANNEL_NAME "shm_iq_channel_test_file"
#include "shm_td_iq_channel.h"

void server(void);
int client(void);

enum { MODE_SERVER, MODE_CLIENT, MODE_FORK };
int main(int argc, char *argv[])
{
  int mode = MODE_FORK;
  if (argc == 2) {
    if (strcmp(argv[1], "server") == 0) {
      mode = MODE_SERVER;
    } else if (strcmp(argv[1], "client") == 0) {
      mode = MODE_CLIENT;
    }
  } else if (argc > 2) {
    printf("Usage: %s [server|client]\n", argv[0]);
    return 1;
  }
  switch (mode) {
    case MODE_SERVER:
      server();
      break;
    case MODE_CLIENT:
      return client();
      break;
    case MODE_FORK: {
      char file[256];
      snprintf(file, sizeof(file), "/dev/shm/%s", SHM_CHANNEL_NAME);
      remove(file);
      int pid = fork();
      if (pid == 0) {
        server();
      } else {
        return client();
      }
    } break;
  }

  return 0;
}

const int num_updates = 50;
const int num_samples_per_update = 30720;
void *produce_symbols(void *arg)
{
  ShmTDIQChannel *channel = (ShmTDIQChannel *)arg;
  for (int i = 0; i < num_updates; i++) {
    shm_td_iq_channel_produce_samples(channel, num_samples_per_update);
    usleep(20000);
  }
  return 0;
}

void server(void)
{
  int num_ant_tx = 1;
  int num_ant_rx = 1;
  ShmTDIQChannel *channel = shm_td_iq_channel_create(SHM_CHANNEL_NAME, num_ant_tx, num_ant_rx);

  pthread_t producer_thread;
  int ret = pthread_create(&producer_thread, NULL, produce_symbols, channel);
  AssertFatal(ret == 0, "pthread_create() failed: errno %d, %s\n", errno, strerror(errno));

  uint64_t timestamp = 0;
  int iq_contents = 0;
  while (timestamp < num_samples_per_update * num_updates) {
    shm_td_iq_channel_wait(channel, timestamp + num_samples_per_update, 0);
    uint64_t target_timestamp = timestamp + num_samples_per_update;
    timestamp += num_samples_per_update;
    uint32_t iq_data[num_samples_per_update];
    for (int i = 0; i < num_samples_per_update; i++) {
      iq_data[i] = iq_contents;
    }
    int result = shm_td_iq_channel_tx(channel, target_timestamp, num_samples_per_update, 0, iq_data);
    AssertFatal(result == CHANNEL_NO_ERROR, "Failed to write data\n");
    iq_contents++;
  }

  printf("Finished writing data\n");

  ret = pthread_join(producer_thread, NULL);
  AssertFatal(ret == 0, "pthread_join() failed: errno %d, %s\n", errno, strerror(errno));
  shm_td_iq_channel_destroy(channel);
}

int client(void)
{
  int total_errors = 0;
  ShmTDIQChannel *channel = shm_td_iq_channel_connect(SHM_CHANNEL_NAME, 10);

  uint64_t timestamp = 0;
  int iq_contents = 0;

  while (timestamp < num_samples_per_update * num_updates) {
    shm_td_iq_channel_wait(channel, timestamp + num_samples_per_update, 0);
    // Server starts producing from second slot
    if (timestamp > num_samples_per_update) {
      uint64_t target_timestamp = timestamp - num_samples_per_update;
      uint32_t iq_data[num_samples_per_update];
      int result = shm_td_iq_channel_rx(channel, target_timestamp, num_samples_per_update, 0, iq_data);
      AssertFatal(result == CHANNEL_NO_ERROR, "Failed to read data\n");
      int num_errors = 0;
      for (int i = 0; i < num_samples_per_update; i++) {
        if (iq_data[i] != iq_contents) {
          num_errors++;
        }
      }
      if (num_errors) {
        printf("Found %d errors, value = %d, reference = %d\n", num_errors, iq_data[0], iq_contents);
        total_errors += num_errors;
      }
      iq_contents++;
    }
    timestamp += num_samples_per_update;
  }

  printf("Finished reading data\n");
  shm_td_iq_channel_destroy(channel);
  return total_errors;
}
