#ifndef TUNDRA_HEAD_H
#define TUNDRA_HEAD_H

#include <time.h>

#include "grid.h"

#define HEAD_TEXT 32

int head_target(int screen_h);

int head_pad(int line);
int head_height(int line);

void head_place(struct grid_pos p, char out[HEAD_TEXT]);

void head_clock(const struct tm *tm, char out[HEAD_TEXT]);

time_t head_next_minute(time_t now);

struct head_layout {
	int place_x, clock_x, y;
};

void head_layout(int w, int line, int place_w, int clock_w,
                 struct head_layout *l);

#endif
