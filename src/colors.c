#include <stdio.h>
#include <tundra/draw.h>

#include "colors.h"

static uint32_t look(const struct tdraw_theme *t, const char *name,
                     uint32_t tundra) {
	char own[48];

	snprintf(own, sizeof(own), "borealis.%s", name);
	return tdraw_theme_color(t, own, tdraw_theme_color(t, name, tundra));
}

void colors_load(struct colors *c, const struct tdraw_theme *t) {
	c->background = look(t, "background", 0xff141a1e);
	c->surface = look(t, "surface", 0xff1f2529);
	c->text = look(t, "text", 0xffccd2d6);
	c->dim = look(t, "dim", 0xff889196);
	c->border = look(t, "border", 0xff3e494f);
	c->focus = look(t, "focus", 0xff74acfe);
	c->accent = look(t, "accent", 0xff66dc5c);
	c->alert = look(t, "alert", 0xffff1712);
	c->warn = look(t, "warn", 0xffd2a408);
	c->ok = look(t, "ok", 0xff65c55b);
	c->fallen = look(t, "fallen", 0xff4d2623);
}

void colors_rgba(uint32_t argb, float out[4]) {
	float a = (float)(argb >> 24) / 255.0f;

	out[0] = (float)(argb >> 16 & 0xff) / 255.0f * a;
	out[1] = (float)(argb >> 8 & 0xff) / 255.0f * a;
	out[2] = (float)(argb & 0xff) / 255.0f * a;
	out[3] = a;
}
