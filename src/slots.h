#ifndef TUNDRA_SLOTS_H
#define TUNDRA_SLOTS_H

#include "layout.h"

struct slots_win {
	unsigned int id;
	char type;
};

struct slots {
	struct slots_win w[LAYOUT_MAX];
	int n;
	int block;
};

struct slots_place {
	unsigned int id;
	struct layout_rect r;
};

void slots_init(struct slots *s);

int slots_add(struct slots *s, unsigned int id, char type);
int slots_remove(struct slots *s, unsigned int id);

int slots_rotate(struct slots *s, unsigned int id);

int slots_move(struct slots *s, unsigned int id);

int slots_place(const struct slots *s, int w, int h, struct slots_place *out,
                struct layout *l);

unsigned int slots_next(const struct slots_place *p, int n, unsigned int id);

#endif
