#include "event.h"
#include "database.h"
#include "utils.h"
#include "configuration.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

event get_event(int socket, OBUF *event_buffer, void *database)
{
#ifdef T_SEND_TIME
  struct timespec t;
#endif
  int type;
  int32_t length;

  /* Events type -1 and -2 are special: the tracee sends its version
   * of T_messages.txt using those events.
   * We have to check that the local version of T_messages.txt is identical
   * to the one the tracee uses. We don't report those events to the
   * application.
   */

again:
  if (fullread(socket, &length, 4) == -1) goto read_error;
#ifdef T_SEND_TIME
  if (length < sizeof(struct timespec)) goto error;
  if (fullread(socket, &t, sizeof(struct timespec)) == -1) goto read_error;
  length -= sizeof(struct timespec);
#endif
  if (length < sizeof(int)) goto error;
  if (fullread(socket, &type, sizeof(int)) == -1) goto read_error;
  length -= sizeof(int);
  if (event_buffer->omaxsize < length) {
    event_buffer->omaxsize = (length + 65535) & ~65535;
    event_buffer->obuf = realloc(event_buffer->obuf, event_buffer->omaxsize);
    if (event_buffer->obuf == NULL) { printf("out of memory\n"); exit(1); }
  }
  if (fullread(socket, event_buffer->obuf, length) == -1) goto read_error;
  event_buffer->osize = length;

  if (type == -1) append_received_config_chunk(event_buffer->obuf, length);
  if (type == -2) verify_config();

  if (type == -1 || type == -2) goto again;

#ifdef T_SEND_TIME
  return new_event(t, type, length, event_buffer->obuf, database);
#else
  return new_event(type, length, event_buffer->obuf, database);
#endif

error:
  printf("error: bad data in get_event()\n");

read_error:
  return (event){.type = -1};
}

#ifdef T_SEND_TIME
event new_event(struct timespec sending_time, int type,
    int length, char *buffer, void *database)
#else
event new_event(int type, int length, char *buffer, void *database)
#endif
{
  database_event_format f;
  event e;
  int i;
  int offset;

  /* arbitrary limit to 1GiB (to simplify size checks) */
  if (length < 0 || length > 1024 * 1024 * 1024) goto fatal;

#ifdef T_SEND_TIME
  e.sending_time = sending_time;
#endif
  e.type = type;
  e.buffer = buffer;

  f = get_format(database, type);
  if (f.count > T_MAX_ARGS) {
    printf("fatal: the T trace %s is defined to take %d arguments, but T_MAX_ARGS is %d. "
           "Increase T_MAX_ARGS in event.h and recompile all the tracers (make clean; make).\n",
           event_name_from_id(database, type), f.count, T_MAX_ARGS);
    exit(1);
  }

  e.ecount = f.count;

  offset = 0;

  /* setup offsets */
  /* TODO: speedup (no strcmp, string event to include length at head) */
  for (i = 0; i < f.count; i++) {
    //e.e[i].offset = offset;
    if (!strcmp(f.type[i], "int")) {
      if (offset + 4 > length) goto fatal;
      e.e[i].type = EVENT_INT;
      e.e[i].i = *(int *)(&buffer[offset]);
      offset += 4;
    } else if (!strcmp(f.type[i], "ulong")) {
      if (offset + sizeof(unsigned long) > length) goto fatal;
      e.e[i].type = EVENT_ULONG;
      e.e[i].ul = *(unsigned long *)(&buffer[offset]);
      offset += sizeof(unsigned long);
    } else if (!strcmp(f.type[i], "float")) {
      if (offset + sizeof(float) > length) goto fatal;
      e.e[i].type = EVENT_FLOAT;
      e.e[i].f = *(float *)(&buffer[offset]);
      offset += sizeof(float);
    } else if (!strcmp(f.type[i], "string")) {
      if (offset + 1 > length) goto fatal;
      e.e[i].type = EVENT_STRING;
      e.e[i].s = &buffer[offset];
      while (offset < length && buffer[offset]) offset++;
      if (offset == length) goto fatal;
      offset++;
    } else if (!strcmp(f.type[i], "buffer")) {
      if (offset + sizeof(int) > length) goto fatal;
      int len;
      e.e[i].type = EVENT_BUFFER;
      len = *(int *)(&buffer[offset]);
      if (len <= 0 || offset + len + sizeof(int) > length) goto fatal;
      e.e[i].bsize = len;
      e.e[i].b = &buffer[offset+sizeof(int)];
      offset += len+sizeof(int);
    } else {
      printf("unhandled type '%s'\n", f.type[i]);
      abort();
    }
  }

  if (e.ecount==0) { printf("FORMAT not set in event %d\n", type); abort(); }

  return e;

fatal:
  printf("fatal: bad buffer in new_event\n");
  abort();
}
