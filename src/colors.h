#ifndef TUNDRA_COLORS_H
#define TUNDRA_COLORS_H

#include <stdint.h>

struct tdraw_theme;

struct colors {
	uint32_t background, surface, text, dim, border, focus, accent, alert, warn,
	    ok, fallen;
};

void colors_load(struct colors *c, const struct tdraw_theme *t);

void colors_rgba(uint32_t argb, float out[4]);

#endif
