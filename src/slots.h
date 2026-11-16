#ifndef TUNDRA_SLOTS_H
#define TUNDRA_SLOTS_H

#include "layout.h"

/* The windows of one canvas. Their order in w is the opening order, which
 * gives each type its ranks: rotations and moves rearrange it. */
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

/* id is never 0. 0, or -1 if the canvas is full or id is already on it. */
int slots_add(struct slots *s, unsigned int id, char type);
int slots_remove(struct slots *s, unsigned int id);

int slots_rotate(struct slots *s, unsigned int id);

/* Super + Shift + Tab: one place clockwise among the windows of its type, the
 * V as a carousel to the right; in 3H + 2V and 3H + 3V the V move as a block
 * through the clock. 0, or -1 if id is unknown or alone in its type. */
int slots_move(struct slots *s, unsigned int id);

/* Where each window goes on a w x h canvas, in clockwise order; l gets the
 * layout, free rectangles included: an empty canvas is one free rectangle.
 * The number placed, or -1. */
int slots_place(const struct slots *s, int w, int h, struct slots_place *out,
                struct layout *l);

/* The window after id in a placement, wrapping around; 0 if id is not in it. */
unsigned int slots_next(const struct slots_place *p, int n, unsigned int id);

#endif
