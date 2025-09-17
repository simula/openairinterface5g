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

#include "time_client.h"

#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "common/utils/assertions.h"
#include "common/utils/LOG/log.h"
#include "common/utils/system.h"

typedef struct {
  pthread_t thread_id;
  int exit_fd;
  struct sockaddr_in server_ip;
  int server_port;
  void (*callback)(void *);
  void *callback_data;
} time_client_private_t;

static int connect_to_server(time_client_private_t *time_client)
{
  int sock = -1;
  int opts;
  int r;
  struct pollfd fds[2];

  /* try forever (or exit requested) until we succeed */
  while (1) {
    sock = socket(AF_INET, SOCK_STREAM, 0);
    AssertFatal(sock != -1, "socket() failed: %s\n", strerror(errno));
    int v = 1;
    r = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &v, sizeof(v));
    AssertFatal(r == 0, "setsockopt() failed: %s\n", strerror(errno));
    /* unblock socket */
    opts = fcntl(sock, F_GETFL);
    AssertFatal(opts >= 0, "fcntl() failed: %s\n", strerror(errno));
    opts |= O_NONBLOCK;
    r = fcntl(sock, F_SETFL, opts);
    AssertFatal(r == 0, "fctnl() failed: %s\n", strerror(errno));
    if (connect(sock, (struct sockaddr *)&time_client->server_ip, sizeof(time_client->server_ip)) == 0)
      break;
    if (errno == EINPROGRESS) {
      /* connection not completed yet, we need to wait */
      fds[0].fd = sock;
      fds[0].events = POLLOUT;
      fds[1].fd = time_client->exit_fd;
      fds[1].events = POLLIN;
      r = poll(fds, 2, -1);
      AssertFatal(r >= 0, "poll() failed: %s\n", strerror(errno));
      /* do we need to exit? */
      if (fds[1].revents) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
        sock = -1;
        break;
      }
      /* check if connection is successful */
      if (fds[0].revents & POLLOUT) {
        int so_error;
        socklen_t so_error_len = sizeof(so_error);
        r = getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &so_error_len);
        AssertFatal(r == 0 && so_error_len == sizeof(so_error),
                    "getsockopt() failed (r = %d) or so_error_len (= %d) is not %ld\n",
                    r, so_error_len, sizeof(so_error));
        if (so_error == 0)
          /* success */
          break;
      }
    }
    /* connection failed, try again in 1s (or stop if exit requested) */
    close(sock);
    sock = -1;
    char server_address[256];
    const char *ret = inet_ntop(AF_INET, &time_client->server_ip.sin_addr, server_address, sizeof(server_address));
    AssertFatal(ret != NULL, "inet_ntop() failed: %s\n", strerror(errno));
    LOG_W(UTIL, "time client: connection to %s:%d failed, try again in 1s\n", server_address, time_client->server_port);
    /* sleep 1s (or exit requested) */
    fds[0].fd = time_client->exit_fd;
    fds[0].events = POLLIN;
    r = poll(fds, 1, 1000);
    AssertFatal(r >= 0, "poll() failed: %s\n", strerror(errno));
    /* exit requested? */
    if (fds[0].revents) {
      shutdown(sock, SHUT_RDWR);
      close(sock);
      sock = -1;
      break;
    }
  }
  if (sock != -1) {
    /* reblock socket */
    opts &= ~O_NONBLOCK;
    r = fcntl(sock, F_SETFL, opts);
    AssertFatal(r == 0, "fcntl() failed: %s\n", strerror(errno));
  }
  return sock;
}

static void *time_client_thread(void *tc)
{
  time_client_private_t *time_client = tc;
  int ret;
  int socket = connect_to_server(time_client);
  /* if socket is -1 at this point, it means that exit was requested */
  if (socket == -1)
    return NULL;

  while (1) {
    struct pollfd polls[2];
    /* polls[0] is for the socket */
    polls[0].fd = socket;
    polls[0].events = POLLIN;

    /* polls[1] is for exit_fd */
    polls[1].fd = time_client->exit_fd;
    polls[1].events = POLLIN;

    ret = poll(polls, 2, -1);
    AssertFatal(ret >= 0, "poll() failed: %s\n", strerror(errno));

    /* whatever is in revents for exit_fd means exit */
    if (polls[1].revents)
      break;

    /* is socket fine? */
    if (polls[0].revents & (POLLERR | POLLHUP)) {
reconnect:
      /* socket not fine, reconnect */
      shutdown(socket, SHUT_RDWR);
      close(socket);
      socket = connect_to_server(time_client);
      /* connect only fails if exit requested, we can safely 'continue' */
      continue;
    }

    /* here we only accept POLLIN */
    AssertFatal(polls[0].revents == POLLIN, "poll() did not return expected POLLIN\n");

    /* get the ticks from server */
    uint8_t tick_count;
    ret = read(socket, &tick_count, 1);
    /* any error => reconnect */
    if (ret == -1) {
      LOG_E(UTIL, "time client: error %s, reconnect\n", strerror(errno));
      goto reconnect;
    }
    /* if 0 is returned, it means socket has been closed by other end */
    if (ret == 0) {
      LOG_E(UTIL, "time client: socket closed by other end, reconnect\n");
      goto reconnect;
    }
    DevAssert(ret == 1);

    /* call callback once for each tick */
    for (int i = 0; i < tick_count; i++)
      time_client->callback(time_client->callback_data);
  }

  shutdown(socket, SHUT_RDWR);
  close(socket);

  return NULL;
}

time_client_t *new_time_client(const char *server_ip,
                               int server_port,
                               void (*callback)(void *),
                               void *callback_data)
{
  time_client_private_t *tc;
  int ret;

  tc = calloc_or_fail(1, sizeof(*tc));

  tc->exit_fd = eventfd(0, 0);
  AssertFatal(tc->exit_fd != -1, "eventfd() failed: %s\n", strerror(errno));

  ret = inet_aton(server_ip, &tc->server_ip.sin_addr);
  AssertFatal(ret != 0, "inet_aton() failed: %s\n", strerror(errno));
  tc->server_ip.sin_family = AF_INET;
  tc->server_ip.sin_port = htons(server_port);
  tc->server_port = server_port;
  tc->callback = callback;
  tc->callback_data = callback_data;

  threadCreate(&tc->thread_id, time_client_thread, tc, "time client", -1, SCHED_OAI);

  return tc;
}

void free_time_client(time_client_t *tc)
{
  time_client_private_t *time_client = tc;

  uint64_t v = 1;
  int ret = write(time_client->exit_fd, &v, 8);
  DevAssert(ret == 8);

  ret = pthread_join(time_client->thread_id, NULL);
  DevAssert(ret == 0);

  close(time_client->exit_fd);
  free(time_client);
}
