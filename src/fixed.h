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

struct fixed_prog {
	char name[TCONF_NAME_MAX];
	char command[TCONF_VALUE_MAX];
	int x, y, w, h;
	int level;
	unsigned int privileges;
};

struct fixed_canvas {
	struct fixed_prog p[FIXED_MAX];
	int n;
};

struct fixed_deaths {
	uint64_t at[FIXED_DEATHS];
	int n;
};

int fixed_file_canvas(const char *name);

int fixed_parse(const char *text, size_t len, struct fixed_canvas *c,
                char *err);

struct layout_rect fixed_rect(const struct fixed_prog *p, int w, int h);

int fixed_died(struct fixed_deaths *d, uint64_t now);

#endif
