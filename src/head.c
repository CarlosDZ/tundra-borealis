#include <stdio.h>

#include "head.h"

#define HEAD_DIV 45
#define HEAD_LINE_MAX 256

int head_target(int screen_h) {
	int line = screen_h / HEAD_DIV;

	return line < 1 ? 1 : line > HEAD_LINE_MAX ? HEAD_LINE_MAX : line;
}

int head_pad(int line) { return (line + 3) / 6; }

int head_height(int line) { return line + 2 * head_pad(line); }

void head_place(struct grid_pos p, char out[HEAD_TEXT]) {
	snprintf(out, HEAD_TEXT, "H%d P%d", p.h, p.d);
}

void head_clock(const struct tm *tm, char out[HEAD_TEXT]) {
	if (!strftime(out, HEAD_TEXT, "%Y-%m-%d  %H:%M", tm))
		out[0] = '\0';
}

time_t head_next_minute(time_t now) { return (now / 60 + 1) * 60; }

void head_layout(int w, int line, int place_w, int clock_w,
                 struct head_layout *l) {
	int margin = line / 2, after = margin + place_w + margin;

	l->place_x = margin;
	l->clock_x = (w - clock_w) / 2;
	if (l->clock_x < after)
		l->clock_x = after;
	l->y = head_pad(line);
}
