#include "desk.c"
#include "grid.c"
#include "layout.c"
#include "slots.c"
#include "test.h"

static struct desk d;

static struct grid_pos at(int h, int dp) {
	struct grid_pos p = {h, dp};
	return p;
}

static int on(unsigned int id, int h, int dp) {
	return desk_find(&d, id) == desk_index(at(h, dp));
}

int main(void) {
	CHECK(desk_index(at(1, 1)) == 0 && desk_index(at(4, 4)) == 15 &&
	          desk_index(at(2, 3)) == 6,
	      "canvas indexes run 11 to 44");
	CHECK(grid_same(desk_pos(6), at(2, 3)), "and back to coordinates");

	desk_init(&d, 1920, 1048);
	CHECK(desk_focused(&d) == 0, "an empty desk focuses nothing");
	CHECK(desk_open(&d, 1, 'H') == 0 && desk_focused(&d) == 1,
	      "a window opens on the focused canvas and takes the focus");
	CHECK(desk_open(&d, 1, 'H') == -1, "the same id cannot open twice");
	desk_open(&d, 2, 'V');
	desk_open(&d, 3, 'H');
	CHECK(desk_focused(&d) == 3, "the latest window has the focus");

	CHECK(desk_focus_next(&d) == 0 && desk_focused(&d) == 1,
	      "Super + Tab after the last clockwise goes round to the first");
	desk_focus_next(&d);
	CHECK(desk_focused(&d) == 2, "and on to the V");

	CHECK(desk_close(&d, 2) == 0 && desk_focused(&d) == 3,
	      "closing the focused window gives the focus to the next one");
	CHECK(desk_close(&d, 9) == -1, "closing an unknown id");
	desk_close(&d, 1);
	desk_close(&d, 3);
	CHECK(desk_focused(&d) == 0, "closing the last window leaves no focus");
	CHECK(desk_focus_next(&d) == -1, "Super + Tab on an empty canvas");

	desk_open(&d, 4, 'H');
	desk_open(&d, 5, 'H');
	CHECK(desk_step(&d, GRID_RIGHT) == 0 && grid_same(d.grid.focus, at(2, 1)),
	      "the view steps right");
	CHECK(desk_focused(&d) == 0, "the new canvas is empty");
	CHECK(desk_send(&d, at(3, 1)) == -1,
	      "nothing to send from an empty canvas");
	desk_goto(&d, at(1, 1));
	CHECK(desk_focused(&d) == 5, "back on 11 its focus is still there");

	CHECK(desk_send(&d, at(2, 3)) == 0, "send the focused window to 23");
	CHECK(on(5, 2, 3) && grid_same(d.grid.focus, at(2, 3)) &&
	          desk_focused(&d) == 5,
	      "the window and the view are on 23, the window focused");
	CHECK(d.focus[0] == 4, "11 focuses the window left behind");
	CHECK(desk_send(&d, at(2, 3)) == -1, "sending onto its own canvas");
	CHECK(desk_send(&d, at(5, 1)) == -1, "sending out of the grid");

	CHECK(desk_send_step(&d, GRID_SHALLOWER) == 0 && on(5, 2, 2) &&
	          grid_same(d.grid.focus, at(2, 2)),
	      "Super + Shift + k sends it one depth up, the view with it");
	desk_goto(&d, at(2, 1));
	CHECK(desk_send_step(&d, GRID_LEFT) == -1, "nothing focused on 21");
	desk_goto(&d, at(1, 1));
	CHECK(desk_send_step(&d, GRID_LEFT) == -1 && on(4, 1, 1),
	      "sending past the edge stays put");

	desk_init(&d, 1920, 1048);
	desk_goto(&d, at(4, 4));
	desk_open(&d, 1, 'H');
	desk_open(&d, 2, 'H');
	desk_focus_next(&d);
	CHECK(desk_focused(&d) == 1 && desk_move(&d) == 15,
	      "Super + Shift + Tab moves the focused window on 44");
	CHECK(desk_rotate(&d) == 15 && d.canvas[15].w[1].type == 'V',
	      "Super + R rotates the focused window");
	desk_goto(&d, at(1, 1));
	CHECK(desk_move(&d) == -1 && desk_rotate(&d) == -1,
	      "neither with nothing focused");

	desk_init(&d, 1920, 1048);
	desk_goto(&d, at(1, 2));
	for (unsigned int i = 1; i <= LAYOUT_MAX; i++)
		desk_open(&d, i, 'H');
	desk_goto(&d, at(1, 1));
	desk_open(&d, 100, 'V');
	CHECK(desk_send(&d, at(1, 2)) == -1 && on(100, 1, 1),
	      "sending onto a full canvas keeps the window where it was");
	desk_goto(&d, at(1, 2));
	CHECK(desk_open(&d, 200, 'H') == -1, "a full canvas refuses a new window");

	desk_init(&d, 1920, 1048);
	desk_open(&d, 1, 'H');
	desk_open(&d, 2, 'H');
	CHECK(desk_focus_window(&d, 1) == 0 && desk_focused(&d) == 1,
	      "the pointer over a window focuses it");
	grid_set_screens(&d.grid, GRID_EXTENDED);
	desk_goto(&d, at(2, 1));
	desk_open(&d, 3, 'H');
	CHECK(desk_focus_window(&d, 1) == 0 && grid_same(d.grid.focus, at(1, 1)),
	      "over a window on the other screen, that screen takes the keyboard");
	CHECK(desk_focus_window(&d, 9) == -1, "an unknown id");

	desk_init(&d, 1920, 1048);
	desk_open(&d, 1, 'H');
	desk_open(&d, 2, 'H');
	desk_open(&d, 3, 'V');
	CHECK(desk_fullscreen(&d, 2, 1) == 0 && d.fullscreen[0] == 2 &&
	          desk_focused(&d) == 2,
	      "a window goes fullscreen and takes the focus of its canvas");
	CHECK(desk_fullscreen(&d, 2, 1) == -1, "asking again changes nothing");
	CHECK(desk_focus_next(&d) == -1 && desk_move(&d) == -1 &&
	          desk_rotate(&d) == -1,
	      "Super + Tab, moving and rotating do nothing in fullscreen");
	CHECK(desk_focus_window(&d, 1) == -1 && desk_focused(&d) == 2,
	      "the pointer cannot focus a window hidden behind it");
	CHECK(desk_fullscreen(&d, 1, 1) == 0 && d.fullscreen[0] == 1,
	      "another window going fullscreen replaces it");
	CHECK(desk_fullscreen(&d, 2, 0) == -1, "a window not in fullscreen leaves "
	                                       "nothing");
	CHECK(desk_fullscreen(&d, 1, 0) == 0 && d.fullscreen[0] == 0,
	      "leaving fullscreen");
	desk_fullscreen(&d, 1, 1);
	desk_open(&d, 4, 'H');
	CHECK(d.fullscreen[0] == 0 && desk_focused(&d) == 4,
	      "a new window on the canvas ends the fullscreen");
	desk_fullscreen(&d, 4, 1);
	desk_close(&d, 4);
	CHECK(d.fullscreen[0] == 0, "closing the fullscreen window ends it");
	desk_fullscreen(&d, 3, 1);
	CHECK(desk_send(&d, at(2, 1)) == 0 && d.fullscreen[0] == 0 &&
	          d.fullscreen[desk_index(at(2, 1))] == 0,
	      "sending it to another canvas ends it on both");
	CHECK(desk_fullscreen(&d, 9, 1) == -1, "an unknown id");

	desk_init(&d, 1920, 1048);
	desk_open(&d, 1, 'H');
	desk_resize(&d, 1366, 736);
	struct slots_place p[LAYOUT_MAX];
	struct layout l;
	desk_goto(&d, at(1, 1));
	CHECK(desk_place(&d, 0, p, &l) == 1 && p[0].r.w == 1366 && p[0].r.h == 736,
	      "places follow the canvas size");

	return test_summary();
}
