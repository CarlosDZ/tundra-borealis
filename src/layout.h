#ifndef TUNDRA_LAYOUT_H
#define TUNDRA_LAYOUT_H

#define LAYOUT_MAX 64
#define LAYOUT_FREE_MAX 8

#define LAYOUT_CATALOG 6

#define LAYOUT_H 'H'
#define LAYOUT_V 'V'
#define LAYOUT_ANY 'A'

#define LAYOUT_TOP_LEFT 0
#define LAYOUT_TOP_RIGHT 1
#define LAYOUT_BOTTOM_RIGHT 2
#define LAYOUT_BOTTOM_LEFT 3

struct layout_rect {
	int x, y, w, h;
};

struct layout_slot {
	char type;
	int rank;
	struct layout_rect r;
};

struct layout {
	struct layout_slot slots[LAYOUT_MAX];
	int n;
	struct layout_rect free[LAYOUT_FREE_MAX];
	int nfree;
};

int layout_has_block(int nh, int nv);

int layout_compute(int nh, int nv, int block, int w, int h, struct layout *out);

#endif
