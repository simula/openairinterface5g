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
#include <nanomsg/nn.h>
#include <nanomsg/pubsub.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "pthread.h"
#include "taps_generated.h"
#include "SIMULATION/TOOLS/sim.h"
extern "C" {
#include "assertions.h"
#include "common/utils/LOG/log.h"
#include "sim.h"
#include "utils.h"
}
#include <cfloat>

#define NUM_TAPS_BUFFERS 4
#define MAX_TAPS_LEN 100
#define MAX_TAPS_MSG_SIZE (sizeof(struct complexf) * MAX_TAPS_LEN * 4 * 4 + 20)

static pthread_t client_thread;
static bool should_run = true;
typedef struct {
  int id;
  int sock;
  uint32_t num_tx_antennas;
  uint32_t num_rx_antennas;
  channel_desc_t **channel_desc;
} client_thread_args_t;

typedef struct {
  void *taps_msg;
  channel_desc_t *channel_desc;
} taps_buffer_t;

typedef struct {
  taps_buffer_t taps_buffers[NUM_TAPS_BUFFERS];
  int current_buffer;
} taps_storage_t;

void ascii_line_plot(const float *data, size_t size, char *buffer)
{
  const char levels[] = "_.-=#"; // ASCII characters for different levels
  size_t num_levels = sizeof(levels) - 1; // Number of levels (excluding the null terminator)

  // Calculate min and max values from the data
  float min_val = FLT_MAX;
  float max_val = FLT_MIN;
  for (size_t i = 0; i < size; i++) {
    if (data[i] < min_val)
      min_val = data[i];
    if (data[i] > max_val)
      max_val = data[i];
  }

  // Handle edge case where all values are the same
  if (min_val == max_val) {
    min_val -= 1.0f;
    max_val += 1.0f;
  }

  // Normalize and map data to levels
  for (size_t i = 0; i < size; i++) {
    float normalized = (data[i] - min_val) / (max_val - min_val); // Normalize to [0, 1]
    size_t level_index = (size_t)(normalized * num_levels); // Map to level index
    if (level_index > num_levels)
      level_index = num_levels; // Clamp to valid range
    snprintf(buffer + i, 2, "%c", levels[level_index]);
  }
}

static void init_taps_storage(taps_storage_t *storage, int num_tx_antennas, int num_rx_antennas)
{
  for (int i = 0; i < NUM_TAPS_BUFFERS; i++) {
    storage->taps_buffers[i].taps_msg = calloc_or_fail(1, MAX_TAPS_MSG_SIZE);
    storage->taps_buffers[i].channel_desc = (channel_desc_t *)calloc_or_fail(1, sizeof(channel_desc_t));
    storage->taps_buffers[i].channel_desc->ch_ps =
        (struct complexf **)calloc_or_fail(num_rx_antennas * num_tx_antennas, sizeof(struct complexf *));
  }
  storage->current_buffer = 0;
}

static taps_storage_t taps_storage;

void *client_thread_func(void *args)
{
  client_thread_args_t *client_thread_args = (client_thread_args_t *)args;
  while (should_run) {
    int next_buffer = (taps_storage.current_buffer + 1) % NUM_TAPS_BUFFERS;
    taps_buffer_t *taps_buffer = &taps_storage.taps_buffers[next_buffer];
    int ret = nn_recv(client_thread_args->sock, taps_buffer->taps_msg, MAX_TAPS_MSG_SIZE, NN_DONTWAIT);
    if (ret < 0) {
      if (errno == EAGAIN) {
      // Timeout: no message available, sleep briefly and continue
      usleep(100);
      continue;
      }
      LOG_E(HW, "nn_recv() failed: errno: %d, %s\n", errno, strerror(errno));
      continue;
    }
    auto taps_message = Phy::GetTaps(taps_buffer->taps_msg);
    if (taps_message->num_rx_antennas() != client_thread_args->num_rx_antennas
        || taps_message->num_tx_antennas() != client_thread_args->num_tx_antennas) {
      LOG_E(HW,
            "Number of antennas mismatch: expected %d x %d, got %d x %d\n",
            client_thread_args->num_rx_antennas,
            client_thread_args->num_tx_antennas,
            taps_message->num_rx_antennas(),
            taps_message->num_tx_antennas());
      continue;
    }
    channel_desc_t *channel_desc = taps_buffer->channel_desc;
    const flatbuffers::Vector<float> *taps = taps_message->taps();
    struct complexf *base_pointer = (struct complexf *)taps->data();
    int taps_len = taps_message->taps_len();
    AssertFatal(taps_len < MAX_TAPS_LEN, "Too many samples in the taps array, taps_len = %d\n", taps_len);
    for (unsigned int aarx = 0U; aarx < client_thread_args->num_rx_antennas; aarx++) {
      for (unsigned int aatx = 0U; aatx < client_thread_args->num_tx_antennas; aatx++) {
        channel_desc->ch_ps[aarx + (client_thread_args->num_rx_antennas * aatx)] =
            &base_pointer[(aarx + (client_thread_args->num_rx_antennas * aatx)) * taps_len];
      }
    }
    channel_desc->path_loss_dB = 0;
    channel_desc->channel_length = taps_len;
    *client_thread_args->channel_desc = channel_desc;
    taps_storage.current_buffer = next_buffer;
    LOG_A(HW, "Receved new taps message, channel_length %d, buffer %d\n", channel_desc->channel_length, next_buffer);
    for (unsigned int aarx = 0; aarx < client_thread_args->num_rx_antennas; aarx++) {
      for (unsigned int aatx = 0; aatx < client_thread_args->num_tx_antennas; aatx++) {
        char buffer[MAX_TAPS_LEN + 1];
        memset(buffer, 0, sizeof(buffer));
        float magnitudes[MAX_TAPS_LEN];
        cf_t *channel = channel_desc->ch_ps[aarx + (client_thread_args->num_rx_antennas * aatx)];
        for (int i = 0; i < channel_desc->channel_length; i++) {
          magnitudes[i] = sqrtf(powf(channel[i].r, 2) + powf(channel[i].i, 2));
        }
        ascii_line_plot(magnitudes, channel_desc->channel_length, buffer);
        LOG_A(HW, "Taps message %d, channel %d x %d: %s\n", client_thread_args->id, aarx, aatx, buffer);
      }
    }
  }
  return NULL;
}

extern "C" void taps_client_connect(int id,
                                    const char *socket_path,
                                    int num_tx_antennas,
                                    int num_rx_antennas,
                                    channel_desc_t **channel_desc)
{
  // Create a socket
  int sock = nn_socket(AF_SP, NN_SUB);
  AssertFatal(sock >= 0, "nn_socket() failed: errno: %d, %s\n", errno, strerror(errno));

  int ret = nn_connect(sock, socket_path);
  AssertFatal(ret >= 0, "nn_connect() failed: errno: %d, %s\n", errno, strerror(errno));

  // Subscribe to all messages
  ret = nn_setsockopt(sock, NN_SUB, NN_SUB_SUBSCRIBE, "", 0);
  AssertFatal(ret == 0, "nn_setsockopt() failed, errno %d, %s\n", errno, strerror(errno));

  init_taps_storage(&taps_storage, num_tx_antennas, num_rx_antennas);

  client_thread_args_t *client_thread_args = static_cast<client_thread_args_t *>(malloc(sizeof(client_thread_args_t)));
  client_thread_args->id = id;
  client_thread_args->sock = sock;
  client_thread_args->num_rx_antennas = num_rx_antennas;
  client_thread_args->num_tx_antennas = num_tx_antennas;
  client_thread_args->channel_desc = channel_desc;
  ret = pthread_create(&client_thread, NULL, client_thread_func, client_thread_args);
  AssertFatal(ret == 0, "pthread_create() failed: errno: %d, %s\n", errno, strerror(errno));
}

extern "C" void taps_client_stop()
{
  should_run = false;
  pthread_join(client_thread, NULL);
  for (int i = 0; i < NUM_TAPS_BUFFERS; i++) {
    free(taps_storage.taps_buffers[i].taps_msg);
    free(taps_storage.taps_buffers[i].channel_desc->ch_ps);
    free(taps_storage.taps_buffers[i].channel_desc);
  }
}
