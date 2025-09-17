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

#include "shm_td_iq_channel.h"
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "assertions.h"
#include "utils.h"
#include "common/utils/threadPool/pthread_utils.h"

#define CIRCULAR_BUFFER_SIZE (30720 * 14 * 20)

typedef struct {
  int magic;
  int num_antennas_tx;
  int num_antennas_rx;
  uint64_t timestamp;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} ShmTDIQChannelData;

typedef struct ShmTDIQChannel_s {
  IQChannelType type;
  ShmTDIQChannelData *data;
  char name[256];
  sample_t *tx_iq_data;
  sample_t *rx_iq_data;
  bool abort;
} ShmTDIQChannel;

ShmTDIQChannel *shm_td_iq_channel_create(const char *name, int num_tx_ant, int num_rx_ant)
{
  // Create shared memory segment
  int fd = shm_open(name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  AssertFatal(fd != -1, "shm_open failed: %s\n", strerror(errno));
  size_t tx_buffer_size = CIRCULAR_BUFFER_SIZE * sizeof(sample_t) * num_tx_ant;
  size_t rx_buffer_size = CIRCULAR_BUFFER_SIZE * sizeof(sample_t) * num_rx_ant;
  size_t total_size = sizeof(ShmTDIQChannelData) + tx_buffer_size + rx_buffer_size;

  // Set the size of the shared memory segment
  int res = ftruncate(fd, total_size);
  AssertFatal(res != -1, "ftruncate failed: %s\n", strerror(errno));

  // Map shared memory segment to address space
  ShmTDIQChannelData *shm_ptr = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  AssertFatal(shm_ptr != MAP_FAILED, "mmap failed: %s\n", strerror(errno));

  // Initialize shared memory
  memset(shm_ptr, 0, total_size);
  shm_ptr->num_antennas_tx = num_tx_ant;
  shm_ptr->num_antennas_rx = num_rx_ant;
  ShmTDIQChannel *channel = calloc_or_fail(1, sizeof(ShmTDIQChannel));
  strncpy(channel->name, name, sizeof(channel->name) - 1);
  channel->tx_iq_data = (sample_t *)(shm_ptr + 1);
  channel->rx_iq_data = channel->tx_iq_data + tx_buffer_size / sizeof(sample_t);
  channel->data = shm_ptr;
  channel->type = IQ_CHANNEL_TYPE_SERVER;
  pthread_mutexattr_t mutex_attr;
  pthread_condattr_t cond_attr;
  int ret = pthread_mutexattr_init(&mutex_attr);
  AssertFatal(ret == 0, "pthread_mutexattr_init() failed: errno %d, %s\n", errno, strerror(errno));

  ret = pthread_condattr_init(&cond_attr);
  AssertFatal(ret == 0, "pthread_condattr_init() failed: errno %d, %s\n", errno, strerror(errno));

  ret = pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
  AssertFatal(ret == 0, "pthread_mutexattr_setpshared() failed: errno %d, %s\n", errno, strerror(errno));

  ret = pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
  AssertFatal(ret == 0, "pthread_condattr_setpshared() failed: errno %d, %s\n", errno, strerror(errno));

  ret = pthread_mutex_init(&shm_ptr->mutex, &mutex_attr);
  AssertFatal(ret == 0, "pthread_mutex_init() failed: errno %d, %s\n", errno, strerror(errno));

  ret = pthread_cond_init(&shm_ptr->cond, &cond_attr);
  AssertFatal(ret == 0, "pthread_cond_init() failed: errno %d, %s\n", errno, strerror(errno));

  shm_ptr->magic = SHM_MAGIC_NUMBER;
  close(fd);
  return channel;
}

ShmTDIQChannel *shm_td_iq_channel_connect(const char *name, int timeout_in_seconds)
{
  // Create shared memory segment
  int fd = -1;
  while (timeout_in_seconds > 0 && fd == -1) {
    fd = shm_open(name, O_RDWR, S_IRUSR | S_IWUSR);
    timeout_in_seconds--;
    printf("Waiting for server to create shared memory segment\n");
    sleep(1);
  }
  AssertFatal(fd != -1, "shm_open() failed: errno %d, %s", errno, strerror(errno));

  struct stat buf;
  fstat(fd, &buf);
  size_t total_size = buf.st_size;

  // Map shared memory segment to address space
  ShmTDIQChannelData *shm_ptr = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (shm_ptr == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }

  ShmTDIQChannel *channel = calloc_or_fail(1, sizeof(ShmTDIQChannel));
  channel->data = shm_ptr;
  channel->tx_iq_data = (sample_t *)(shm_ptr + 1);
  size_t tx_buffer_size = CIRCULAR_BUFFER_SIZE * sizeof(sample_t) * channel->data->num_antennas_tx;
  channel->rx_iq_data = channel->tx_iq_data + tx_buffer_size / sizeof(sample_t);
  channel->type = IQ_CHANNEL_TYPE_CLIENT;
  while (shm_ptr->magic != SHM_MAGIC_NUMBER) {
    printf("Waiting for server to initialize shared memory\n");
    sleep(1);
  }
  close(fd);
  return channel;
}

IQChannelErrorType shm_td_iq_channel_tx(ShmTDIQChannel *channel,
                                        uint64_t timestamp,
                                        uint64_t num_samples,
                                        int antenna,
                                        const sample_t *tx_iq_data)
{
  ShmTDIQChannelData *data = channel->data;
  // timestamp in the past
  uint64_t current_time = data->timestamp;
  if (timestamp < current_time) {
    return CHANNEL_ERROR_TOO_LATE;
  }
  // timestamp is too far in the future
  if (timestamp - current_time + num_samples >= CIRCULAR_BUFFER_SIZE) {
    return CHANNEL_ERROR_TOO_EARLY;
  }

  sample_t *base_ptr;
  if (channel->type == IQ_CHANNEL_TYPE_CLIENT) {
    base_ptr = channel->rx_iq_data + antenna * CIRCULAR_BUFFER_SIZE;
  } else {
    base_ptr = channel->tx_iq_data + antenna * CIRCULAR_BUFFER_SIZE;
  }

  uint64_t first_sample = timestamp % CIRCULAR_BUFFER_SIZE;
  uint64_t last_sample = first_sample + num_samples - 1;
  if (last_sample >= CIRCULAR_BUFFER_SIZE) {
    size_t num_samples_first_copy = CIRCULAR_BUFFER_SIZE - first_sample;
    memcpy(base_ptr + first_sample, tx_iq_data, num_samples_first_copy * sizeof(sample_t));
    memcpy(base_ptr, tx_iq_data + num_samples_first_copy, (num_samples - num_samples_first_copy) * sizeof(sample_t));
  } else {
    memcpy(base_ptr + first_sample, tx_iq_data, num_samples * sizeof(sample_t));
  }
  return CHANNEL_NO_ERROR;
}

IQChannelErrorType shm_td_iq_channel_rx(ShmTDIQChannel *channel,
                                        uint64_t timestamp,
                                        uint64_t num_samples,
                                        int antenna,
                                        sample_t *tx_iq_data)
{
  ShmTDIQChannelData *data = channel->data;
  // timestamp in the future
  uint64_t current_time = data->timestamp;
  if (timestamp > current_time) {
    return CHANNEL_ERROR_TOO_EARLY;
  }
  // timestamp is too far in the past
  if (current_time - timestamp >= CIRCULAR_BUFFER_SIZE) {
    return CHANNEL_ERROR_TOO_LATE;
  }

  sample_t *base_ptr;
  if (channel->type == IQ_CHANNEL_TYPE_CLIENT) {
    base_ptr = channel->tx_iq_data + antenna * CIRCULAR_BUFFER_SIZE;
  } else {
    base_ptr = channel->rx_iq_data + antenna * CIRCULAR_BUFFER_SIZE;
  }

  uint64_t first_sample = timestamp % CIRCULAR_BUFFER_SIZE;
  uint64_t last_sample = first_sample + num_samples - 1;
  if (last_sample >= CIRCULAR_BUFFER_SIZE) {
    size_t num_samples_first_copy = CIRCULAR_BUFFER_SIZE - first_sample;
    memcpy(tx_iq_data, base_ptr + first_sample, num_samples_first_copy * sizeof(sample_t));
    memcpy(tx_iq_data + num_samples_first_copy, base_ptr, (num_samples - num_samples_first_copy) * sizeof(sample_t));
  } else {
    memcpy(tx_iq_data, base_ptr + first_sample, num_samples * sizeof(sample_t));
  }
  return CHANNEL_NO_ERROR;
}

void shm_td_iq_channel_produce_samples(ShmTDIQChannel *channel, size_t num_samples)
{
  ShmTDIQChannelData *data = channel->data;
  mutexlock(data->mutex);
  data->timestamp += num_samples;
  condbroadcast(data->cond);
  mutexunlock(data->mutex);
}

int shm_td_iq_channel_wait(ShmTDIQChannel *channel, uint64_t timestamp, uint64_t timeout_uS)
{
  ShmTDIQChannelData *data = channel->data;
  size_t current_timestamp = data->timestamp;
  if (current_timestamp >= timestamp) {
    return 0;
  }
  if (timeout_uS == 0) {
    mutexlock(data->mutex);
    while (current_timestamp < timestamp && !channel->abort) {
      condwait(data->cond, data->mutex);
      current_timestamp = data->timestamp;
    }
    mutexunlock(data->mutex);
  } else {
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 0};
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
      fprintf(stderr, "Error: clock_gettime failed: %s\n", strerror(errno));
      return 1;
    }
    
    ts.tv_sec += timeout_uS / 1000000; // Convert microseconds to seconds
    ts.tv_nsec += (timeout_uS % 1000000) * 1000; // Convert remaining microseconds to nanoseconds

    mutexlock(data->mutex);
    while (current_timestamp < timestamp && !channel->abort) {
      int ret = pthread_cond_timedwait(&data->cond, &data->mutex, &ts);
      if (ret == ETIMEDOUT) {
        fprintf(stderr, "Error: Timed out waiting for samples.\n");
        mutexunlock(data->mutex);
        return 1;
      } else if (ret != 0) {
        fprintf(stderr, "Error: pthread_cond_timedwait failed: %s\n", strerror(ret));
        mutexunlock(data->mutex);
        return 1;
      } else {
        current_timestamp = data->timestamp;
      }
    }
    mutexunlock(data->mutex);
  }
  return 0;
}

uint64_t shm_td_iq_channel_get_current_sample(const ShmTDIQChannel *channel)
{
  ShmTDIQChannelData *data = channel->data;
  return data->timestamp;
}

void shm_td_iq_channel_abort(ShmTDIQChannel *channel)
{
  ShmTDIQChannelData *data = channel->data;
  mutexlock(data->mutex);
  channel->abort = true;
  condbroadcast(data->cond);
  mutexunlock(data->mutex);
}

bool shm_td_iq_channel_is_aborted(const ShmTDIQChannel *channel)
{
  return channel->abort;
}

void shm_td_iq_channel_destroy(ShmTDIQChannel *channel)
{
  ShmTDIQChannelData *data = channel->data;
  size_t tx_buffer_size = CIRCULAR_BUFFER_SIZE * sizeof(sample_t) * data->num_antennas_tx;
  size_t rx_buffer_size = CIRCULAR_BUFFER_SIZE * sizeof(sample_t) * data->num_antennas_rx;
  size_t total_size = sizeof(ShmTDIQChannelData) + tx_buffer_size + rx_buffer_size;
  if (channel->type == IQ_CHANNEL_TYPE_SERVER) {
    munmap(data, total_size);
    shm_unlink(channel->name);
  } else {
    munmap(data, total_size);
  }
  free(channel);
}
