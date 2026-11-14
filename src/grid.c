#include "grid.h"

void grid_init(struct grid *g) {
	g->focus.h = 1;
	g->focus.d = 1;
	g->screens = GRID_ONE;
}

void grid_set_screens(struct grid *g, enum grid_screens screens) {
	g->screens = screens;
}

int grid_valid(struct grid_pos p) {
	return p.h >= 1 && p.h <= GRID_SIDE && p.d >= 1 && p.d <= GRID_SIDE;
}

int grid_same(struct grid_pos a, struct grid_pos b) {
	return a.h == b.h && a.d == b.d;
}

int grid_step(const struct grid *g, enum grid_dir dir, struct grid_pos *to) {
	struct grid_pos p = g->focus;
	int across = g->screens == GRID_EXTENDED ? 2 : 1;

	switch (dir) {
	case GRID_LEFT:
		p.h -= across;
		break;
	case GRID_RIGHT:
		p.h += across;
		break;
	case GRID_DEEPER:
		p.d++;
		break;
	case GRID_SHALLOWER:
		p.d--;
		break;
	}
	if (!grid_valid(p))
		return -1;
	*to = p;
	return 0;
}

int grid_goto(struct grid *g, struct grid_pos p) {
	if (!grid_valid(p))
		return -1;
	g->focus = p;
	return 0;
}

int grid_other_screen(struct grid *g) {
	if (g->screens != GRID_EXTENDED)
		return -1;
	g->focus.h += g->focus.h % 2 ? 1 : -1;
	return 0;
}

struct grid_pos grid_shown(const struct grid *g, int screen) {
	if (g->screens != GRID_EXTENDED)
		return g->focus;
	struct grid_pos p = g->focus;
	int odd = p.h % 2 ? p.h : p.h - 1;
	p.h = screen == GRID_SECOND ? odd : odd + 1;
	return p;
}

int grid_focused_screen(const struct grid *g) {
	if (g->screens == GRID_EXTENDED && g->focus.h % 2)
		return GRID_SECOND;
	return GRID_MAIN;
}

int grid_is_shown(const struct grid *g, struct grid_pos p) {
	if (grid_same(p, grid_shown(g, GRID_MAIN)))
		return 1;
	return g->screens == GRID_EXTENDED &&
	       grid_same(p, grid_shown(g, GRID_SECOND));
}
