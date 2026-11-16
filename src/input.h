#ifndef TUNDRA_INPUT_H
#define TUNDRA_INPUT_H

#include <wlr/types/wlr_input_device.h>

#include "server.h"

int input_init(struct server *s);
void input_finish(struct server *s);

void input_add_device(struct server *s, struct wlr_input_device *dev);

void input_focus(struct server *s, int warp);

struct window *input_window_at(struct server *s, double lx, double ly,
                               struct wlr_surface **surface, double *sx,
                               double *sy);

#endif
