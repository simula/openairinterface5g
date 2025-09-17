#ifndef _VIEW_H_
#define _VIEW_H_

#include "gui/gui.h"

/* defines the public API of views */

typedef struct view {
  void (*clear)(struct view *this);
  void (*append)(struct view *this, ...);
  void (*set)(struct view *this, char *name, ...);
} view;

enum xy_mode { XY_LOOP_MODE, XY_FORCED_MODE };

view *new_view_stdout(void);
view *new_view_textlist(int maxsize, float refresh_rate, gui *g, widget *w);
view *new_view_xy(int length, float refresh_rate, gui *g, widget *w,
    int color, enum xy_mode mode);
view *new_view_tti(float refresh_rate, gui *g, widget *w, int color,
    int ticks_per_frame);
view *new_view_scrolltti(float refresh_rate, gui *g, widget *w,
    int color, widget *throughput_label, int ticks_per_frame);
view *new_view_time(int number_of_seconds, float refresh_rate,
    gui *g, widget *w);
view *new_subview_time(view *time, int line, int color, int size);
view *new_view_ticktime(float refresh_rate, gui *g, widget *w);
view *new_subview_ticktime(view *ticktime, int line, int color, int size);
void ticktime_set_tick(view *ticktime, void *logger);
void view_tti_enable_automax(view *tti, widget *w2);

#endif /* _VIEW_H_ */
