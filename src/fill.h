#ifndef TUNDRA_FILL_H
#define TUNDRA_FILL_H

#include "layout.h"

struct server;

enum fill_kind { FILL_EMPTY, FILL_FREE, FILL_FALLEN, FILL_KINDS };

int fill_init(struct server *s);
void fill_finish(struct server *s);

void fill_holes(struct server *s, int index, const struct layout_rect *r, int n,
                enum fill_kind kind);

void fill_fallen(struct server *s, int index, int prog, struct layout_rect r,
                 int fallen);

void fill_view(struct server *s);

void fill_theme(struct server *s);

#ifdef TUNDRA_BOREALIS_TEST
struct fill_info {
	enum fill_kind kind;
	const char *name;
	struct layout_rect r;
	int w, h, scale, in_view;
	unsigned int frames;
};

int fill_info(struct server *s, int index, int i, struct fill_info *out);
#endif

#endif
