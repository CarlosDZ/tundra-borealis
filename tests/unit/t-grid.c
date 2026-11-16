#include "grid.c"
#include "test.h"

static struct grid g;

static struct grid_pos at(int h, int d) {
	struct grid_pos p = {h, d};
	return p;
}

static int lands(enum grid_dir dir, int h, int d) {
	struct grid_pos to;
	return grid_step(&g, dir, &to) == 0 && grid_same(to, at(h, d));
}

static int edge(enum grid_dir dir) {
	struct grid_pos to = at(9, 9);
	return grid_step(&g, dir, &to) == -1 && grid_same(to, at(9, 9));
}

static int shows(int screen, int h, int d) {
	return grid_same(grid_shown(&g, screen), at(h, d));
}

int main(void) {
	grid_init(&g);
	CHECK(grid_same(g.focus, at(1, 1)), "starts at 11");
	CHECK(lands(GRID_RIGHT, 2, 1), "right from 11 is 21");
	CHECK(lands(GRID_DEEPER, 1, 2), "deeper from 11 is 12");
	CHECK(edge(GRID_LEFT), "left from 11 is an edge");
	CHECK(edge(GRID_SHALLOWER), "shallower from 11 is an edge");

	grid_goto(&g, at(4, 1));
	CHECK(edge(GRID_RIGHT), "right from 41 does not move");
	CHECK(lands(GRID_LEFT, 3, 1), "left from 41 is 31");

	grid_goto(&g, at(2, 4));
	CHECK(edge(GRID_DEEPER), "depth does not wrap past 4");
	CHECK(lands(GRID_SHALLOWER, 2, 3), "shallower from 24 is 23");

	CHECK(grid_goto(&g, at(2, 3)) == 0 && grid_same(g.focus, at(2, 3)),
	      "goto 23");
	CHECK(grid_goto(&g, at(5, 1)) == -1 && grid_same(g.focus, at(2, 3)),
	      "goto out of the grid keeps the focus");
	CHECK(grid_goto(&g, at(1, 0)) == -1, "depth 0 is out of the grid");

	CHECK(shows(GRID_MAIN, 2, 3) && shows(GRID_SECOND, 2, 3),
	      "one screen: both views are the focus");
	CHECK(grid_focused_screen(&g) == GRID_MAIN, "one screen: the main one");
	CHECK(grid_other_screen(&g) == -1 && grid_same(g.focus, at(2, 3)),
	      "one screen: the other screen does nothing");

	grid_set_screens(&g, GRID_MIRROR);
	CHECK(lands(GRID_RIGHT, 3, 3), "mirrored: right is one step");
	CHECK(grid_other_screen(&g) == -1, "mirrored: no other screen");

	grid_set_screens(&g, GRID_EXTENDED);
	grid_goto(&g, at(1, 3));
	CHECK(shows(GRID_SECOND, 1, 3) && shows(GRID_MAIN, 2, 3),
	      "extended at 13: 1 on the second, 2 on the main");
	CHECK(grid_focused_screen(&g) == GRID_SECOND, "13 has the second screen");
	grid_goto(&g, at(2, 3));
	CHECK(shows(GRID_SECOND, 1, 3) && shows(GRID_MAIN, 2, 3),
	      "23 shows the same as 13");
	CHECK(grid_focused_screen(&g) == GRID_MAIN, "23 has the main screen");

	CHECK(lands(GRID_RIGHT, 4, 3), "extended: right from 23 jumps to 43");
	CHECK(edge(GRID_LEFT), "extended: left from the first pair is an edge");
	grid_goto(&g, at(1, 3));
	CHECK(lands(GRID_RIGHT, 3, 3), "extended: right from 13 keeps the screen");
	grid_goto(&g, at(3, 3));
	CHECK(edge(GRID_RIGHT), "extended: right from the last pair is an edge");

	grid_goto(&g, at(4, 2));
	CHECK(shows(GRID_SECOND, 3, 2) && shows(GRID_MAIN, 4, 2),
	      "the depth is the same on both screens");
	CHECK(lands(GRID_DEEPER, 4, 3), "extended: deeper keeps the horizontal");

	CHECK(grid_other_screen(&g) == 0 && grid_same(g.focus, at(3, 2)),
	      "other screen from 42 is 32");
	CHECK(grid_other_screen(&g) == 0 && grid_same(g.focus, at(4, 2)),
	      "and back to 42");

	CHECK(grid_is_shown(&g, at(3, 2)) && grid_is_shown(&g, at(4, 2)),
	      "both canvases of the pair are shown");
	CHECK(!grid_is_shown(&g, at(4, 3)), "another depth is not shown");
	grid_set_screens(&g, GRID_ONE);
	CHECK(!grid_is_shown(&g, at(3, 2)), "one screen: only the focus is shown");

	enum grid_dir dir = GRID_LEFT;
	CHECK(grid_swipe(0, -200, &dir) == 0 && dir == GRID_DEEPER,
	      "fingers up go to the next depth");
	CHECK(grid_swipe(0, 200, &dir) == 0 && dir == GRID_SHALLOWER,
	      "fingers down come back a depth");
	CHECK(grid_swipe(150, 20, &dir) == 0 && dir == GRID_RIGHT,
	      "fingers right go right");
	CHECK(grid_swipe(-150, -90, &dir) == 0 && dir == GRID_LEFT,
	      "the longer axis decides");
	dir = GRID_RIGHT;
	CHECK(grid_swipe(20, -30, &dir) == -1 && dir == GRID_RIGHT,
	      "a short swipe does nothing");

	return test_summary();
}
