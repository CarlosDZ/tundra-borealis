#ifndef TUNDRA_OUTPUTS_H
#define TUNDRA_OUTPUTS_H

#include <wlr/types/wlr_output.h>
#include <wlr/util/box.h>

struct server;

void outputs_init(struct server *s);
void outputs_finish(struct server *s);

/* The laptop panel if there is one, else the first screen that appeared; the
 * second is the next one. A third is left out. */
struct wlr_output *outputs_main(struct server *s);
struct wlr_output *outputs_second(struct server *s);

/* Lays the screens out for the mode of s and arranges every canvas. */
void outputs_apply(struct server *s);

/* Where canvas index starts in the layout, and the size of its screen. */
void outputs_canvas_box(struct server *s, int index, struct wlr_box *box);

/* GRID_MAIN or GRID_SECOND for the screen under a point, -1 for none. */
int outputs_screen_at(struct server *s, double x, double y);

#endif
