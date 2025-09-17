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
 * Author and copyright: Laurent Thomas, open-cells.com
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

/*
 * Open issues and limitations
 * The read and write should be called in the same thread, that is not new USRP UHD design
 * When the opposite side switch from passive reading to active R+Write, the synchro is not fully deterministic
 */

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

#include <common/utils/assertions.h>
#include <common/utils/LOG/log.h>
#include <common/utils/load_module_shlib.h>
#include <common/utils/telnetsrv/telnetsrv.h>
#include <common/config/config_userapi.h>
#include "common_lib.h"
#define CHANNELMOD_DYNAMICLOAD
#include <openair1/SIMULATION/TOOLS/sim.h>
#include "rfsimulator.h"

#define PORT 4043 // default TCP port for this simulator
//
// CirSize defines the number of samples inquired for a read cycle
// It is bounded by a slot read capability (which depends on bandwidth and numerology)
// up to multiple slots read to allow I/Q buffering of the I/Q TCP stream
//
// As a rule of thumb:
// -it can't be less than the number of samples for a slot
// -it can range up to multiple slots
//
// The default value is chosen for 10ms buffering which makes 23040*20 = 460800 samples
// The previous value is kept below in comment it was computed for 100ms 1x 20MHz
// #define CirSize 6144000 // 100ms SiSo 20MHz LTE
#define minCirSize 460800 // 10ms  SiSo 40Mhz 3/4 sampling NR78 FR1
#define sampleToByte(a, b) ((a) * (b) * sizeof(sample_t))
#define byteToSample(a, b) ((a) / (sizeof(sample_t) * (b)))

#define GENERATE_CHANNEL 10 // each frame (or slot?) in DL
#define MAX_FD_RFSIMU 250
#define SEND_BUFF_SIZE 100000000 // Socket buffer size

// Simulator role
typedef enum { SIMU_ROLE_SERVER = 1, SIMU_ROLE_CLIENT } simuRole;

//

#define RFSIMU_SECTION "rfsimulator"
#define RFSIMU_OPTIONS_PARAMNAME "options"

#define RFSIM_CONFIG_HELP_OPTIONS                                                                  \
  " list of comma separated options to enable rf simulator functionalities. Available options: \n" \
  "        chanmod:   enable channel modelisation\n"                                               \
  "        saviq:     enable saving written iqs to a file\n"

// clang-format off
/*-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*                                            configuration parameters for the rfsimulator device */
/*   optname                     helpstr                     paramflags           XXXptr                               defXXXval                          type         numelt  */
/*-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
#define simOpt PARAMFLAG_NOFREE | PARAMFLAG_CMDLINE_NOPREFIXENABLED
#define RFSIMULATOR_PARAMS_DESC {					\
    {"serveraddr",             "<ip address to connect to>\n",        simOpt,  .strptr=&rfsimulator->ip,               .defstrval="127.0.0.1",           TYPE_STRING,    0 },\
      {"serverport", "<port to connect to>\n", simOpt, .u16ptr = &(rfsimulator->port), .defuintval = PORT, TYPE_UINT16, 0},       \
      {RFSIMU_OPTIONS_PARAMNAME, RFSIM_CONFIG_HELP_OPTIONS, 0, .strlistptr = NULL, .defstrlistval = NULL, TYPE_STRINGLIST, 0},    \
    {"IQfile",                 "<file path to use when saving IQs>\n",simOpt,  .strptr=&saveF,                         .defstrval="/tmp/rfsimulator.iqs",TYPE_STRING,    0 },\
      {"modelname", "<channel model name>\n", simOpt, .strptr = &modelname, .defstrval = "AWGN", TYPE_STRING, 0},                 \
      {"ploss", "<channel path loss in dB>\n", simOpt, .dblptr = &(rfsimulator->chan_pathloss), .defdblval = 0, TYPE_DOUBLE, 0},  \
    {"forgetfact",             "<channel forget factor ((0 to 1)>\n", simOpt,  .dblptr=&(rfsimulator->chan_forgetfact),.defdblval=0,                     TYPE_DOUBLE,    0 },\
      {"offset", "<channel offset in samps>\n", simOpt, .u64ptr = &(rfsimulator->chan_offset), .defint64val = 0, TYPE_UINT64, 0}, \
    {"prop_delay",             "<propagation delay in ms>\n",         simOpt,  .dblptr=&(rfsimulator->prop_delay_ms),  .defdblval=0.0,                   TYPE_DOUBLE,    0 },\
    {"wait_timeout",           "<wait timeout if no UE connected>\n", simOpt,  .iptr=&(rfsimulator->wait_timeout),     .defintval=1,                     TYPE_INT,       0 },\
  };
// clang-format on
static void getset_currentchannels_type(char *buf, int debug, webdatadef_t *tdata, telnet_printfunc_t prnt);
extern int get_currentchannels_type(char *buf, int debug, webdatadef_t *tdata, telnet_printfunc_t prnt); // in random_channel.c
static int rfsimu_setchanmod_cmd(char *buff, int debug, telnet_printfunc_t prnt, void *arg);
static int rfsimu_setdistance_cmd(char *buff, int debug, telnet_printfunc_t prnt, void *arg);
static int rfsimu_getdistance_cmd(char *buff, int debug, telnet_printfunc_t prnt, void *arg);
static int rfsimu_vtime_cmd(char *buff, int debug, telnet_printfunc_t prnt, void *arg);
// clang-format off
static telnetshell_cmddef_t rfsimu_cmdarray[] = {
    {"show models", "", (cmdfunc_t)rfsimu_setchanmod_cmd, {(webfunc_t)getset_currentchannels_type}, TELNETSRV_CMDFLAG_WEBSRVONLY | TELNETSRV_CMDFLAG_GETWEBTBLDATA, NULL},
    {"setmodel", "<model name> <model type>", (cmdfunc_t)rfsimu_setchanmod_cmd, {NULL}, TELNETSRV_CMDFLAG_PUSHINTPOOLQ | TELNETSRV_CMDFLAG_TELNETONLY, NULL},
    {"setdistance", "<model name> <distance>", (cmdfunc_t)rfsimu_setdistance_cmd, {NULL}, TELNETSRV_CMDFLAG_PUSHINTPOOLQ | TELNETSRV_CMDFLAG_NEEDPARAM },
    {"getdistance", "<model name>", (cmdfunc_t)rfsimu_getdistance_cmd, {NULL}, TELNETSRV_CMDFLAG_PUSHINTPOOLQ},
    {"vtime", "", (cmdfunc_t)rfsimu_vtime_cmd, {NULL}, TELNETSRV_CMDFLAG_PUSHINTPOOLQ | TELNETSRV_CMDFLAG_AUTOUPDATE},
    {"", "", NULL},
};
// clang-format on
static telnetshell_cmddef_t *setmodel_cmddef = &(rfsimu_cmdarray[1]);

static telnetshell_vardef_t rfsimu_vardef[] = {{"", 0, 0, NULL}};
static uint64_t CirSize = minCirSize;
typedef c16_t sample_t; // 2*16 bits complex number

typedef struct buffer_s {
  int conn_sock;
  openair0_timestamp lastReceivedTS;
  bool headerMode;
  bool trashingPacket;
  samplesBlockHeader_t th;
  char *transferPtr;
  uint64_t remainToTransfer;
  char *circularBufEnd;
  sample_t *circularBuf;
  channel_desc_t *channel_model;
} buffer_t;

typedef struct {
  int listen_sock, epollfd;
  pthread_mutex_t Sockmutex;
  unsigned int nb_cnx;
  openair0_timestamp nextRxTstamp;
  openair0_timestamp lastWroteTS;
  simuRole role;
  char *ip;
  uint16_t port;
  int saveIQfile;
  buffer_t buf[MAX_FD_RFSIMU];
  int next_buf;
  int rx_num_channels;
  int tx_num_channels;
  double sample_rate;
  double rx_freq;
  double tx_bw;
  int channelmod;
  double chan_pathloss;
  double chan_forgetfact;
  uint64_t chan_offset;
  void *telnetcmd_qid;
  poll_telnetcmdq_func_t poll_telnetcmdq;
  int wait_timeout;
  double prop_delay_ms;
} rfsimulator_state_t;

static bool flushInput(rfsimulator_state_t *t, int timeout, bool first_time);

static buffer_t *allocCirBuf(rfsimulator_state_t *bridge, int sock)
{
  uint64_t buff_index = bridge->next_buf++ % MAX_FD_RFSIMU;
  buffer_t *ptr = &bridge->buf[buff_index];
  bridge->nb_cnx++;
  ptr->circularBuf = calloc(1, sampleToByte(CirSize, 1));
  if (ptr->circularBuf == NULL) {
    LOG_E(HW, "malloc(%lu) failed\n", sampleToByte(CirSize, 1));
    return NULL;
  }
  ptr->circularBufEnd = ((char *)ptr->circularBuf) + sampleToByte(CirSize, 1);
  ptr->conn_sock = sock;
  ptr->lastReceivedTS = 0;
  ptr->headerMode = true;
  ptr->trashingPacket = true;
  ptr->transferPtr = (char *)&ptr->th;
  ptr->remainToTransfer = sizeof(samplesBlockHeader_t);
  int sendbuff = SEND_BUFF_SIZE;
  if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sendbuff, sizeof(sendbuff)) != 0) {
    LOG_E(HW, "setsockopt(SO_SNDBUF) failed\n");
    return NULL;
  }
  struct epoll_event ev = {0};
  ev.events = EPOLLIN | EPOLLRDHUP;
  ev.data.ptr = ptr;
  if (epoll_ctl(bridge->epollfd, EPOLL_CTL_ADD, sock, &ev) != 0) {
    LOG_E(HW, "epoll_ctl(EPOLL_CTL_ADD) failed\n");
    return NULL;
  }

  if (bridge->channelmod > 0) {
    // create channel simulation model for this mode reception
    static bool init_done = false;

    if (!init_done) {
      uint64_t rand;
      FILE *h = fopen("/dev/random", "r");

      if (1 != fread(&rand, sizeof(rand), 1, h))
        LOG_W(HW, "Can't read /dev/random\n");

      fclose(h);
      randominit(rand);
      tableNor(rand);
      init_done = true;
    }
    char modelname[30];
    snprintf(modelname,
             sizeofArray(modelname),
             "rfsimu_channel_%s%d",
             (bridge->role == SIMU_ROLE_SERVER) ? "ue" : "enB",
             bridge->nb_cnx - 1);
    ptr->channel_model = find_channel_desc_fromname(modelname); // path_loss in dB
    if (!ptr->channel_model) {
      // Use legacy method to find channel model - this will use the same channel model for all clients
      char *legacy_model_name = (bridge->role == SIMU_ROLE_SERVER) ? "rfsimu_channel_ue0" : "rfsimu_channel_enB0";
      ptr->channel_model = find_channel_desc_fromname(legacy_model_name);
      if (!ptr->channel_model) {
        LOG_E(HW, "Channel model %s/%s not found, check config file\n", modelname, legacy_model_name);
        return NULL;
      }
    }

    set_channeldesc_owner(ptr->channel_model, RFSIMU_MODULEID);
    set_channeldesc_direction(ptr->channel_model, bridge->role == SIMU_ROLE_SERVER);
    random_channel(ptr->channel_model, false);
    LOG_I(HW, "Random channel %s in rfsimulator activated\n", modelname);
  }
  return ptr;
}

static void removeCirBuf(rfsimulator_state_t *bridge, buffer_t *buf)
{
  if (epoll_ctl(bridge->epollfd, EPOLL_CTL_DEL, buf->conn_sock, NULL) != 0) {
    LOG_E(HW, "epoll_ctl(EPOLL_CTL_DEL) failed\n");
  }
  close(buf->conn_sock);
  free(buf->circularBuf);
  // Fixme: no free_channel_desc_scm(bridge->buf[sock].channel_model) implemented
  // a lot of mem leaks
  // free(bridge->buf[sock].channel_model);
  memset(buf, 0, sizeof(buffer_t));
  buf->conn_sock = -1;
  bridge->nb_cnx--;
}

static void socketError(rfsimulator_state_t *bridge, buffer_t *buf)
{
  if (buf->conn_sock != -1) {
    LOG_W(HW, "Lost socket\n");
    removeCirBuf(bridge, buf);

    if (bridge->role == SIMU_ROLE_CLIENT)
      exit(1);
  }
}

enum  blocking_t {
  notBlocking,
  blocking
};

static int setblocking(int sock, enum blocking_t active)
{
  int opts = fcntl(sock, F_GETFL);
  if (opts < 0) {
    LOG_E(HW, "fcntl(F_GETFL) failed, errno(%d)\n", errno);
    return -1;
  }

  if (active == blocking)
    opts = opts & ~O_NONBLOCK;
  else
    opts = opts | O_NONBLOCK;

  opts = fcntl(sock, F_SETFL, opts);
  if (opts < 0) {
    LOG_E(HW, "fcntl(F_SETFL) failed, errno(%d)\n", errno);
    return -1;
  }
  return 0;
}

static void fullwrite(int fd, void *_buf, ssize_t count, rfsimulator_state_t *t) {
  if (t->saveIQfile != -1) {
    if (write(t->saveIQfile, _buf, count) != count)
      LOG_E(HW, "write() in save iq file failed (%d)\n", errno);
  }

  char *buf = _buf;
  ssize_t l;

  while (count) {
    l = write(fd, buf, count);

    if (l == 0) {
      LOG_E(HW, "write() failed, returned 0\n");
      return;
    }

    if (l < 0) {
      if (errno == EINTR)
        continue;

      if (errno == EAGAIN) {
        LOG_D(HW, "write() failed, errno(%d)\n", errno);
        usleep(250);
        continue;
      } else {
        LOG_E(HW, "write() failed, errno(%d)\n", errno);
        return;
      }
    }

    count -= l;
    buf += l;
  }
}

static void rfsimulator_readconfig(rfsimulator_state_t *rfsimulator) {
  char *saveF = NULL;
  char *modelname = NULL;
  paramdef_t rfsimu_params[] = RFSIMULATOR_PARAMS_DESC;
  int p = config_paramidx_fromname(rfsimu_params, sizeofArray(rfsimu_params), RFSIMU_OPTIONS_PARAMNAME);
  int ret = config_get(config_get_if(), rfsimu_params, sizeofArray(rfsimu_params), RFSIMU_SECTION);
  AssertFatal(ret >= 0, "configuration couldn't be performed\n");

  rfsimulator->saveIQfile = -1;

  for (int i = 0; i < rfsimu_params[p].numelt; i++) {
    if (strcmp(rfsimu_params[p].strlistptr[i], "saviq") == 0) {
      rfsimulator->saveIQfile = open(saveF, O_APPEND | O_CREAT | O_TRUNC | O_WRONLY, 0666);

      if (rfsimulator->saveIQfile != -1)
        LOG_I(HW, "Will save written IQ samples in %s\n", saveF);
      else {
        LOG_E(HW, "open(%s) failed for IQ saving, errno(%d)\n", saveF, errno);
        exit(-1);
      }

      break;
    } else if (strcmp(rfsimu_params[p].strlistptr[i], "chanmod") == 0) {
      init_channelmod();
      load_channellist(rfsimulator->tx_num_channels, rfsimulator->rx_num_channels, rfsimulator->sample_rate, rfsimulator->rx_freq, rfsimulator->tx_bw);
      rfsimulator->channelmod = true;
    } else {
      fprintf(stderr, "unknown rfsimulator option: %s\n", rfsimu_params[p].strlistptr[i]);
      exit(-1);
    }
  }

  /* for compatibility keep environment variable usage */
  if (getenv("RFSIMULATOR") != NULL) {
    rfsimulator->ip = getenv("RFSIMULATOR");
    LOG_W(HW, "The RFSIMULATOR environment variable is deprecated and support will be removed in the future. Instead, add parameter --rfsimulator.serveraddr %s to set the server address. Note: the default is \"server\"; for the gNB/eNB, you don't have to set any configuration.\n", rfsimulator->ip);
    LOG_I(HW, "Remove RFSIMULATOR environment variable to get rid of this message and the sleep.\n");
    sleep(10);
  }

  if ( strncasecmp(rfsimulator->ip,"enb",3) == 0 ||
       strncasecmp(rfsimulator->ip,"server",3) == 0 )
    rfsimulator->role = SIMU_ROLE_SERVER;
  else
    rfsimulator->role = SIMU_ROLE_CLIENT;
}

static int rfsimu_setchanmod_cmd(char *buff, int debug, telnet_printfunc_t prnt, void *arg) {
  char *modelname = NULL;
  char *modeltype = NULL;
  rfsimulator_state_t *t = (rfsimulator_state_t *)arg;
  if (t->channelmod == false) {
    prnt("%s: ERROR channel modelisation disabled...\n", __func__);
    return 0;
  }
  if (buff == NULL) {
    prnt("%s: ERROR wrong rfsimu setchannelmod command...\n", __func__);
    return 0;
  }
  if (debug)
    prnt("%s: rfsimu_setchanmod_cmd buffer \"%s\"\n", __func__, buff);
  int s = sscanf(buff, "%m[^ ] %ms\n", &modelname, &modeltype);

  if (s == 2) {
    int channelmod = modelid_fromstrtype(modeltype);

    if (channelmod < 0)
      prnt("%s: ERROR: model type %s unknown\n", __func__, modeltype);
    else {
      rfsimulator_state_t *t = (rfsimulator_state_t *)arg;
      int found = 0;
      for (int i = 0; i < MAX_FD_RFSIMU; i++) {
        buffer_t *b = &t->buf[i];
        if (b->channel_model == NULL)
          continue;
        if (b->channel_model->model_name == NULL)
          continue;
        if (b->conn_sock >= 0 && (strcmp(b->channel_model->model_name, modelname) == 0)) {
          channel_desc_t *newmodel = new_channel_desc_scm(t->tx_num_channels,
                                                          t->rx_num_channels,
                                                          channelmod,
                                                          t->sample_rate,
                                                          t->rx_freq,
                                                          t->tx_bw,
                                                          30e-9, // TDL delay-spread parameter
                                                          0.0,
                                                          CORR_LEVEL_LOW,
                                                          t->chan_forgetfact, // forgetting_factor
                                                          t->chan_offset, // propagation delay in samples
                                                          t->chan_pathloss,
                                                          0); // noise_power
          set_channeldesc_owner(newmodel, RFSIMU_MODULEID);
          set_channeldesc_direction(newmodel, t->role == SIMU_ROLE_SERVER);
          set_channeldesc_name(newmodel, modelname);
          random_channel(newmodel, false);
          channel_desc_t *oldmodel = b->channel_model;
          b->channel_model = newmodel;
          free_channel_desc_scm(oldmodel);
          prnt("%s: New model type %s applied to channel %s connected to sock %d\n", __func__, modeltype, modelname, i);
          found = 1;
          break;
        }
      } /* for */
      if (found == 0)
        prnt("%s: Channel %s not found or not currently used\n", __func__, modelname);
    }
  } else {
    prnt("%s: ERROR: 2 parameters required: model name and model type (%i found)\n", __func__, s);
  }

  free(modelname);
  free(modeltype);
  return CMDSTATUS_FOUND;
}

static void getset_currentchannels_type(char *buf, int debug, webdatadef_t *tdata, telnet_printfunc_t prnt)
{
  if (strncmp(buf, "set", 3) == 0) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "setmodel %s %s", tdata->lines[0].val[1], tdata->lines[0].val[3]);
    push_telnetcmd_func_t push_telnetcmd = (push_telnetcmd_func_t)get_shlibmodule_fptr("telnetsrv", TELNET_PUSHCMD_FNAME);
    push_telnetcmd(setmodel_cmddef, cmd, prnt);
  } else {
    get_currentchannels_type("modify type", debug, tdata, prnt);
  }
}

static int rfsimu_setdistance_cmd(char *buff, int debug, telnet_printfunc_t prnt, void *arg)
{
  if (debug)
    prnt("%s() buffer \"%s\"\n", __func__, buff);

  char *modelname;
  int distance;
  int s = sscanf(buff, "%m[^ ] %d\n", &modelname, &distance);
  if (s != 2) {
    prnt("%s: require exact two parameters\n", __func__);
    return CMDSTATUS_VARNOTFOUND;
  }

  rfsimulator_state_t *t = (rfsimulator_state_t *)arg;
  const double sample_rate = t->sample_rate;
  const double c = 299792458; /* 3e8 */

  const uint64_t new_offset = (double)distance * sample_rate / c;
  const double new_distance = (double)new_offset * c / sample_rate;
  const double new_delay_ms = new_offset * 1000.0 / sample_rate;

  prnt("\n%s: new_offset %lu, new (exact) distance %.3f m, new delay %f ms\n", __func__, new_offset, new_distance, new_delay_ms);
  t->prop_delay_ms = new_delay_ms;
  t->chan_offset = new_offset;

  /* Set distance in rfsim and channel model, update channel and ringbuffer */
  for (int i = 0; i < MAX_FD_RFSIMU; i++) {
    buffer_t *b = &t->buf[i];
    if (b->conn_sock <= 0 || b->channel_model == NULL || b->channel_model->model_name == NULL || strcmp(b->channel_model->model_name, modelname) != 0) {
      if (b->channel_model != NULL && b->channel_model->model_name != NULL)
        prnt("  %s: model %s unmodified\n", __func__, b->channel_model->model_name);
      continue;
    }

    channel_desc_t *cd = b->channel_model;
    cd->channel_offset = new_offset;
  }

  free(modelname);

  return CMDSTATUS_FOUND;
}

static int rfsimu_getdistance_cmd(char *buff, int debug, telnet_printfunc_t prnt, void *arg)
{
  if (debug)
    prnt("%s() buffer \"%s\"\n", __func__, (buff != NULL) ? buff : "NULL");

  rfsimulator_state_t *t = (rfsimulator_state_t *)arg;
  const double sample_rate = t->sample_rate;
  const double c = 299792458; /* 3e8 */

  for (int i = 0; i < MAX_FD_RFSIMU; i++) {
    buffer_t *b = &t->buf[i];
    if (b->conn_sock <= 0 || b->channel_model == NULL || b->channel_model->model_name == NULL)
      continue;

    channel_desc_t *cd = b->channel_model;
    const uint64_t offset = cd->channel_offset;
    const double distance = (double)offset * c / sample_rate;
    prnt("%s: %s offset %lu distance %.3f m\n", __func__, cd->model_name, offset, distance);
  }
  prnt("%s: <default> offset %lu delay %f ms\n", __func__, t->chan_offset, t->prop_delay_ms);

  return CMDSTATUS_FOUND;
}

static int rfsimu_vtime_cmd(char *buff, int debug, telnet_printfunc_t prnt, void *arg)
{
  rfsimulator_state_t *t = (rfsimulator_state_t *)arg;
  const openair0_timestamp ts = t->nextRxTstamp;
  const double sample_rate = t->sample_rate;
  prnt("%s: vtime measurement: TS %llu sample_rate %.3f\n", __func__, ts, sample_rate);
  return CMDSTATUS_FOUND;
}

static int startServer(openair0_device *device)
{
  int sock = -1;
  struct addrinfo *results = NULL;
  struct addrinfo *rp = NULL;

  rfsimulator_state_t *t = (rfsimulator_state_t *)device->priv;
  t->role = SIMU_ROLE_SERVER;

  char port[6];
  snprintf(port, sizeof(port), "%d", t->port);

  struct addrinfo hints = {
      .ai_family = AF_INET6,
      .ai_socktype = SOCK_STREAM,
      .ai_flags = AI_PASSIVE,
  };

  int s = getaddrinfo(NULL, port, &hints, &results);
  if (s != 0) {
    LOG_E(HW, "getaddrinfo: %s\n", gai_strerror(s));
    freeaddrinfo(results);
    return -1;
  }

  int enable = 1;
  int disable = 0;
  for (rp = results; rp != NULL; rp = rp->ai_next) {
    sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sock == -1) {
      continue;
    }

    if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &disable, sizeof(int)) != 0) {
      continue;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) != 0) {
      continue;
    }

    if (bind(sock, rp->ai_addr, rp->ai_addrlen) == 0) {
      break;
    }

    close(sock);
    sock = -1;
  }

  freeaddrinfo(results);

  if (sock <= 0) {
    LOG_E(HW, "could not open a socket\n");
    return -1;
  }
  t->listen_sock = sock;

  if (listen(t->listen_sock, 5) != 0) {
    LOG_E(HW, "listen() failed, errno(%d)\n", errno);
    return -1;
  }
  struct epoll_event ev = {0};
  ev.events = EPOLLIN;
  ev.data.ptr = NULL;
  if (epoll_ctl(t->epollfd, EPOLL_CTL_ADD, t->listen_sock, &ev) != 0) {
    LOG_E(HW, "epoll_ctl(EPOLL_CTL_ADD) failed, errno(%d)\n", errno);
    return -1;
  }
  return 0;
}

static int client_try_connect(const char *host, uint16_t port)
{
  int sock = -1;
  int s;
  struct addrinfo *result = NULL;
  struct addrinfo *rp = NULL;

  char dport[6];
  snprintf(dport, sizeof(dport), "%d", port);

  struct addrinfo hints = {
      .ai_family = AF_UNSPEC,
      .ai_socktype = SOCK_STREAM,
  };

  s = getaddrinfo(host, dport, &hints, &result);
  if (s != 0) {
    LOG_E(HW, "getaddrinfo: %s\n", gai_strerror(s));
    return -1;
  }
  for (rp = result; rp != NULL; rp = rp->ai_next) {
    sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sock == -1) {
      continue;
    }

    if (connect(sock, rp->ai_addr, rp->ai_addrlen) != -1) {
      break;
    }

    close(sock);
    sock = -1;
  }

  freeaddrinfo(result);

  return sock;
}

static int startClient(openair0_device *device)
{
  rfsimulator_state_t *t = device->priv;
  t->role = SIMU_ROLE_CLIENT;
  int sock;

  while (true) {
    LOG_I(HW, "Trying to connect to %s:%d\n", t->ip, t->port);
    sock = client_try_connect(t->ip, t->port);

    if (sock > 0) {
      LOG_I(HW, "Connection to %s:%d established\n", t->ip, t->port);
      break;
    }

    LOG_I(HW, "connect() to %s:%d failed, errno(%d)\n", t->ip, t->port, errno);
    sleep(1);
  }

  if (setblocking(sock, notBlocking) == -1) {
    return -1;
  }
  buffer_t *b = allocCirBuf(t, sock);
  if (!b)
    return -1;
  // read a 1 sample block to initialize the current time
  bool have_to_wait;
  do {
    have_to_wait = true;
    flushInput(t, 3, true);
    if (b->lastReceivedTS)
      have_to_wait = false;
  } while (have_to_wait);
  if (b->lastReceivedTS > 0)
    b->lastReceivedTS--;
  t->nextRxTstamp = b->lastReceivedTS;
  LOG_D(HW, "Client got first timestamp: starting at %lu\n", t->nextRxTstamp);
  if (b->channel_model)
    b->channel_model->start_TS = t->nextRxTstamp;
  return 0;
}

static int rfsimulator_write_internal(rfsimulator_state_t *t,
                                      openair0_timestamp timestamp,
                                      void **samplesVoid,
                                      int nsamps,
                                      int nbAnt,
                                      int flags)
{
  mutexlock(t->Sockmutex);
  LOG_D(HW, "Sending %d samples at time: %ld, nbAnt %d\n", nsamps, timestamp, nbAnt);

  for (int i = 0; i < MAX_FD_RFSIMU; i++) {
    buffer_t *b = &t->buf[i];

    if (b->conn_sock >= 0) {
      samplesBlockHeader_t header = {nsamps, nbAnt, timestamp};
      fullwrite(b->conn_sock, &header, sizeof(header), t);
      if (nbAnt == 1) {
        fullwrite(b->conn_sock, samplesVoid[0], sampleToByte(nsamps, nbAnt), t);
      } else {
        sample_t tmpSamples[nsamps][nbAnt];
        for (int a = 0; a < nbAnt; a++) {
          sample_t *in = (sample_t *)samplesVoid[a];
          for (int s = 0; s < nsamps; s++)
            tmpSamples[s][a] = in[s];
        }
        fullwrite(b->conn_sock, (void *)tmpSamples, sampleToByte(nsamps, nbAnt), t);
      }
    }
  }

  if (t->lastWroteTS != 0 && fabs((double)t->lastWroteTS - timestamp) > (double)CirSize)
    LOG_W(HW, "Discontinuous TX gap too large Tx:%lu, %lu\n", t->lastWroteTS, timestamp);

  if (t->lastWroteTS > timestamp)
    LOG_W(HW, "Not supported to send Tx out of order %lu, %lu\n", t->lastWroteTS, timestamp);

  if ((flags != TX_BURST_START) && (flags != TX_BURST_START_AND_END) && (t->lastWroteTS < timestamp))
    LOG_W(HW,
          "Gap in writing to USRP: last written %lu, now %lu, gap %lu\n",
          t->lastWroteTS,
          timestamp,
          timestamp - t->lastWroteTS);

  t->lastWroteTS = timestamp + nsamps;
  mutexunlock(t->Sockmutex);

  LOG_D(HW,
        "Sent %d samples at time: %ld->%ld, energy in first antenna: %d\n",
        nsamps,
        timestamp,
        timestamp + nsamps,
        signal_energy(samplesVoid[0], nsamps));

  /* trace only first antenna */
  T(T_USRP_TX_ANT0, T_INT(timestamp), T_BUFFER(samplesVoid[0], sampleToByte(nsamps, 1)));

  return nsamps;
}

static int rfsimulator_write(openair0_device *device,
                             openair0_timestamp timestamp,
                             void **samplesVoid,
                             int nsamps,
                             int nbAnt,
                             int flags)
{
  timestamp -= device->openair0_cfg->command_line_sample_advance;
  return rfsimulator_write_internal(device->priv, timestamp, samplesVoid, nsamps, nbAnt, flags);
}

static bool add_client(rfsimulator_state_t *t)
{
  struct sockaddr_storage sa = {0};
  socklen_t socklen = sizeof(sa);
  int conn_sock = accept(t->listen_sock, (struct sockaddr *)&sa, &socklen);
  if (conn_sock == -1) {
    LOG_E(HW, "accept() failed, errno(%d)\n", errno);
    return false;
  }
  if (setblocking(conn_sock, notBlocking)) {
    return false;
  }
  mutexlock(t->Sockmutex);
  buffer_t *new_buf = allocCirBuf(t, conn_sock);
  if (new_buf == NULL) {
    mutexunlock(t->Sockmutex);
    return false;
  }
  new_buf->lastReceivedTS = t->lastWroteTS;
  char ip[INET6_ADDRSTRLEN];
  getnameinfo((struct sockaddr *)&sa, socklen, ip, sizeof(ip), NULL, 0, NI_NUMERICHOST);
  uint16_t port = ((struct sockaddr_in *)&sa)->sin_port;
  LOG_I(HW, "Client connects from %s:%d\n", ip, port);
  c16_t v = {0};
  void *samplesVoid[t->tx_num_channels];
  for (int i = 0; i < t->tx_num_channels; i++)
    samplesVoid[i] = (void *)&v;
  samplesBlockHeader_t header = {1, t->tx_num_channels, t->lastWroteTS};
  fullwrite(conn_sock, &header, sizeof(header), t);
  fullwrite(conn_sock, samplesVoid, sampleToByte(1, t->tx_num_channels), t);

  if (new_buf->channel_model)
    new_buf->channel_model->start_TS = t->lastWroteTS;
  mutexunlock(t->Sockmutex);
  return true;
}

static void process_recv_header(rfsimulator_state_t *t, buffer_t *b, bool first_time)
{
  // check the header and start block transfer
  if (b->remainToTransfer != 0)
    return;

  b->headerMode = false; // We got the header

  if (first_time) {
    b->lastReceivedTS = b->th.timestamp;
    b->trashingPacket = true;
  } else {
    if (b->lastReceivedTS < b->th.timestamp) {
      // We have a transmission hole to fill, like TDD
      // we create no signal samples up to the beginning of this reception
      int nbAnt = b->th.nbAnt;
      if (b->th.timestamp - b->lastReceivedTS < CirSize) {
        // case we wrap at circular buffer end
        for (uint64_t index = b->lastReceivedTS; index < b->th.timestamp; index++) {
          for (int a = 0; a < nbAnt; a++) {
            b->circularBuf[(index * nbAnt + a) % CirSize] = (c16_t){0};
          }
        }
      } else {
        // case no circular buffer wrap
        memset(b->circularBuf, 0, sampleToByte(CirSize, 1));
      }
      b->lastReceivedTS = b->th.timestamp;
    } else if (b->lastReceivedTS > b->th.timestamp) {
      LOG_W(HW, "Received data in past: current is %lu, new reception: %lu!\n", b->lastReceivedTS, b->th.timestamp);
      b->trashingPacket = true;
    }
    // verify the time gap is not too large
    mutexlock(t->Sockmutex);
    if (t->lastWroteTS != 0 && (fabs((double)t->lastWroteTS - b->lastReceivedTS) > (double)CirSize))
      LOG_W(HW, "UEsock(%d) Tx/Rx shift too large Tx:%lu, Rx:%lu\n", b->conn_sock, t->lastWroteTS, b->lastReceivedTS);
    mutexunlock(t->Sockmutex);
  }
  // move back the pointer to overwrite the header, we don't need it anymore
  b->transferPtr = (char *)&b->circularBuf[(b->lastReceivedTS * b->th.nbAnt) % CirSize];
  // we now need to read the samples
  b->remainToTransfer = sampleToByte(b->th.size, b->th.nbAnt);
  return;
}

static void process_recv(rfsimulator_state_t *t, buffer_t *b, bool first_time)
{
  if (b->transferPtr == b->circularBufEnd)
    b->transferPtr = (char *)b->circularBuf;

  if (!b->trashingPacket) {
    b->lastReceivedTS = b->th.timestamp + b->th.size - byteToSample(b->remainToTransfer, b->th.nbAnt);
    LOG_D(HW, "UEsock: %d Set b->lastReceivedTS %ld\n", b->conn_sock, b->lastReceivedTS);
  }

  if (b->remainToTransfer == 0) {
    LOG_D(HW, "UEsock: %d Completed block reception: %ld\n", b->conn_sock, b->lastReceivedTS);
    b->headerMode = true;
    b->transferPtr = (char *)&b->th;
    b->remainToTransfer = sizeof(samplesBlockHeader_t);
    b->trashingPacket = false;
  }
}

static bool flushInput(rfsimulator_state_t *t, int timeout, bool first_time)
{
  // Process all incoming events on sockets
  // store the data in lists
  struct epoll_event events[MAX_FD_RFSIMU] = {{0}};
  int nfds = epoll_wait(t->epollfd, events, MAX_FD_RFSIMU, timeout);

  if (nfds == -1) {
    if (!(errno == EINTR || errno == EAGAIN))
      LOG_W(HW, "epoll_wait() failed, errno(%d)\n", errno);
    return false;
  }

  for (int nbEv = 0; nbEv < nfds; ++nbEv) {
    buffer_t *b = events[nbEv].data.ptr;

    if (events[nbEv].events & EPOLLIN && b == NULL) {
      bool ret = add_client(t);
      if (!ret)
        return ret;
      continue;
    } else {
      if (events[nbEv].events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP)) {
        socketError(t, b);
        continue;
      }
    }

    if (b->circularBuf == NULL) {
      LOG_E(HW, "Received data on not connected socket %d\n", events[nbEv].data.fd);
      continue;
    }

    ssize_t blockSz;
    if (b->headerMode)
      blockSz = b->remainToTransfer;
    else
      blockSz =
          b->transferPtr + b->remainToTransfer <= b->circularBufEnd ? b->remainToTransfer : b->circularBufEnd - b->transferPtr;

    ssize_t sz = recv(b->conn_sock, b->transferPtr, blockSz, MSG_DONTWAIT);
    if (sz <= 0) {
      if (sz < 0 && errno != EAGAIN)
        LOG_E(HW, "recv() failed, errno(%d)\n", errno);
      continue;
    }
    LOG_D(HW, "Socket rcv %zd bytes\n", sz);
    b->remainToTransfer -= sz;
    b->transferPtr += sz;
    if (b->headerMode)
      process_recv_header(t, b, first_time);
    else
      process_recv(t, b, first_time);
  }
  return nfds > 0;
}

static int rfsimulator_read(openair0_device *device, openair0_timestamp *ptimestamp, void **samplesVoid, int nsamps, int nbAnt)
{
  rfsimulator_state_t *t = device->priv;
  LOG_D(HW,
        "Enter rfsimulator_read, expect %d samples, will release at TS: %ld, nbAnt %d\n",
        nsamps,
        t->nextRxTstamp + nsamps,
        nbAnt);

  // deliver data from received data
  // check if a UE is connected
  int first_sock;

  for (first_sock = 0; first_sock < MAX_FD_RFSIMU; first_sock++)
    if (t->buf[first_sock].circularBuf != NULL)
      break;

  if (first_sock == MAX_FD_RFSIMU) {
    // no connected device (we are eNB, no UE is connected)
    if (t->nextRxTstamp == 0)
      LOG_I(HW, "No connected device, generating void samples...\n");

    if (!flushInput(t, t->wait_timeout, false)) {
      for (int x = 0; x < nbAnt; x++)
        memset(samplesVoid[x], 0, sampleToByte(nsamps, 1));

      t->nextRxTstamp += nsamps;

      if (((t->nextRxTstamp / nsamps) % 100) == 0)
        LOG_D(HW, "No UE, Generating void samples for Rx: %ld\n", t->nextRxTstamp);

      *ptimestamp = t->nextRxTstamp - nsamps;
      return nsamps;
    }
  } else {
    bool have_to_wait;
    do {
      have_to_wait = false;
      buffer_t *b = NULL;
      for (int sock = 0; sock < MAX_FD_RFSIMU; sock++) {
        b = &t->buf[sock];
        if (b->circularBuf && (t->nextRxTstamp + nsamps) > b->lastReceivedTS) {
            have_to_wait = true;
            break;
        }
      }

      if (have_to_wait) {
        LOG_D(HW,
              "Waiting on socket, current last ts: %ld, expected at least : %ld\n",
              b->lastReceivedTS,
              t->nextRxTstamp + nsamps);
        flushInput(t, 3, false);
      }
    } while (have_to_wait);
  }

  struct timespec start_time;
  int ret = clock_gettime(CLOCK_REALTIME, &start_time);
  AssertFatal(ret == 0, "clock_gettime() failed: errno %d, %s\n", errno, strerror(errno));

  // Clear the output buffer
  for (int a = 0; a < nbAnt; a++)
    memset(samplesVoid[a], 0, sampleToByte(nsamps, 1));
  cf_t temp_array[nbAnt][nsamps];
  bool apply_noise_per_channel = get_noise_power_dBFS() == INVALID_DBFS_VALUE;
  int num_chanmod_channels = 0;
  // Add all input nodes signal in the output buffer
  for (int sock = 0; sock < MAX_FD_RFSIMU; sock++) {
    buffer_t *ptr = &t->buf[sock];

    if (ptr->circularBuf && !ptr->trashingPacket) {
      bool reGenerateChannel = false;

      // fixme: when do we regenerate
      //  it seems legacy behavior is: never in UL, each frame in DL
      if (reGenerateChannel)
        random_channel(ptr->channel_model, 0);

      if (t->poll_telnetcmdq)
        t->poll_telnetcmdq(t->telnetcmd_qid, t);

      for (int a_rx = 0; a_rx < nbAnt; a_rx++) { // loop over number of Rx antennas
        if (ptr->channel_model != NULL) { // apply a channel model
          if (num_chanmod_channels == 0) {
            memset(temp_array, 0, sizeof(temp_array));
          }
          num_chanmod_channels++;
          rxAddInput(ptr->circularBuf,
                     temp_array[a_rx],
                     a_rx,
                     ptr->channel_model,
                     nsamps,
                     t->nextRxTstamp,
                     CirSize,
                     apply_noise_per_channel);
        } else { // no channel modeling
          int nbAnt_tx = ptr->th.nbAnt; // number of Tx antennas
          int firstIndex = (CirSize + t->nextRxTstamp - t->chan_offset) % CirSize;
          sample_t *out = (sample_t *)samplesVoid[a_rx];
          if (nbAnt_tx == 1 && t->nb_cnx == 1) { // optimized for 1 Tx and 1 UE
            sample_t *firstSample = (sample_t *)&(ptr->circularBuf[firstIndex]);
            if (firstIndex + nsamps > CirSize) {
              int tailSz = CirSize - firstIndex;
              memcpy(out, firstSample, sampleToByte(tailSz, 1));
              memcpy(out + tailSz, &ptr->circularBuf[0], sampleToByte(nsamps - tailSz, 1));
            } else {
              memcpy(out, firstSample, sampleToByte(nsamps, 1));
            }
          } else {
            // SIMD (with simde) optimization might be added here later
            LOG_D(HW, "nbAnt_tx %d nbAnt %d\n", nbAnt_tx, nbAnt);
            double H_awgn_mimo_coeff[nbAnt_tx];
            for (int a_tx = 0; a_tx < nbAnt_tx; a_tx++) {
              uint32_t ant_diff = abs(a_tx - a_rx);
              H_awgn_mimo_coeff[a_tx] = ant_diff ? (0.2 / ant_diff) : 1.0;
            }
            for (int i = 0; i < nsamps; i++) { // loop over nsamps
              for (int a_tx = 0; a_tx < nbAnt_tx; a_tx++) { // sum up signals from nbAnt_tx antennas
                out[i].r += (short)(ptr->circularBuf[((firstIndex + i) * nbAnt_tx + a_tx) % CirSize].r * H_awgn_mimo_coeff[a_tx]);
                out[i].i += (short)(ptr->circularBuf[((firstIndex + i) * nbAnt_tx + a_tx) % CirSize].i * H_awgn_mimo_coeff[a_tx]);
              } // end for a_tx
            } // end for i (number of samps)
          } // end of 1 tx antenna optimization
        } // end of no channel modeling
      } // end for a (number of rx antennas)
    }
  }
  if (apply_noise_per_channel && num_chanmod_channels > 0) {
    // Noise is already applied through the channel model
    for (int a = 0; a < nbAnt; a++) {
      sample_t *out = (sample_t *)samplesVoid[a];
      for (int i = 0; i < nsamps; i++) {
        out[i].r += lroundf(temp_array[a][i].r);
        out[i].i += lroundf(temp_array[a][i].i);
      }
    }
  } else if (num_chanmod_channels > 0) {
    // Apply noise from global setting
    int16_t noise_power = (int16_t)(32767.0 / powf(10.0, .05 * -get_noise_power_dBFS()));
    for (int a = 0; a < nbAnt; a++) {
      sample_t *out = (sample_t *)samplesVoid[a];
      for (int i = 0; i < nsamps; i++) {
        out[i].r += lroundf(temp_array[a][i].r + noise_power * gaussZiggurat(0.0, 1.0));
        out[i].i += lroundf(temp_array[a][i].i + noise_power * gaussZiggurat(0.0, 1.0));
      }
    }
  }

  struct timespec end_time;
  ret = clock_gettime(CLOCK_REALTIME, &end_time);
  AssertFatal(ret == 0, "clock_gettime() failed: errno %d, %s\n", errno, strerror(errno));
  double diff_ns = (end_time.tv_sec - start_time.tv_sec) * 1000000000 + (end_time.tv_nsec - start_time.tv_nsec);
  static double average = 0.0;
  average = (average * 0.98) + (nsamps / (diff_ns / 1e9) * 0.02);
  static int calls = 0;
  if (calls++ % 10000 == 0) {
    LOG_D(HW, "Rfsimulator: velocity %.2f Msps, realtime requirements %.2f Msps\n", average / 1e6, t->sample_rate / 1e6);
  }

  *ptimestamp = t->nextRxTstamp; // return the time of the first sample
  t->nextRxTstamp += nsamps;
  LOG_D(HW,
        "Rx to upper layer: %d from %ld to %ld, energy in first antenna %d\n",
        nsamps,
        *ptimestamp,
        t->nextRxTstamp,
        signal_energy(samplesVoid[0], nsamps));

  /* trace only first antenna */
  T(T_USRP_RX_ANT0, T_INT(t->nextRxTstamp), T_BUFFER(samplesVoid[0], sampleToByte(nsamps, 1)));

  return nsamps;
}

static int rfsimulator_get_stats(openair0_device *device) {
  return 0;
}
static int rfsimulator_reset_stats(openair0_device *device) {
  return 0;
}
static void rfsimulator_end(openair0_device *device) {
  rfsimulator_state_t *s = device->priv;
  for (int i = 0; i < MAX_FD_RFSIMU; i++) {
    buffer_t *b = &s->buf[i];
    if (b->conn_sock >= 0)
      removeCirBuf(s, b);
  }
  close(s->epollfd);
  free(s);
}
static void stopServer(openair0_device *device)
{
  rfsimulator_state_t *t = (rfsimulator_state_t *)device->priv;
  DevAssert(t != NULL);
  close(t->listen_sock);
  rfsimulator_end(device);
}

static int rfsimulator_stop(openair0_device *device) {
  return 0;
}
static int rfsimulator_set_freq(openair0_device *device, openair0_config_t *openair0_cfg) {
  rfsimulator_state_t *s = device->priv;
  s->rx_freq = openair0_cfg->rx_freq[0];
  return 0;
}
static int rfsimulator_set_gains(openair0_device *device, openair0_config_t *openair0_cfg) {
  return 0;
}
static int rfsimulator_write_init(openair0_device *device) {
  return 0;
}

__attribute__((__visibility__("default")))
int device_init(openair0_device *device, openair0_config_t *openair0_cfg) {
  // to change the log level, use this on command line
  // --log_config.hw_log_level debug
  rfsimulator_state_t *rfsimulator = calloc(sizeof(rfsimulator_state_t), 1);
  // initialize channel simulation
  rfsimulator->tx_num_channels = openair0_cfg->tx_num_channels;
  rfsimulator->rx_num_channels = openair0_cfg->rx_num_channels;
  rfsimulator->sample_rate = openair0_cfg->sample_rate;
  rfsimulator->rx_freq = openair0_cfg->rx_freq[0];
  rfsimulator->tx_bw = openair0_cfg->tx_bw;
  rfsimulator_readconfig(rfsimulator);
  if (rfsimulator->prop_delay_ms > 0.0)
    rfsimulator->chan_offset = ceil(rfsimulator->sample_rate * rfsimulator->prop_delay_ms / 1000);
  if (rfsimulator->chan_offset != 0) {
    if (CirSize < minCirSize + rfsimulator->chan_offset) {
      CirSize = minCirSize + rfsimulator->chan_offset;
      LOG_I(HW, "CirSize = %lu\n", CirSize);
    }
    rfsimulator->prop_delay_ms = rfsimulator->chan_offset * 1000 / rfsimulator->sample_rate;
    LOG_I(HW, "propagation delay %f ms, %lu samples\n", rfsimulator->prop_delay_ms, rfsimulator->chan_offset);
  }
  mutexinit(rfsimulator->Sockmutex);
  LOG_I(HW,
        "Running as %s\n",
        rfsimulator->role == SIMU_ROLE_SERVER ? "server waiting opposite rfsimulators to connect"
                                              : "client: will connect to a rfsimulator server side");
  device->trx_start_func = rfsimulator->role == SIMU_ROLE_SERVER ? startServer : startClient;
  device->trx_get_stats_func = rfsimulator_get_stats;
  device->trx_reset_stats_func = rfsimulator_reset_stats;
  device->trx_end_func = rfsimulator->role == SIMU_ROLE_SERVER ? stopServer : rfsimulator_end;
  device->trx_stop_func = rfsimulator_stop;
  device->trx_set_freq_func = rfsimulator_set_freq;
  device->trx_set_gains_func = rfsimulator_set_gains;
  device->trx_write_func = rfsimulator_write;
  device->trx_read_func = rfsimulator_read;
  /* let's pretend to be a b2x0 */
  device->type = RFSIMULATOR;
  openair0_cfg[0].rx_gain[0] = 0;
  device->openair0_cfg = &openair0_cfg[0];
  device->priv = rfsimulator;
  device->trx_write_init = rfsimulator_write_init;

  for (int i = 0; i < MAX_FD_RFSIMU; i++)
    rfsimulator->buf[i].conn_sock = -1;
  rfsimulator->next_buf = 0;

  AssertFatal((rfsimulator->epollfd = epoll_create1(0)) != -1, "epoll_create1() failed, errno(%d)", errno);
  // we need to call randominit() for telnet server (use gaussdouble=>uniformrand)
  randominit(0);
  set_taus_seed(0);
  /* look for telnet server, if it is loaded, add the channel modeling commands to it */
  add_telnetcmd_func_t addcmd = (add_telnetcmd_func_t)get_shlibmodule_fptr("telnetsrv", TELNET_ADDCMD_FNAME);

  if (addcmd != NULL) {
    rfsimulator->poll_telnetcmdq = (poll_telnetcmdq_func_t)get_shlibmodule_fptr("telnetsrv", TELNET_POLLCMDQ_FNAME);
    addcmd("rfsimu", rfsimu_vardef, rfsimu_cmdarray);

    for (int i = 0; rfsimu_cmdarray[i].cmdfunc != NULL; i++) {
      if (rfsimu_cmdarray[i].qptr != NULL) {
        rfsimulator->telnetcmd_qid = rfsimu_cmdarray[i].qptr;
        break;
      }
    }
  }

  /* write on a socket fails if the other end is closed and we get SIGPIPE */
  if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
    perror("SIGPIPE");
    exit(1);
  }

  return 0;
}
