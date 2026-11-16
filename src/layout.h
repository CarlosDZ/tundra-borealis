#ifndef TUNDRA_LAYOUT_H
#define TUNDRA_LAYOUT_H

#define LAYOUT_MAX 64
#define LAYOUT_FREE_MAX 8

/* Past LAYOUT_CATALOG windows the canvas is a board that ignores H and V. */
#define LAYOUT_CATALOG 6

#define LAYOUT_H 'H'
#define LAYOUT_V 'V'
#define LAYOUT_ANY 'A'

/* Clock cells of a 2x2 grid, the order windows move in. */
#define LAYOUT_TOP_LEFT 0
#define LAYOUT_TOP_RIGHT 1
#define LAYOUT_BOTTOM_RIGHT 2
#define LAYOUT_BOTTOM_LEFT 3

struct layout_rect {
	int x, y, w, h;
};

/* The rank-th window of its type, counted in opening order, goes here. A board
 * slot has type LAYOUT_ANY and counts every window. */
struct layout_slot {
	char type;
	int rank;
	struct layout_rect r;
};

/* Slots in clockwise order from the top left: the order of Super + Tab. */
struct layout {
	struct layout_slot slots[LAYOUT_MAX];
	int n;
	struct layout_rect free[LAYOUT_FREE_MAX];
	int nfree;
};

/* 3H + 2V and 3H + 3V: the V share one clock cell and move as a block. */
int layout_has_block(int nh, int nv);

/* The layout of nh horizontal and nv vertical windows on a w x h canvas. block
 * is the clock cell of the V block, used only when layout_has_block. 0, or -1
 * with no windows, more than LAYOUT_MAX or a canvas too small to split. */
int layout_compute(int nh, int nv, int block, int w, int h, struct layout *out);

#endif
