#ifndef TUNDRA_FIXED_H
#define TUNDRA_FIXED_H

#include <stddef.h>
#include <stdint.h>
#include <tundra/conf.h>

#include "layout.h"

#define FIXED_MAX 16
#define FIXED_LEVELS 10
#define FIXED_DEATHS 3
#define FIXED_DEATH_SPAN_MS 10000
#define FIXED_ERR_MAX 160

#define FIXED_OVERLAY 1u
#define FIXED_CAPTURE 2u
#define FIXED_VIRTUAL_INPUT 4u
#define FIXED_WINDOW_CONTROL 8u
#define FIXED_LOCK 16u

/* place and size in whole percents of the canvas. */
struct fixed_prog {
	char name[TCONF_NAME_MAX];
	char command[TCONF_VALUE_MAX];
	int x, y, w, h;
	int level;
	unsigned int privileges;
};

/* Programs from the lowest level to the highest: the order they are drawn. */
struct fixed_canvas {
	struct fixed_prog p[FIXED_MAX];
	int n;
};

struct fixed_deaths {
	uint64_t at[FIXED_DEATHS];
	int n;
};

/* The canvas index of a file name "canvas-HD", or -1. */
int fixed_file_canvas(const char *name);

/* 0, or -1 with err set and c left empty. */
int fixed_parse(const char *text, size_t len, struct fixed_canvas *c,
                char *err);

struct layout_rect fixed_rect(const struct fixed_prog *p, int w, int h);

/* The privileges a client needs to see a global of that interface; 0 for one
 * every client sees. */
unsigned int fixed_privilege_of(const char *interface);

/* p died at now, in milliseconds: 1 to start it again, 0 once it died
 * FIXED_DEATHS times within FIXED_DEATH_SPAN_MS and its place is covered. */
int fixed_died(struct fixed_deaths *d, uint64_t now);

#endif
