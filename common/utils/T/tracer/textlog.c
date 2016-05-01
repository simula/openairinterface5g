#include "textlog.h"
#include "handler.h"
#include "database.h"
#include "view/view.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

enum format_item_type {
  INSTRING,
  INT, STRING, BUFFER };

struct format_item {
  enum format_item_type type;
  union {
    /* INSTRING */
    char *s;
    /* others */
    int event_arg;
  };
};

struct textlog {
  char *event_name;
  char *format;
  void *database;
  unsigned long handler_id;
  /* parsed format string */
  struct format_item *f;
  int fsize;
  /* list of views */
  view **v;
  int vsize;
  /* local output buffer */
  OBUF o;
};

static void _event(void *p, event e)
{
  struct textlog *l = p;
  int i;

  l->o.osize = 0;

  for (i = 0; i < l->fsize; i++)
  switch(l->f[i].type) {
  case INSTRING: PUTS(&l->o, l->f[i].s); break;
  case INT:      PUTI(&l->o, e.e[l->f[i].event_arg].i); break;
  case STRING:   PUTS(&l->o, e.e[l->f[i].event_arg].s); break;
  case BUFFER:
    PUTS(&l->o, "{buffer size:");
    PUTI(&l->o, e.e[l->f[i].event_arg].bsize);
    PUTS(&l->o, "}");
    break;
  }
  PUTC(&l->o, 0);

  for (i = 0; i < l->vsize; i++) l->v[i]->append(l->v[i], l->o.obuf);
}

enum chunk_type { C_ERROR, C_STRING, C_ARG_NAME, C_EVENT_NAME };
struct chunk {
  enum chunk_type type;
  char *s;
  enum format_item_type it;
  int event_arg;
};

/* TODO: speed it up? */
static int find_argument(char *name, database_event_format f,
    enum format_item_type *it, int *event_arg)
{
  int i;
  for (i = 0; i < f.count; i++) if (!strcmp(name, f.name[i])) break;
  if (i == f.count) return 0;
  *event_arg = i;
  if (!strcmp(f.type[i], "int"))         *it = INT;
  else if (!strcmp(f.type[i], "string")) *it = STRING;
  else if (!strcmp(f.type[i], "buffer")) *it = BUFFER;
  else return 0;
  return 1;
}

static struct chunk next_chunk(char **s, database_event_format f)
{
  char *cur = *s;
  char *name;
  enum format_item_type it;
  int event_arg;

  /* argument in [ ] */
  if (*cur == '[') {
    *cur = 0;
    cur++;
    name = cur;
    /* no \ allowed there */
    while (*cur && *cur != ']' && *cur != '\\') cur++;
    if (*cur != ']') goto error;
    *cur = 0;
    cur++;
    *s = cur;
    if (find_argument(name, f, &it, &event_arg) == 0) goto error;
    return (struct chunk){type:C_ARG_NAME, s:name, it:it, event_arg:event_arg};
  }

  /* { } is name of event (anything in between is smashed) */
  if (*cur == '{') {
    *cur = 0;
    cur++;
    while (*cur && *cur != '}') cur++;
    if (*cur != '}') goto error;
    *cur = 0;
    cur++;
    *s = cur;
    return (struct chunk){type:C_EVENT_NAME};
  }

  /* anything but [ and { is raw string */
  /* TODO: deal with \ */
  name = cur;
  while (*cur && *cur != '[' && *cur != '{') cur++;
  *s = cur;
  return (struct chunk){type:C_STRING, s:name};

error:
  return (struct chunk){type:C_ERROR};
}

textlog *new_textlog(event_handler *h, void *database,
    char *event_name, char *format)
{
  struct textlog *ret;
  int event_id;
  database_event_format f;
  char *cur;

  ret = calloc(1, sizeof(struct textlog)); if (ret == NULL) abort();

  ret->event_name = strdup(event_name); if (ret->event_name == NULL) abort();
  ret->format = strdup(format); if (ret->format == NULL) abort();
  ret->database = database;

  event_id = event_id_from_name(database, event_name);

  ret->handler_id = register_handler_function(h, event_id, _event, ret);

  f = get_format(database, event_id);

  /* we won't get more than strlen(format) "chunks" */
  ret->f = malloc(sizeof(struct format_item) * strlen(format));
  if (ret->f == NULL) abort();

  cur = ret->format;

  while (*cur) {
    struct chunk c = next_chunk(&cur, f);
    switch (c.type) {
    case C_ERROR: goto error;
    case C_STRING:
      ret->f[ret->fsize].type = INSTRING;
      ret->f[ret->fsize].s = c.s;
      break;
    case C_ARG_NAME:
      ret->f[ret->fsize].type = c.it;
      ret->f[ret->fsize].event_arg = c.event_arg;
      break;
    case C_EVENT_NAME:
      ret->f[ret->fsize].type = INSTRING;
      ret->f[ret->fsize].s = ret->event_name;
      break;
    }
    ret->fsize++;
  }

  return ret;

error:
  printf("%s:%d: bad format '%s'\n", __FILE__, __LINE__, format);
  abort();
}

void textlog_add_view(textlog *_l, view *v)
{
  struct textlog *l = _l;
  l->vsize++;
  l->v = realloc(l->v, l->vsize * sizeof(view *)); if (l->v == NULL) abort();
  l->v[l->vsize-1] = v;
}
