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

#include "PHY/TOOLS/tools_defs.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <linux/limits.h>

#include <common/utils/assertions.h>
#include <common/utils/LOG/log.h>
#include <common/utils/load_module_shlib.h>
#include <common/utils/telnetsrv/telnetsrv.h>
#include <common/config/config_userapi.h>
#include "common_lib.h"
#include "shm_td_iq_channel.h"
#include "SIMULATION/TOOLS/sim.h"
#include "actor.h"
#include "noise_device.h"
#include "simde/x86/avx512.h"
#include "taps_client.h"

// Simulator role
typedef enum { ROLE_SERVER = 1, ROLE_CLIENT } role;

#define MAX_NUM_ANTENNAS_TX 4
#define MAX_CHANNEL_LENGTH (1 << 20)

#define ROLE_CLIENT_STRING "client"
#define ROLE_SERVER_STRING "server"

#define VRTSIM_SECTION "vrtsim"
#define TIME_SCALE_HLP \
  "sample time scale. 1.0 means realtime. Values > 1 mean faster than realtime. Values < 1 mean slower than realtime\n"
#define TAPS_SOCKET_HLP "Socket to connect to the channel emulation server\n"
#define CLIENT_NUM_RX_HLP "Number of RX antennas of the client, specified on the server\n"
#define CONNECTION_DESCRIPTOR_HLP "Path to the file written by the server that the client can use to connect."
#define DEFAULT_CHANNEL_NAME "vrtsim_channel"
#define DEFAULT_DESCRIPTOR "/tmp/vrtsim_connection"

// clang-format off
#define VRTSIM_PARAMS_DESC \
  { \
     {"connection_descriptor",  CONNECTION_DESCRIPTOR_HLP,   0, .strptr = &vrtsim_state->connection_descriptor,  .defstrval = DEFAULT_DESCRIPTOR, TYPE_STRING, 0}, \
     {"role",                   "either client or server\n", 0, .strptr = &role,                                 .defstrval = ROLE_CLIENT_STRING, TYPE_STRING, 0}, \
     {"timescale",              TIME_SCALE_HLP,              0, .dblptr = &vrtsim_state->timescale,              .defdblval = 1.0,                TYPE_DOUBLE, 0}, \
     {"chanmod",                "Enable channel modelling",  0, .iptr = &vrtsim_state->chanmod,                  .defintval = 0,                  TYPE_INT,    0}, \
     {"taps-socket",            TAPS_SOCKET_HLP,             0, .strptr = &vrtsim_state->taps_socket,            .defstrval = NULL,               TYPE_STRING, 0}, \
     {"client-num-rx-antennas", CLIENT_NUM_RX_HLP,           0, .iptr = &vrtsim_state->client_num_rx_antennas,   .defintval = 1,                  TYPE_INT,    0}, \
  };
// clang-format on

typedef struct histogram_s {
  uint64_t diff[30];
  int num_samples;
  int min_samples;
  double range;
} histogram_t;

// Information about the peer
typedef struct peer_info_s {
  int num_rx_antennas;
} peer_info_t;

typedef struct tx_timing_s {
  uint64_t tx_samples_late;
  uint64_t tx_early;
  uint64_t tx_samples_total;
  double average_tx_budget;
  histogram_t tx_histogram;
} tx_timing_t;

typedef struct {
  int role;
  char *connection_descriptor;
  ShmTDIQChannel *channel;
  uint64_t last_received_sample;
  pthread_t timing_thread;
  bool run_timing_thread;
  double timescale;
  double sample_rate;
  uint64_t rx_samples_late;
  uint64_t rx_early;
  uint64_t rx_samples_total;
  tx_timing_t *tx_timing;
  peer_info_t peer_info;
  int chanmod;
  double rx_freq;
  double tx_bw;
  int tx_num_channels;
  int rx_num_channels;
  channel_desc_t *channel_desc;
  Actor_t *channel_modelling_actors;
  char *taps_socket;
  int client_num_rx_antennas;
} vrtsim_state_t;

// Sample history for channel impulse response
static c16_t saved_samples[MAX_NUM_ANTENNAS_TX][MAX_CHANNEL_LENGTH] __attribute__((aligned(32))) = {0};

static void histogram_add(histogram_t *histogram, double diff)
{
  histogram->num_samples++;
  if (histogram->num_samples >= histogram->min_samples) {
    int bin = min(sizeofArray(histogram->diff) - 1, max(0, (int)(diff / histogram->range * sizeofArray(histogram->diff))));
    histogram->diff[bin]++;
  }
}

static void histogram_print(histogram_t *histogram)
{
  LOG_I(HW, "VRTSIM: TX budget histogram: %d samples\n", histogram->num_samples);
  float bin_size = histogram->range / sizeofArray(histogram->diff);
  float bin_start = 0;
  for (int i = 0; i < sizeofArray(histogram->diff); i++) {
    LOG_I(HW, "Bin %d\t[%.1f - %.1fuS]:\t\t%lu\n", i, bin_start, bin_start + bin_size, histogram->diff[i]);
    bin_start += bin_size;
  }
}

static void histogram_merge(histogram_t *dest, histogram_t *src)
{
  for (int i = 0; i < sizeofArray(dest->diff); i++) {
    dest->diff[i] += src->diff[i];
  }
  dest->num_samples += src->num_samples;
}

static void load_channel_model(vrtsim_state_t *vrtsim_state)
{
  load_channellist(vrtsim_state->tx_num_channels,
                   vrtsim_state->peer_info.num_rx_antennas,
                   vrtsim_state->sample_rate,
                   vrtsim_state->rx_freq,
                   vrtsim_state->tx_bw);
  char *model_name = vrtsim_state->role == ROLE_CLIENT ? "client_tx_channel_model" : "server_tx_channel_model";
  vrtsim_state->channel_desc = find_channel_desc_fromname(model_name);
  AssertFatal(vrtsim_state->channel_desc != NULL,
              "Could not find model name %s. Make sure it is present in the config file",
              model_name);
  LOG_I(HW,
        "Channel model %s parameters: path_loss_dB=%.2f, nb_tx=%d, nb_rx=%d, channel_length=%d\n",
        model_name,
        vrtsim_state->channel_desc->path_loss_dB,
        vrtsim_state->channel_desc->nb_tx,
        vrtsim_state->channel_desc->nb_rx,
        vrtsim_state->channel_desc->channel_length);
  random_channel(vrtsim_state->channel_desc, 0);
  AssertFatal(vrtsim_state->channel_desc != NULL, "Could not find channel model %s\n", model_name);
}

static void vrtsim_readconfig(vrtsim_state_t *vrtsim_state)
{
  char *role = NULL;
  paramdef_t vrtsim_params[] = VRTSIM_PARAMS_DESC;
  int ret = config_get(config_get_if(), vrtsim_params, sizeofArray(vrtsim_params), VRTSIM_SECTION);
  AssertFatal(ret >= 0, "configuration couldn't be performed\n");
  if (strncmp(role, ROLE_CLIENT_STRING, strlen(ROLE_CLIENT_STRING)) == 0) {
    vrtsim_state->role = ROLE_CLIENT;
  } else if (strncmp(role, ROLE_SERVER_STRING, strlen(ROLE_SERVER_STRING)) == 0) {
    vrtsim_state->role = ROLE_SERVER;
  } else {
    AssertFatal(false, "Invalid role configuration\n");
  }
#ifdef ENABLE_TAPS_CLIENT
  if (vrtsim_state->taps_socket) {
    LOG_A(HW, "VRTSIM: will use taps socket %s\n", vrtsim_state->taps_socket);
  }
#else
  if (vrtsim_state->taps_socket) {
    AssertFatal(false, "Invalid configuration: Build with ENABLE_TAPS_CLIENT to use taps socket\n");
  }
#endif
}

static void *vrtsim_timing_job(void *arg)
{
  vrtsim_state_t *vrtsim_state = arg;
  struct timespec timestamp;
  if (clock_gettime(CLOCK_REALTIME, &timestamp)) {
    LOG_E(UTIL, "clock_gettime failed\n");
    exit(1);
  }
  double leftover_samples = 0;
  while (vrtsim_state->run_timing_thread) {
    struct timespec current_time;
    if (clock_gettime(CLOCK_REALTIME, &current_time)) {
      LOG_E(UTIL, "clock_gettime failed\n");
      exit(1);
    }
    uint64_t diff = (current_time.tv_sec - timestamp.tv_sec) * 1000000000 + (current_time.tv_nsec - timestamp.tv_nsec);
    timestamp = current_time;
    double samples_to_produce = vrtsim_state->sample_rate * vrtsim_state->timescale * diff / 1e9;

    // Attempt to correct compounding rounding error
    leftover_samples += samples_to_produce - (uint64_t)samples_to_produce;
    if (leftover_samples > 1.0f) {
      samples_to_produce += 1;
      leftover_samples -= 1;
    }
    AssertFatal(samples_to_produce >= 0, "Negative samples to produce: %f\n", samples_to_produce);
    shm_td_iq_channel_produce_samples(vrtsim_state->channel, samples_to_produce);
    usleep(1);
  }
  return 0;
}

typedef struct client_info_s {
  int server_num_rx_antennas;
  int client_num_rx_antennas;
} client_info_t;

/**
 * @brief Publishes the client information information to a file for the client to read.
 *
 * The server writes its client_info (number of RX antennas) to a file, which the client reads.
 * The server does not wait for the client to write back; the client can connect at any point.
 *
 * @param client_info The client information to publish.
 * @return The peer information (same as input, server is authoritative).
 */
static void server_publish_client_info(client_info_t client_info, char *descriptor_file)
{
  FILE *fp = fopen(descriptor_file, "wb");
  AssertFatal(fp != NULL, "Failed to open client info file for writing: %s\n", strerror(errno));
  size_t written = fwrite(&client_info, sizeof(client_info), 1, fp);
  AssertFatal(written == 1, "Failed to write client info to file\n");
  fclose(fp);
}

static client_info_t client_read_info(char *descriptor_file)
{
  client_info_t client_info;
  int tries = 0;
  while (tries < 10) {
    FILE *fp = fopen(descriptor_file, "rb");
    if (fp) {
      size_t read = fread(&client_info, sizeof(client_info), 1, fp);
      fclose(fp);
      if (read == 1) {
        return client_info;
      }
    }
    sleep(1);
    tries++;
  }
  AssertFatal(0, "Timeout waiting for client info\n");
  return client_info;
}

static int vrtsim_connect(openair0_device *device)
{
  vrtsim_state_t *vrtsim_state = (vrtsim_state_t *)device->priv;

  // Setup a shared memory channel
  if (vrtsim_state->role == ROLE_SERVER) {
    vrtsim_state->peer_info.num_rx_antennas = vrtsim_state->client_num_rx_antennas;
    vrtsim_state->channel = shm_td_iq_channel_create(DEFAULT_CHANNEL_NAME,
                                                     vrtsim_state->peer_info.num_rx_antennas,
                                                     device->openair0_cfg[0].rx_num_channels);
    // Exchange peer info
    client_info_t client_info = {
        .server_num_rx_antennas = device->openair0_cfg[0].rx_num_channels,
        .client_num_rx_antennas = vrtsim_state->client_num_rx_antennas,
    };
    server_publish_client_info(client_info, vrtsim_state->connection_descriptor);

    vrtsim_state->run_timing_thread = true;
    int ret = pthread_create(&vrtsim_state->timing_thread, NULL, vrtsim_timing_job, vrtsim_state);
    AssertFatal(ret == 0, "pthread_create() failed: errno: %d, %s\n", errno, strerror(errno));
  } else {
    client_info_t client_info = client_read_info(vrtsim_state->connection_descriptor);
    AssertFatal(client_info.server_num_rx_antennas > 0, "Server did not publish valid client info, aborting client connection\n");
    AssertFatal(
        client_info.client_num_rx_antennas == device->openair0_cfg[0].rx_num_channels,
        "Server expects different number of RX antennas. %d != %d. Use server command line option --client-num-rx-antennas\n",
        client_info.client_num_rx_antennas,
        device->openair0_cfg[0].rx_num_channels);
    vrtsim_state->channel = shm_td_iq_channel_connect(DEFAULT_CHANNEL_NAME, 10);
    vrtsim_state->peer_info.num_rx_antennas = client_info.server_num_rx_antennas;
    vrtsim_state->last_received_sample = shm_td_iq_channel_get_current_sample(vrtsim_state->channel);
  }

  // Handle channel modelling after number of RX antennas are known
  int num_tx_stats = 1;
  if (vrtsim_state->chanmod || vrtsim_state->taps_socket) {
    vrtsim_state->channel_modelling_actors = calloc_or_fail(vrtsim_state->peer_info.num_rx_antennas, sizeof(Actor_t));
    for (int i = 0; i < vrtsim_state->peer_info.num_rx_antennas; i++) {
      init_actor(&vrtsim_state->channel_modelling_actors[i], "chanmod", -1);
    }
    if (vrtsim_state->taps_socket) {
      taps_client_connect(0,
                          vrtsim_state->taps_socket,
                          device->openair0_cfg[0].tx_num_channels,
                          vrtsim_state->peer_info.num_rx_antennas,
                          &vrtsim_state->channel_desc);
    } else {
      load_channel_model(vrtsim_state);
    }
    num_tx_stats = vrtsim_state->peer_info.num_rx_antennas;
  }
  vrtsim_state->tx_timing = calloc_or_fail(num_tx_stats, sizeof(tx_timing_t));
  for (int i = 0; i < num_tx_stats; i++) {
    vrtsim_state->tx_timing[i].tx_histogram.min_samples = 100;
    // Set the histogram range to 3000uS. Anything above that is not interesting
    vrtsim_state->tx_timing[i].tx_histogram.range = 3000.0;
  }

  return 0;
}

static int vrtsim_write_internal(vrtsim_state_t *vrtsim_state,
                                 openair0_timestamp timestamp,
                                 c16_t *samples,
                                 int nsamps,
                                 int aarx,
                                 int flags,
                                 int stats_index)
{
  tx_timing_t *tx_timing = &vrtsim_state->tx_timing[stats_index];

  uint64_t sample = shm_td_iq_channel_get_current_sample(vrtsim_state->channel);
  int64_t diff = timestamp - sample;
  double budget = diff / (vrtsim_state->sample_rate / 1e6);
  tx_timing->average_tx_budget = .05 * budget + .95 * tx_timing->average_tx_budget;
  histogram_add(&tx_timing->tx_histogram, budget);

  int ret = shm_td_iq_channel_tx(vrtsim_state->channel, timestamp, nsamps, aarx, (sample_t *)samples);

  if (ret == CHANNEL_ERROR_TOO_LATE) {
    tx_timing->tx_samples_late += nsamps;
  } else if (ret == CHANNEL_ERROR_TOO_EARLY) {
    tx_timing->tx_early += 1;
  }
  tx_timing->tx_samples_total += nsamps;

  return nsamps;
}

typedef struct {
  vrtsim_state_t *vrtsim_state;
  openair0_timestamp timestamp;
  c16_t *samples[MAX_NUM_ANTENNAS_TX];
  int nsamps;
  int nbAnt;
  int flags;
  int aarx;
} channel_modelling_args_t;

static void perform_channel_modelling(void *arg)
{
  channel_modelling_args_t *channel_modelling_args = arg;
  vrtsim_state_t *vrtsim_state = channel_modelling_args->vrtsim_state;
  int nsamps = channel_modelling_args->nsamps;
  int aarx = channel_modelling_args->aarx;
  int nb_tx_ant = channel_modelling_args->nbAnt;
  c16_t **input_samples = (c16_t **)channel_modelling_args->samples;

  int aligned_nsamps = ceil_mod(nsamps, (512 / 8) / sizeof(cf_t));
  cf_t samples[aligned_nsamps] __attribute__((aligned(64)));
  // Apply noise from global settings
  get_noise_vector((float *)samples, nsamps * 2);

  channel_desc_t *channel_desc = vrtsim_state->channel_desc;

  if (channel_desc == NULL) {
    return;
  }

  cf_t channel_impulse_response[nb_tx_ant][channel_desc->channel_length];
  cf_t *channel_impulse_response_p[nb_tx_ant];
  if (!vrtsim_state->taps_socket) {
    const float pathloss_linear = powf(10, channel_desc->path_loss_dB / 20.0);
    // Convert channel impulse response to float + apply pathloss
    for (int aatx = 0; aatx < nb_tx_ant; aatx++) {
      const struct complexd *channelModel = channel_desc->ch[aarx + (aatx * channel_desc->nb_rx)];
      for (int i = 0; i < channel_desc->channel_length; i++) {
        channel_impulse_response[aatx][i].r = channelModel[i].r * pathloss_linear;
        channel_impulse_response[aatx][i].i = channelModel[i].i * pathloss_linear;
      }
      channel_impulse_response_p[aatx] = channel_impulse_response[aatx];
    }
  } else {
    for (int aatx = 0; aatx < nb_tx_ant; aatx++) {
      struct complexf *channelModel = channel_desc->ch_ps[aarx + (aatx * channel_desc->nb_rx)];
      channel_impulse_response_p[aatx] = channelModel;
    }
  }

  for (int aatx = 0; aatx < nb_tx_ant; aatx++) {
    c16_t *previous_samples = saved_samples[aatx];
    for (int i = 0; i < nsamps; i++) {
      cf_t *impulse_response = channel_impulse_response_p[aatx];
      for (int l = 0; l < channel_desc->channel_length; l++) {
        int idx = i - l;
        // TODO: Use AVX2 for this
        c16_t tx_input = idx >= 0 ? input_samples[aatx][idx]
                                  : previous_samples[(channel_modelling_args->timestamp + i + idx) % MAX_CHANNEL_LENGTH];
        samples[i].r += tx_input.r * impulse_response[l].r - tx_input.i * impulse_response[l].i;
        samples[i].i += tx_input.i * impulse_response[l].r + tx_input.r * impulse_response[l].i;
      }
    }
  }

  // Convert to c16_t
  c16_t samples_out[aligned_nsamps] __attribute__((aligned(64)));
#if defined(__AVX512F__)
  for (int i = 0; i < aligned_nsamps / 8; i++) {
    simde__m512 *in = (simde__m512 *)&samples[i * 8];
    simde__m256i *out = (simde__m256i *)&samples_out[i * 8];
    *out = simde_mm512_cvtsepi32_epi16(simde_mm512_cvtps_epi32(*in));
  }
#elif defined(__AVX2__)
  for (int i = 0; i < aligned_nsamps / 4; i++) {
    simde__m256 *in = (simde__m256 *)&samples[i * 4];
    simde__m128i *out = (simde__m128i *)&samples_out[i * 4];
    *out = simde_mm256_cvtsepi32_epi16(simde_mm256_cvtps_epi32(*in));
  }
#else
  for (int i = 0; i < nsamps; i++) {
    samples_out[i].r = lroundf(samples[i].r);
    samples_out[i].i = lroundf(samples[i].i);
  }
#endif

  vrtsim_write_internal(channel_modelling_args->vrtsim_state,
                        channel_modelling_args->timestamp,
                        samples_out,
                        channel_modelling_args->nsamps,
                        aarx,
                        channel_modelling_args->flags,
                        aarx);
}

static int vrtsim_write_with_chanmod(vrtsim_state_t *vrtsim_state,
                                     openair0_timestamp timestamp,
                                     void **samplesVoid,
                                     int nsamps,
                                     int nbAnt,
                                     int flags)
{
  AssertFatal(nbAnt < MAX_NUM_ANTENNAS_TX, "Number of antennas %d exceeds maximum %d\n", nbAnt, MAX_NUM_ANTENNAS_TX);
  for (int aarx = 0; aarx < vrtsim_state->peer_info.num_rx_antennas; aarx++) {
    notifiedFIFO_elt_t *task = newNotifiedFIFO_elt(sizeof(channel_modelling_args_t), 0, NULL, perform_channel_modelling);
    channel_modelling_args_t *args = (channel_modelling_args_t *)NotifiedFifoData(task);
    args->vrtsim_state = vrtsim_state;
    args->timestamp = timestamp;
    args->nsamps = nsamps;
    args->nbAnt = nbAnt;
    args->flags = flags;
    args->aarx = aarx;
    for (int i = 0; i < nbAnt; i++) {
      args->samples[i] = samplesVoid[i];
    }
    pushNotifiedFIFO(&vrtsim_state->channel_modelling_actors[aarx].fifo, task);
  }
  int start_index = timestamp % MAX_CHANNEL_LENGTH;
  int end_index = min(start_index + nsamps, MAX_CHANNEL_LENGTH);
  int cp_nsamps = end_index - start_index;
  for (int aatx = 0; aatx < nbAnt; aatx++) {
    c16_t *samples = (c16_t *)samplesVoid[aatx];
    memcpy(&saved_samples[aatx][start_index], &samples[0], sizeof(c16_t) * cp_nsamps);
  }

  if (end_index < start_index + nsamps) {
    // wrap around condition, write at beginning of buffer
    cp_nsamps = nsamps - cp_nsamps; // remaining samples
    start_index = 0;
    for (int aatx = 0; aatx < nbAnt; aatx++) {
      c16_t *samples = (c16_t *)samplesVoid[aatx];
      memcpy(&saved_samples[aatx][start_index], &samples[0], sizeof(c16_t) * cp_nsamps);
    }
  }
  return nsamps;
}

static int vrtsim_write(openair0_device *device, openair0_timestamp timestamp, void **samplesVoid, int nsamps, int nbAnt, int flags)
{
  AssertFatal(nsamps > 0, "Number of samples must be greater than 0\n");
  AssertFatal(nbAnt > 0 && nbAnt <= MAX_NUM_ANTENNAS_TX,
              "Number of antennas %d must be between 1 and %d\n",
              nbAnt,
              MAX_NUM_ANTENNAS_TX);
  AssertFatal(timestamp >= 0, "Timestamp must be non-negative, got %ld\n", timestamp);
  timestamp -= device->openair0_cfg->command_line_sample_advance;
  vrtsim_state_t *vrtsim_state = (vrtsim_state_t *)device->priv;
  bool channel_modelling = vrtsim_state->chanmod || vrtsim_state->taps_socket;
  return channel_modelling ? vrtsim_write_with_chanmod(vrtsim_state, timestamp, samplesVoid, nsamps, nbAnt, flags)
                           : vrtsim_write_internal(vrtsim_state, timestamp, (c16_t *)samplesVoid[0], nsamps, 0, flags, 0);
}

static int vrtsim_read(openair0_device *device, openair0_timestamp *ptimestamp, void **samplesVoid, int nsamps, int nbAnt)
{
  vrtsim_state_t *vrtsim_state = (vrtsim_state_t *)device->priv;
  if (shm_td_iq_channel_is_aborted(vrtsim_state->channel)) {
    return 0;
  }
  if (vrtsim_state->role == ROLE_SERVER) {
    uint64_t timeout_uS = 0; // 0 means no timeout
    shm_td_iq_channel_wait(vrtsim_state->channel, vrtsim_state->last_received_sample + nsamps, timeout_uS);
  } else {
    uint64_t start_sample = shm_td_iq_channel_get_current_sample(vrtsim_state->channel);
    uint64_t timeout_uS = 2 * 1000 * 1000; // 2 seconds timeout waiting for sample number to change
    //
    while (shm_td_iq_channel_wait(vrtsim_state->channel, vrtsim_state->last_received_sample + nsamps, timeout_uS) == 1) {
      uint64_t sample = shm_td_iq_channel_get_current_sample(vrtsim_state->channel);
      if (sample == start_sample) {
        LOG_E(HW,
              "VRTSIM: Read timeout waiting for sample %lu to change, aborting channel\n",
              vrtsim_state->last_received_sample + nsamps);
        shm_td_iq_channel_abort(vrtsim_state->channel);
        break;
      } else {
        start_sample = sample;
      }
    }
  }

  int ret = shm_td_iq_channel_rx(vrtsim_state->channel, vrtsim_state->last_received_sample, nsamps, 0, samplesVoid[0]);
  if (ret == CHANNEL_ERROR_TOO_LATE) {
    vrtsim_state->rx_samples_late += nsamps;
  } else if (ret == CHANNEL_ERROR_TOO_EARLY) {
    vrtsim_state->rx_early += 1;
  }
  vrtsim_state->rx_samples_total += nsamps;
  *ptimestamp = vrtsim_state->last_received_sample;
  vrtsim_state->last_received_sample += nsamps;
  return nsamps;
}

static void vrtsim_end(openair0_device *device)
{
  vrtsim_state_t *vrtsim_state = (vrtsim_state_t *)device->priv;
  if (vrtsim_state->role == ROLE_SERVER && vrtsim_state->run_timing_thread) {
    vrtsim_state->run_timing_thread = false;
    int ret = pthread_join(vrtsim_state->timing_thread, NULL);
    AssertFatal(ret == 0, "pthread_join() failed: errno: %d, %s\n", errno, strerror(errno));
  }

  tx_timing_t *tx_timing = vrtsim_state->tx_timing;
  if (vrtsim_state->chanmod || vrtsim_state->taps_socket) {
    for (int i = 0; i < vrtsim_state->peer_info.num_rx_antennas; i++) {
      shutdown_actor(&vrtsim_state->channel_modelling_actors[i]);
    }
    free(vrtsim_state->channel_modelling_actors);
    for (int i = 1; i < vrtsim_state->peer_info.num_rx_antennas; i++) {
      histogram_merge(&tx_timing->tx_histogram, &tx_timing[i].tx_histogram);
      tx_timing->tx_early += tx_timing[i].tx_early;
      tx_timing->tx_samples_late += tx_timing[i].tx_samples_late;
      tx_timing->average_tx_budget += tx_timing[i].average_tx_budget;
      tx_timing->tx_samples_total += tx_timing[i].tx_samples_total;
    }
    tx_timing->average_tx_budget /= vrtsim_state->peer_info.num_rx_antennas;
    free_noise_device();
    if (vrtsim_state->taps_socket) {
      taps_client_stop();
    }
  }
  shm_td_iq_channel_abort(vrtsim_state->channel);
  sleep(1);
  shm_td_iq_channel_destroy(vrtsim_state->channel);

  LOG_I(HW,
        "VRTSIM: Realtime issues: TX %.2f%%, RX %.2f%%\n",
        tx_timing->tx_samples_late / (float)tx_timing->tx_samples_total * 100,
        vrtsim_state->rx_samples_late / (float)vrtsim_state->rx_samples_total * 100);
  LOG_I(HW,
        "VRTSIM: Read/write too early (suspected radio implementaton error) TX: %lu, RX: %lu\n",
        tx_timing->tx_early,
        vrtsim_state->rx_early);
  LOG_I(HW, "VRTSIM: Average TX budget %.3lf uS (more is better)\n", tx_timing->average_tx_budget);
  histogram_print(&tx_timing->tx_histogram);
  free(vrtsim_state->tx_timing);

  if (vrtsim_state->role == ROLE_SERVER) {
    int ret = remove(vrtsim_state->connection_descriptor);
    if (ret != 0) {
      LOG_E(HW, "Failed to remove connection descriptor file %s: %s\n", vrtsim_state->connection_descriptor, strerror(errno));
    } else {
      LOG_A(HW, "Removed connection descriptor file %s\n", vrtsim_state->connection_descriptor);
    }
  }
}

static int vrtsim_stub(openair0_device *device)
{
  return 0;
}
static int vrtsim_stub2(openair0_device *device, openair0_config_t *openair0_cfg)
{
  return 0;
}

static int vrtsim_set_freq(openair0_device *device, openair0_config_t *openair0_cfg)
{
  vrtsim_state_t *s = device->priv;
  s->rx_freq = openair0_cfg->rx_freq[0];
  return 0;
}

__attribute__((__visibility__("default"))) int device_init(openair0_device *device, openair0_config_t *openair0_cfg)
{
  vrtsim_state_t *vrtsim_state = calloc_or_fail(1, sizeof(vrtsim_state_t));
  vrtsim_readconfig(vrtsim_state);
  LOG_I(HW,
        "Running as %s\n",
        vrtsim_state->role == ROLE_SERVER ? "server: waiting for client to connect" : "client: will connect to a vrtsim server");
  device->trx_start_func = vrtsim_connect;
  device->trx_reset_stats_func = vrtsim_stub;
  device->trx_end_func = vrtsim_end;
  device->trx_stop_func = vrtsim_stub;
  device->trx_set_freq_func = vrtsim_set_freq;
  device->trx_set_gains_func = vrtsim_stub2;
  device->trx_write_func = vrtsim_write;
  device->trx_read_func = vrtsim_read;

  device->type = RFSIMULATOR;
  device->openair0_cfg = &openair0_cfg[0];
  device->priv = vrtsim_state;
  device->trx_write_init = vrtsim_stub;
  vrtsim_state->last_received_sample = 0;
  vrtsim_state->sample_rate = openair0_cfg->sample_rate;
  vrtsim_state->rx_freq = openair0_cfg->rx_freq[0];
  vrtsim_state->tx_bw = openair0_cfg->tx_bw;
  vrtsim_state->tx_num_channels = openair0_cfg->tx_num_channels;
  vrtsim_state->rx_num_channels = openair0_cfg->rx_num_channels;

  if (vrtsim_state->chanmod || vrtsim_state->taps_socket) {
    init_channelmod();
    int noise_power_dBFS = get_noise_power_dBFS();
    int16_t noise_power = noise_power_dBFS == INVALID_DBFS_VALUE ? 0 : (int16_t)(32767.0 / powf(10.0, .05 * -noise_power_dBFS));
    LOG_A(HW, "VRTSIM: Noise power %d sample value\n", noise_power);
    init_noise_device(noise_power);
  }
  return 0;
}
