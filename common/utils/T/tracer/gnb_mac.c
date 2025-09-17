#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"
#include "gui/gui.h"
#include "view/view.h"
#include "logger/logger.h"
#include "filter/filter.h"
#include "database.h"
#include "configuration.h"
#include "handler.h"

typedef struct {
  int used;
} ue_list_item;

typedef struct {
  widget *container;
} gnb_gui;

typedef struct {
  int socket;
  gui *g;
  gnb_gui *gg;
  event_handler *h;
  void *database;
  ue_list_item ues[65536];
  int rnti_arg;
} gnb_data;

void *gui_thread(void *_g)
{
  gui *g = _g;
  gui_loop(g);
  return 0;
}

void add_ue(gnb_data *gd, int rnti, int lcid)
{
  event_handler *h = gd->h;
  void *database = gd->database;
  gnb_gui *gg = gd->gg;
  gui *g = gd->g;
  widget *line;
  widget *w, *w2, *col;
  view *v;
  logger *l;

  gd->ues[rnti].used = 1;

  line = new_container(g, HORIZONTAL);
  widget_add_child(g, gg->container, line, -1);

  /* label */
  char s[512];
  sprintf(s, "UE %d", rnti);
  widget *label = new_label(g, s);
  widget_add_child(g, line, label, -1);

  /* DL MCS */
  w = new_xy_plot(g, 128, 55, "DL mcs", 20);
  xy_plot_set_range(g, w, 0, 1024*20, -2, 30);
  widget_add_child(g, line, w, -1);
  l = new_ticked_ttilog(h, database, "GNB_PHY_DL_TICK", "frame", "slot",
      "GNB_MAC_DL", "mcs", 0, -1);
  v = new_view_tti(10, g, w, new_color(g, "#0c0c72"), 20);
  logger_add_view(l, v);
  logger_set_filter(l,
      filter_eq(
        filter_evarg(database, "GNB_MAC_DL", "rnti"),
        filter_int(rnti)));

  /* DL TBS */
  col = new_container(g, VERTICAL);
  widget_add_child(g, line, col, -1);
  w = new_xy_plot(g, 70, 10, "DL tbs", 35);
  w2 = new_textarea(g, 70, 11, 64);
  xy_plot_set_range(g, w, 0, 1024*20, 0, 1000);
  xy_plot_set_tick_type(g, w, XY_PLOT_SCROLL_TICK);
  widget_add_child(g, col, w2, -1);
  widget_add_child(g, col, w, -1);
  container_set_child_growable(g, col, w, 1);
  l = new_ticked_ttilog(h, database, "GNB_PHY_DL_TICK", "frame", "slot",
      "GNB_MAC_DL", "tbs", 0, -1);
  v = new_view_tti(10, g, w, new_color(g, "#0c0c72"), 20);
  view_tti_enable_automax(v, w2);
  logger_add_view(l, v);
  logger_set_filter(l,
      filter_eq(
        filter_evarg(database, "GNB_MAC_DL", "rnti"),
        filter_int(rnti)));

  /* DL lcid throughput */
  col = new_container(g, VERTICAL);
  widget_add_child(g, line, col, -1);
  sprintf(s, "DL lcid %d", lcid);
  w = new_xy_plot(g, 70, 10, s, 35);
  w2 = new_textarea(g, 70, 11, 64);
  xy_plot_set_range(g, w, 0, 1000, 0, 100000);
  xy_plot_set_tick_type(g, w, XY_PLOT_SCROLL_TICK);
  widget_add_child(g, col, w2, -1);
  widget_add_child(g, col, w, -1);
  container_set_child_growable(g, col, w, 1);
  l = new_throughputlog(h, database, "GNB_PHY_DL_TICK", "frame", "slot",
      "GNB_MAC_LCID_DL", "data_size", 20);
  v = new_view_scrolltti(10, g, w, new_color(g, "#0c0c72"), w2, 20);
  logger_add_view(l, v);
  logger_set_filter(l,
      filter_and(
        filter_eq(
          filter_evarg(database, "GNB_MAC_LCID_DL", "rnti"),
          filter_int(rnti)),
        filter_eq(
          filter_evarg(database, "GNB_MAC_LCID_DL", "lcid"),
          filter_int(lcid))));

  /* UL MCS */
  w = new_xy_plot(g, 128, 55, "UL mcs", 20);
  xy_plot_set_range(g, w, 0, 1024*20, -2, 30);
  widget_add_child(g, line, w, -1);
  /* GNB_PHY_DL_TICK (and not UL) because GNB_MAC_UL is from DCI */
  l = new_ticked_ttilog(h, database, "GNB_PHY_DL_TICK", "frame", "slot",
      "GNB_MAC_UL", "mcs", 0, -1);
  v = new_view_tti(10, g, w, new_color(g, "#0c0c72"), 20);
  logger_add_view(l, v);
  logger_set_filter(l,
      filter_eq(
        filter_evarg(database, "GNB_MAC_UL", "rnti"),
        filter_int(rnti)));

  /* UL TBS */
  col = new_container(g, VERTICAL);
  widget_add_child(g, line, col, -1);
  w = new_xy_plot(g, 70, 10, "UL tbs", 35);
  w2 = new_textarea(g, 70, 11, 64);
  xy_plot_set_range(g, w, 0, 1024*20, 0, 1000);
  xy_plot_set_tick_type(g, w, XY_PLOT_SCROLL_TICK);
  widget_add_child(g, col, w2, -1);
  widget_add_child(g, col, w, -1);
  container_set_child_growable(g, col, w, 1);
  /* GNB_PHY_DL_TICK (and not UL) because GNB_MAC_UL is from DCI */
  l = new_ticked_ttilog(h, database, "GNB_PHY_DL_TICK", "frame", "slot",
      "GNB_MAC_UL", "tbs", 0, -1);
  v = new_view_tti(10, g, w, new_color(g, "#0c0c72"), 20);
  view_tti_enable_automax(v, w2);
  logger_add_view(l, v);
  logger_set_filter(l,
      filter_eq(
        filter_evarg(database, "GNB_MAC_UL", "rnti"),
        filter_int(rnti)));

  /* UL lcid throughput */
  col = new_container(g, VERTICAL);
  widget_add_child(g, line, col, -1);
  sprintf(s, "UL lcid %d", lcid);
  w = new_xy_plot(g, 70, 10, s, 35);
  w2 = new_textarea(g, 70, 11, 64);
  xy_plot_set_range(g, w, 0, 1000, 0, 100000);
  xy_plot_set_tick_type(g, w, XY_PLOT_SCROLL_TICK);
  widget_add_child(g, col, w2, -1);
  widget_add_child(g, col, w, -1);
  container_set_child_growable(g, col, w, 1);
  l = new_throughputlog(h, database, "GNB_PHY_UL_TICK", "frame", "slot",
      "GNB_MAC_LCID_UL", "data_size", 20);
  v = new_view_scrolltti(10, g, w, new_color(g, "#0c0c72"), w2, 20);
  logger_add_view(l, v);
  logger_set_filter(l,
      filter_and(
        filter_eq(
          filter_evarg(database, "GNB_MAC_LCID_UL", "rnti"),
          filter_int(rnti)),
        filter_eq(
          filter_evarg(database, "GNB_MAC_LCID_UL", "lcid"),
          filter_int(lcid))));

  /* lcid RLC tx buffer occupancy */
  col = new_container(g, VERTICAL);
  widget_add_child(g, line, col, -1);
  sprintf(s, "rlc tx %d", lcid);
  w = new_xy_plot(g, 70, 10, s, 35);
  w2 = new_textarea(g, 70, 11, 64);
  xy_plot_set_range(g, w, 0, 1024*20, 0, 1000);
  xy_plot_set_tick_type(g, w, XY_PLOT_SCROLL_TICK);
  widget_add_child(g, col, w2, -1);
  widget_add_child(g, col, w, -1);
  container_set_child_growable(g, col, w, 1);
  l = new_ticked_ttilog(h, database, "GNB_PHY_DL_TICK", "frame", "slot",
      "GNB_MAC_LCID_DL", "tx_list_occupancy", 0, -1);
  v = new_view_tti(10, g, w, new_color(g, "#0c0c72"), 20);
  view_tti_enable_automax(v, w2);
  logger_add_view(l, v);
  logger_set_filter(l,
      filter_and(
        filter_eq(
          filter_evarg(database, "GNB_MAC_LCID_DL", "rnti"),
          filter_int(rnti)),
        filter_eq(
          filter_evarg(database, "GNB_MAC_LCID_DL", "lcid"),
          filter_int(lcid))));
}

void build_gui(gnb_gui *gg, gui *g)
{
  widget *main_window;

  main_window = new_toplevel_window(g, 900, 1000, "gNB MAC tracer");
  gg->container = new_container(g, VERTICAL);
  widget_add_child(g, main_window, gg->container, -1);
}

void usage(void)
{
  printf(
"options:\n"
"    -d <database file>        this option is mandatory\n"
"    -ip <host>                connect to given IP address (default %s)\n"
"    -p <port>                 connect to given port (default %d)\n",
  DEFAULT_REMOTE_IP,
  DEFAULT_REMOTE_PORT
  );
  exit(1);
}

void check_new_ue(void *_gd, event e)
{
  gnb_data *gd = _gd;
  int rnti = e.e[gd->rnti_arg].i;
  if (!gd->ues[rnti].used)
    add_ue(gd, rnti, 4);
}

int main(int n, char **v)
{
  gui *g;
  gnb_gui gg;
  gnb_data *gd;
  char *database_filename = NULL;
  void *database;
  char *ip = DEFAULT_REMOTE_IP;
  int port = DEFAULT_REMOTE_PORT;
  event_handler *h;
  int *is_on;
  int number_of_events;
  int i;

  for (i = 1; i < n; i++) {
    if (!strcmp(v[i], "-h") || !strcmp(v[i], "--help")) usage();
    if (!strcmp(v[i], "-d")) { if (i > n-2) usage(); database_filename = v[++i]; continue; }
    if (!strcmp(v[i], "-ip")) { if (i > n-2) usage(); ip = v[++i]; continue; }
    if (!strcmp(v[i], "-p")) { if (i > n-2) usage(); port = atoi(v[++i]); continue; }
    usage();
  }

  if (database_filename == NULL) {
    printf("ERROR: provide a database file (-d)\n");
    exit(1);
  }

  database = parse_database(database_filename);

  load_config_file(database_filename);

  number_of_events = number_of_ids(database);
  is_on = calloc(number_of_events, sizeof(int));
  if (is_on == NULL) abort();

  on_off(database, "GNB_PHY_DL_TICK", is_on, 1);
  on_off(database, "GNB_MAC_DL", is_on, 1);
  on_off(database, "GNB_MAC_LCID_DL", is_on, 1);
  on_off(database, "GNB_PHY_UL_TICK", is_on, 1);
  on_off(database, "GNB_MAC_UL", is_on, 1);
  on_off(database, "GNB_MAC_LCID_UL", is_on, 1);

  gd = calloc(1, sizeof(*gd)); if (!gd) abort();

  h = new_handler(database);

  g = gui_init();
  new_thread(gui_thread, g);

  build_gui(&gg, g);

  gd->g = g;
  gd->gg = &gg;
  gd->h = h;
  gd->database = database;

  /* get rnti_arg from event GNB_MAC_DL */
  int event_id = event_id_from_name(database, "GNB_MAC_DL");
  database_event_format f = get_format(database, event_id);
  gd->rnti_arg = -1;
  for (i = 0; i < f.count; i++) {
    if (!strcmp(f.name[i], "rnti")) gd->rnti_arg = i;
  }
  if (gd->rnti_arg == -1) {
    printf("fatal: event 'GNB_MAC_DL' does not have argument 'rnti'\n");
    exit(1);
  }
  if (strcmp(f.type[gd->rnti_arg], "int") != 0) {
    printf("fatal: argument 'rnti' of event 'GNB_MAC_DL' is not 'int'\n");
    exit(1);
  }

  register_handler_function(h, event_id, check_new_ue, gd);

  OBUF ebuf = {.osize = 0, .omaxsize = 0, .obuf = NULL};

  gd->socket = -1;

restart:
  clear_remote_config();
  if (gd->socket != -1) close(gd->socket);
  gd->socket = connect_to(ip, port);

  /* send the first message - activate selected traces */
  int t = 1;
  if (socket_send(gd->socket, &t, 1) == -1 ||
      socket_send(gd->socket, &number_of_events, sizeof(int)) == -1 ||
      socket_send(gd->socket, is_on, number_of_events * sizeof(int)) == -1)
    goto restart;

  while (1) {
    event e;
    e = get_event(gd->socket, &ebuf, database);
    if (e.type == -1) goto restart;
    handle_event(h, e);
  }

  return 0;
}
