#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "utils.h"
#include "event.h"
#include "database.h"
#include "configuration.h"
#include "../T_defs.h"

void usage(void)
{
  printf(
"options:\n"
"    -d <database file>        this option is mandatory\n"
"    -ip <host>                connect to given IP address (default %s)\n"
"    -p <port>                 connect to given port (default %d)\n"
"    -e <event>                event to trace (default VCD_FUNCTION_ENB_DLSCH_ULSCH_SCHEDULER)\n"
"    -s                        stat mode, print every second min/avg/max + count\n",
  DEFAULT_REMOTE_IP,
  DEFAULT_REMOTE_PORT
  );
  exit(1);
}

struct timespec time_sub(struct timespec a, struct timespec b)
{
  struct timespec ret;
  if (a.tv_nsec < b.tv_nsec) {
    ret.tv_nsec = (int64_t)a.tv_nsec - (int64_t)b.tv_nsec + 1000000000;
    ret.tv_sec = a.tv_sec - b.tv_sec - 1;
  } else {
    ret.tv_nsec = a.tv_nsec - b.tv_nsec;
    ret.tv_sec = a.tv_sec - b.tv_sec;
  }
  return ret;
}

volatile uint64_t acc, count, acc_min, acc_max;

void *stat_thread(void *_)
{
  while (1) {
    uint64_t v = acc;
    uint64_t c = count;
    uint64_t min = acc_min;
    uint64_t max = acc_max;
    acc = acc_min = acc_max = count = 0;
    printf("%ld %ld %ld [%ld]\n", min, c==0 ? 0 : v/c, max, c);
    sleepms(1000);
  }
}

int main(int n, char **v)
{
  char *database_filename = NULL;
  void *database;
  char *ip = DEFAULT_REMOTE_IP;
  int port = DEFAULT_REMOTE_PORT;
  int i;
  char t;
  int number_of_events;
  int socket;
  int *is_on;
  int ev_fun;
  int start_valid = 0;
  struct timespec start_time, stop_time, delta_time;
  char *name = "VCD_FUNCTION_ENB_DLSCH_ULSCH_SCHEDULER";
  int stat_mode = 0;

  for (i = 1; i < n; i++) {
    if (!strcmp(v[i], "-h") || !strcmp(v[i], "--help")) usage();
    if (!strcmp(v[i], "-d"))
      { if (i > n-2) usage(); database_filename = v[++i]; continue; }
    if (!strcmp(v[i], "-ip")) { if (i > n-2) usage(); ip = v[++i]; continue; }
    if (!strcmp(v[i], "-p"))
      { if (i > n-2) usage(); port = atoi(v[++i]); continue; }
    if (!strcmp(v[i], "-e")) { if (i > n-2) usage(); name = v[++i]; continue; }
    if (!strcmp(v[i], "-s")) { stat_mode = 1; continue; }
    usage();
  }

  if (stat_mode)
    new_thread(stat_thread, NULL);

  if (database_filename == NULL) {
    printf("ERROR: provide a database file (-d)\n");
    exit(1);
  }

  database = parse_database(database_filename);

  load_config_file(database_filename);

  number_of_events = number_of_ids(database);
  is_on = calloc(number_of_events, sizeof(int));
  if (is_on == NULL) abort();

  on_off(database, name, is_on, 1);

  ev_fun = event_id_from_name(database, name);

  socket = connect_to(ip, port);

  t = 1;
  if (socket_send(socket, &t, 1) == -1 ||
      socket_send(socket, &number_of_events, sizeof(int)) == -1 ||
      socket_send(socket, is_on, number_of_events * sizeof(int)) == -1)
    abort();

  OBUF ebuf = {.osize = 0, .omaxsize = 0, .obuf = NULL};

  while (1) {
    event e;
    int on_off;
    e = get_event(socket, &ebuf, database);
    if (e.type == -1) break;
    if (e.type != ev_fun)
      { printf("unhandled event %d\n", e.type); continue; }
    on_off = e.e[0].i;
    if (!stat_mode)
      printf("yo %d\n", on_off);
    if (on_off == 1) {
      start_time = e.sending_time;
      start_valid = 1;
      continue;
    }
    if (on_off != 0) { printf("fatal!\n"); abort(); }
    if (!start_valid) continue;
    start_valid = 0;
    stop_time = e.sending_time;
    delta_time = time_sub(stop_time, start_time);
    if (stat_mode) {
      uint64_t v = delta_time.tv_sec * 1000000000UL + delta_time.tv_nsec;
      if (count == 0 || v < acc_min) acc_min = v;
      if (v > acc_max) acc_max = v;
      acc += v;
      count++;
    } else {
      fprintf(stderr, "%ld\n",
              delta_time.tv_sec * 1000000000UL + delta_time.tv_nsec);
      fflush(stderr);
    }
  }

  return 0;
}
