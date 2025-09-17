#include "logger.h"
#include "logger_defs.h"
#include "event.h"
#include "database.h"
#include "handler.h"
#include "filter/filter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct throughputlog {
  struct logger common;
  void *database;
  int tick_frame_arg;
  int tick_tick_arg;
  int data_frame_arg;
  int data_tick_arg;
  int data_arg;
  int last_tick_frame;
  int last_tick_tick;
  int ticks_per_frame;
  unsigned long *bits;
  unsigned long total;
  int insert_point;
  char *tick_event_name;
  unsigned long tick_handler_id;
  /* tick filter - NULL is no filter set */
  void *tick_filter;
};

static void _event(void *p, event e)
{
  struct throughputlog *l = p;
  int frame;
  int tick;
  unsigned long value;

  if (l->common.filter != NULL && filter_eval(l->common.filter, e) == 0)
    return;

  frame = e.e[l->data_frame_arg].i;
  tick = e.e[l->data_tick_arg].i;

  if (frame != l->last_tick_frame || tick != l->last_tick_tick) {
    printf("WARNING: %s:%d: data comes without previous tick!\n",
           __FILE__, __LINE__);
    return;
  }

  switch (e.e[l->data_arg].type) {
  case EVENT_INT: value = e.e[l->data_arg].i; break;
  case EVENT_ULONG: value = e.e[l->data_arg].ul; break;
  default: printf("%s:%d: unsupported type\n", __FILE__, __LINE__); abort();
  }

  l->total += value;
  l->bits[l->insert_point] += value;
}

static void _tick_event(void *p, event e)
{
  struct throughputlog *l = p;
  int i;
  int frame;
  int tick;

  if (l->tick_filter != NULL && filter_eval(l->tick_filter, e) == 0)
    return;

  frame = e.e[l->tick_frame_arg].i;
  tick = e.e[l->tick_tick_arg].i;

  for (i = 0; i < l->common.vsize; i++)
    l->common.v[i]->append(l->common.v[i], frame, tick, (double)l->total);

  while (l->last_tick_frame != frame || l->last_tick_tick != tick) {
    l->insert_point = (l->insert_point + 1) % (100 * l->ticks_per_frame);
    l->total -= l->bits[l->insert_point];
    l->bits[l->insert_point] = 0;
    l->last_tick_tick++;
    if (l->last_tick_tick == l->ticks_per_frame) {
      l->last_tick_tick = 0;
      l->last_tick_frame++;
      l->last_tick_frame %= 1024;
    }
  }

}

logger *new_throughputlog(event_handler *h, void *database,
    char *tick_event_name, char *frame_varname, char *tick_varname,
    char *event_name, char *data_varname, int ticks_per_frame)
{
  struct throughputlog *ret;
  int event_id;
  database_event_format f;
  int i;

  ret = calloc(1, sizeof(struct throughputlog)); if (ret == NULL) abort();

  ret->common.event_name = strdup(event_name);
  if (ret->common.event_name == NULL) abort();
  ret->database = database;

  ret->tick_event_name = strdup(tick_event_name);
  if (ret->tick_event_name == NULL) abort();

  /* tick event */
  event_id = event_id_from_name(database, tick_event_name);

  ret->tick_handler_id=register_handler_function(h,event_id,_tick_event,ret);

  f = get_format(database, event_id);

  /* look for frame and tick */
  ret->tick_frame_arg = -1;
  ret->tick_tick_arg = -1;
  for (i = 0; i < f.count; i++) {
    if (!strcmp(f.name[i], frame_varname)) ret->tick_frame_arg = i;
    if (!strcmp(f.name[i], tick_varname)) ret->tick_tick_arg = i;
  }
  if (ret->tick_frame_arg == -1) {
    printf("%s:%d: frame argument '%s' not found in event '%s'\n",
        __FILE__, __LINE__, frame_varname, event_name);
    abort();
  }
  if (ret->tick_tick_arg == -1) {
    printf("%s:%d: tick argument '%s' not found in event '%s'\n",
        __FILE__, __LINE__, tick_varname, event_name);
    abort();
  }
  if (strcmp(f.type[ret->tick_frame_arg], "int") != 0) {
    printf("%s:%d: argument '%s' has wrong type (should be 'int')\n",
        __FILE__, __LINE__, frame_varname);
    abort();
  }
  if (strcmp(f.type[ret->tick_tick_arg], "int") != 0) {
    printf("%s:%d: argument '%s' has wrong type (should be 'int')\n",
        __FILE__, __LINE__, tick_varname);
    abort();
  }

  /* data event */
  event_id = event_id_from_name(database, event_name);

  ret->common.handler_id = register_handler_function(h,event_id,_event,ret);

  f = get_format(database, event_id);

  /* look for frame, tick and data args */
  ret->data_frame_arg = -1;
  ret->data_tick_arg = -1;
  ret->data_arg = -1;
  for (i = 0; i < f.count; i++) {
    if (!strcmp(f.name[i], frame_varname)) ret->data_frame_arg = i;
    if (!strcmp(f.name[i], tick_varname)) ret->data_tick_arg = i;
    if (!strcmp(f.name[i], data_varname)) ret->data_arg = i;
  }
  if (ret->data_frame_arg == -1) {
    printf("%s:%d: frame argument '%s' not found in event '%s'\n",
        __FILE__, __LINE__, frame_varname, event_name);
    abort();
  }
  if (ret->data_tick_arg == -1) {
    printf("%s:%d: tick argument '%s' not found in event '%s'\n",
        __FILE__, __LINE__, tick_varname, event_name);
    abort();
  }
  if (ret->data_arg == -1) {
    printf("%s:%d: data argument '%s' not found in event '%s'\n",
        __FILE__, __LINE__, data_varname, event_name);
    abort();
  }
  if (strcmp(f.type[ret->data_frame_arg], "int") != 0) {
    printf("%s:%d: argument '%s' has wrong type (should be 'int')\n",
        __FILE__, __LINE__, frame_varname);
    abort();
  }
  if (strcmp(f.type[ret->data_tick_arg], "int") != 0) {
    printf("%s:%d: argument '%s' has wrong type (should be 'int')\n",
        __FILE__, __LINE__, tick_varname);
    abort();
  }
  if (strcmp(f.type[ret->data_arg], "int") != 0 &&
      strcmp(f.type[ret->data_arg], "float") != 0) {
    printf("%s:%d: argument '%s' has wrong type"
           " (should be 'int' or 'float')\n",
        __FILE__, __LINE__, data_varname);
    abort();
  }

  ret->ticks_per_frame = ticks_per_frame;
  ret->bits = calloc(100 * ticks_per_frame, sizeof(unsigned long));
  if (ret->bits == NULL) abort();

  return ret;
}

void throughputlog_set_tick_filter(logger *_l, void *filter)
{
  struct throughputlog *l = (struct throughputlog *)_l;
  free(l->tick_filter);
  l->tick_filter = filter;
}
