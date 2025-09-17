#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "database.h"
//#include "utils.h"
#include "configuration.h"
#include "event.h"

void usage(void)
{
  fprintf(stderr,
"options:\n"
"    -d <database file>        this option is mandatory\n"
"    -o <output file>          this option is mandatory (use - to dump to stdout)\n"
"    -ip <host>                connect to given IP address (default %s)\n"
"    -p <port>                 connect to given port (default %d)\n"
"    -n <max>                  save maximum 'max' IQ points (may be a bit more)\n"
"    -nosig                    don't intercept ctrl+c, ctrl+z (it may mess up when piping to another tool)\n"
"    -e <event name>           trace given event (default 'USRP_TX_ANT0')\n"
"    -b <buffer name>          trace given buffer (default 'data')\n"
"    -no-timestamp             don't use 'timestamp' to fill missing transmission\n"
"    -scale <scale value>      divide 16bits input by given value for float output (default 32768.)\n"
"    -s16                      output short instead of float\n",
  DEFAULT_REMOTE_IP,
  DEFAULT_REMOTE_PORT
  );
  exit(1);
}

volatile int run = 1;

static int socket = -1;

void force_stop(int x)
{
  fprintf(stderr, "\ngently quit...\n");
  close(socket);
  socket = -1;
  run = 0;
}

int main(int n, char **v)
{
  char *database_filename = NULL;
  char *output_filename = NULL;
  FILE *out;
  void *database;
  char *ip = DEFAULT_REMOTE_IP;
  int port = DEFAULT_REMOTE_PORT;
  int *is_on;
  int number_of_events;
  int i;
  char mt;
  long max = -1;
  long dumped = 0;
  int nosig = 0;
  char *event_name = "USRP_TX_ANT0";
  char *buffer_name = "data";
  int no_timestamp = 0;
  float scale = 32768;
  int do_float = 1;

  /* write on a socket fails if the other end is closed and we get SIGPIPE */
  if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) abort();

  for (i = 1; i < n; i++) {
    if (!strcmp(v[i], "-h") || !strcmp(v[i], "--help")) usage();
    if (!strcmp(v[i], "-d")) { if (i > n-2) usage(); database_filename = v[++i]; continue; }
    if (!strcmp(v[i], "-o")) { if (i > n-2) usage(); output_filename = v[++i]; continue; }
    if (!strcmp(v[i], "-ip")) { if (i > n-2) usage(); ip = v[++i]; continue; }
    if (!strcmp(v[i], "-p")) { if (i > n-2) usage(); port = atoi(v[++i]); continue; }
    if (!strcmp(v[i], "-n")) { if (i > n-2) usage(); max = atol(v[++i]); continue; }
    if (!strcmp(v[i], "-nosig")) { nosig = 1; continue; }
    if (!strcmp(v[i], "-e")) { if (i > n-2) usage(); event_name = v[++i]; continue; }
    if (!strcmp(v[i], "-b")) { if (i > n-2) usage(); buffer_name = v[++i]; continue; }
    if (!strcmp(v[i], "-no-timestamp")) { no_timestamp = 1; continue; }
    if (!strcmp(v[i], "-scale")) { if (i > n-2) usage(); scale = atof(v[++i]); continue; }
    if (!strcmp(v[i], "-s16")) { do_float = 0; continue; }
    usage();
  }

  if (database_filename == NULL) {
    fprintf(stderr, "ERROR: provide a database file (-d)\n");
    exit(1);
  }
  if (output_filename == NULL) {
    fprintf(stderr, "ERROR: provide an output file (-o)\n");
    exit(1);
  }

  database = parse_database(database_filename);
  load_config_file(database_filename);

  /* get the event and fields from database */
  int tx_ant0_id = event_id_from_name(database, event_name);
  database_event_format f = get_format(database, tx_ant0_id);

#define G(var_name, var_type, var) \
  if (!strcmp(f.name[i], var_name)) { \
    if (strcmp(f.type[i], var_type)) abort(); \
    var = i; \
    continue; \
  }

  int timestamp_id = -1;
  int data_id = -1;
  for (i = 0; i < f.count; i++) {
    /* there is a typo in T_messages.txt, "timestap" instead of "timestamp" */
    if (!no_timestamp)
      G("timestap", "int", timestamp_id);
    G(buffer_name, "buffer", data_id);
  }
  if ((!no_timestamp && timestamp_id == -1) || data_id == -1) {
    if (data_id == -1)
      fprintf(stderr, "fatal: field '%s' of event '%s' does not exist\n",
              buffer_name, event_name);
    if (!no_timestamp && timestamp_id == -1)
      fprintf(stderr, "fatal: field 'timestamp' of event '%s' does not exist\n",
              event_name);
    exit(1);
  }

  number_of_events = number_of_ids(database);
  is_on = calloc(number_of_events, sizeof(int));
  if (is_on == NULL) abort();

  on_off(database, event_name, is_on, 1);

  /* connect to tracee */
  socket = connect_to(ip, port);

  /* activate trace */
  mt = 1;
  if (socket_send(socket, &mt, 1) == -1 ||
      socket_send(socket, &number_of_events, sizeof(int)) == -1 ||
      socket_send(socket, is_on, number_of_events * sizeof(int)) == -1)
    abort();

  /* - means to output to stdout */
  if (!strcmp(output_filename, "-")) {
    out = stdout;
  } else {
    out = fopen(output_filename, "w");
    if (out == NULL) {
      perror(output_filename);
      exit(1);
    }
  }

  if (!nosig) {
    /* exit on ctrl+c and ctrl+z */
    if (signal(SIGQUIT, force_stop) == SIG_ERR) abort();
    if (signal(SIGINT, force_stop) == SIG_ERR) abort();
    if (signal(SIGTSTP, force_stop) == SIG_ERR) abort();
  }

  OBUF ebuf = {.osize = 0, .omaxsize = 0, .obuf = NULL};

  /* read messages */
  unsigned int last_timestamp = 0;
  int last_count = 0;
  int first = 1;

  /* stop if max reached */
  while ((max == -1 || dumped < max) && run) {
    event e;
    e = get_event(socket, &ebuf, database);

    if (e.type == -1) break;
    if (e.type != tx_ant0_id) continue;

    unsigned int timestamp = 0;
    if (!no_timestamp)
      timestamp = e.e[timestamp_id].i;
    int count = e.e[data_id].bsize / 4;

#if 0
    fprintf(stderr, "timestamp %u buffer %p count %d not transmitted %d delta %d\n",
           timestamp,
           e.e[data_id].b,
           count,
           timestamp - (last_timestamp + last_count),
           timestamp - last_timestamp);
#endif

    /* rfsimulator may send 1 sample with old time when a new client connects
     * let's reject this packet
     */
    if (!first && timestamp < last_timestamp + last_count) {
      /* more than 1 sample is not accepted */
      if (count != 1) {
        fprintf(stderr, "fatal: past transmission detected (%d samples)\n", count);
        abort();
      }
      fprintf(stderr, "past transmission detected (%d samples), ignore\n", count);
      continue;
    }

    /* insert missing samples */
    if (!no_timestamp && !first) {
      int missing = timestamp - (last_timestamp + last_count);
      if (missing < 0) abort();
      while (missing) {
        /* limit size of array miss[], it's allocated on the stack */
        int count = missing > 65536 ? 65536 : missing;
        float miss[count * 2];
        memset(miss, 0, count * 2 * sizeof(float));

        if (fwrite(miss, count * 2 * sizeof(float), 1, out) != 1) {
          fprintf(stderr, "error writing to %s\n", output_filename);
          if (strcmp(output_filename, "-"))
            fclose(out);
          close(socket);
          exit(1);
        }
        dumped += count;
        missing -= count;
      }
    }

    first = 0;

    if (do_float) {
      /* transform to float */
      float iq[count * 2];
      short *iq_short = (short *)e.e[data_id].b;
      for (int i = 0; i < count * 2; i++)
        iq[i] = iq_short[i] / scale;

      /* save */
      if (fwrite(iq, count * 2 * sizeof(float), 1, out) != 1) {
        fprintf(stderr, "error writing to %s\n", output_filename);
        if (strcmp(output_filename, "-"))
          fclose(out);
        exit(1);
      }
    } else {
      /* save directly as short */
      if (fwrite(e.e[data_id].b, count * 2 * 2, 1, out) != 1) {
        fprintf(stderr, "error writing to %s\n", output_filename);
        if (strcmp(output_filename, "-"))
          fclose(out);
        exit(1);
      }
    }
    dumped += count;

    last_timestamp = timestamp;
    last_count = count;
  }

  if (strcmp(output_filename, "-"))
    fclose(out);
  close(socket);

  return 0;
}
