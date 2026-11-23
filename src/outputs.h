#ifndef TUNDRA_OUTPUTS_H
#define TUNDRA_OUTPUTS_H

#include <wlr/types/wlr_output.h>
#include <wlr/util/box.h>

struct server;

void outputs_init(struct server *s);
void outputs_finish(struct server *s);

struct wlr_output *outputs_main(struct server *s);
struct wlr_output *outputs_second(struct server *s);

void outputs_apply(struct server *s);

void outputs_canvas_box(struct server *s, int index, struct wlr_box *box);

int outputs_canvas_top(struct server *s, int index);

void outputs_recolor(struct server *s);

struct header *outputs_header(struct server *s, struct wlr_output *o);

int outputs_screen_at(struct server *s, double x, double y);

#endif
