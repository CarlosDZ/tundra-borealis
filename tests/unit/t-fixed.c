#include "fixed.c"
#include "test.h"

static struct fixed_canvas c;
static char err[FIXED_ERR_MAX];

static int parse(const char *text) {
	err[0] = '\0';
	return fixed_parse(text, strlen(text), &c, err);
}

static int fails(const char *text, const char *why) {
	return parse(text) == -1 && strstr(err, why) && c.n == 0;
}

#define PROG(name, place, size, extra)                                         \
	name " {\ncommand = \"frost\"\nplace = " place "\nsize = " size "\n" extra \
	     "}\n"
#define NONE "privileges = none\n"

int main(void) {
	CHECK(fixed_file_canvas("canvas-23") == 6, "canvas-23 is canvas 23");
	CHECK(fixed_file_canvas("canvas-11") == 0 &&
	          fixed_file_canvas("canvas-44") == 15,
	      "from 11 to 44");
	CHECK(fixed_file_canvas("canvas-50") == -1 &&
	          fixed_file_canvas("canvas-2") == -1 &&
	          fixed_file_canvas("canvas-230") == -1 &&
	          fixed_file_canvas("windows") == -1,
	      "any other name is not a canvas");

	CHECK(parse(PROG("art", "0 0", "100 100", NONE)
	                PROG("top", "50 0", "50 50", "level = 1\n" NONE)) == 0 &&
	          c.n == 2,
	      "two programs, the upper one over the lower");
	CHECK(strcmp(c.p[0].name, "art") == 0 && c.p[0].level == 0 &&
	          strcmp(c.p[1].name, "top") == 0 && c.p[1].level == 1,
	      "level 0 by default, kept from lowest to highest");
	CHECK(strcmp(c.p[0].command, "frost") == 0 && c.p[0].privileges == 0,
	      "command and no privileges");

	CHECK(parse(PROG("high", "0 0", "50 50", "level = 9\n" NONE)
	                PROG("low", "50 0", "50 50", NONE)) == 0 &&
	          strcmp(c.p[0].name, "low") == 0,
	      "the order is by level, not by the file");
	CHECK(parse(PROG("b", "0 0", "50 50", NONE)
	                PROG("a", "50 0", "50 50", NONE)) == 0 &&
	          strcmp(c.p[0].name, "b") == 0,
	      "on one level, the order of the file");

	CHECK(parse(PROG("p", "0 0", "50 50",
	                 "privileges = capture, virtual-input\n")) == 0 &&
	          c.p[0].privileges == (FIXED_CAPTURE | FIXED_VIRTUAL_INPUT),
	      "a list of privileges");
	CHECK(parse(PROG("p", "0 0", "50 50",
	                 "privileges = overlay,capture,virtual-input,window-"
	                 "control,lock\n")) == 0 &&
	          c.p[0].privileges == 31,
	      "all five privileges");

	CHECK(fails(PROG("a", "0 0", "60 100", NONE)
	                PROG("b", "50 0", "50 100", NONE),
	            "a and b overlap on level 0"),
	      "two programs of one level cannot overlap");
	CHECK(fails(PROG("a", "60 0", "50 50", NONE), "past the canvas"),
	      "a program past the right edge");
	CHECK(fails(PROG("a", "0 60", "50 50", NONE), "past the canvas"),
	      "a program past the bottom");
	CHECK(fails(PROG("a", "0 0", "0 50", NONE), "size is"), "a size of 0");
	CHECK(fails(PROG("a", "100 0", "1 1", NONE), "place is"),
	      "a place of 100 leaves no room");
	CHECK(fails(PROG("a", "0", "50 50", NONE), "place is"), "place needs two");
	CHECK(fails(PROG("a", "0 0 0", "50 50", NONE), "place is"),
	      "place takes two, no more");
	CHECK(fails(PROG("a", "-1 0", "50 50", NONE), "place is"),
	      "no negative numbers");
	CHECK(fails(PROG("a", "0 0", "50 50", "level = 10\n" NONE), "level is"),
	      "level past 9");
	CHECK(fails(PROG("a", "0 0", "50 50", ""), "needs command"),
	      "privileges cannot be left out");
	CHECK(fails("a {\nplace = 0 0\nsize = 1 1\nprivileges = none\n}\n",
	            "needs command"),
	      "neither can the command");
	CHECK(fails(PROG("a", "0 0", "50 50", "privileges = root\n"), "privilege"),
	      "an unknown privilege");
	CHECK(fails(PROG("a", "0 0", "50 50", "privileges = capture, capture\n"),
	            "privilege"),
	      "a privilege twice");
	CHECK(fails(PROG("a", "0 0", "50 50", "privileges = none, capture\n"),
	            "privilege"),
	      "none with others");
	CHECK(fails(PROG("a", "0 0", "50 50", NONE "colour = red\n"),
	            "unknown key colour"),
	      "an unknown key");
	CHECK(fails(PROG("a", "0 0", "50 50", NONE NONE), "privileges twice"),
	      "a key twice");
	CHECK(fails(PROG("a", "0 0", "50 50", NONE)
	                PROG("a", "50 0", "50 50", NONE),
	            "a twice"),
	      "a program name twice");
	CHECK(fails("command = frost\n", "no programs"), "no blocks at all");
	CHECK(fails("command = frost\n" PROG("a", "0 0", "50 50", NONE),
	            "outside a program"),
	      "a key outside a program");
	CHECK(fails("a {\ncommand = \"\"\nplace = 0 0\nsize = 1 1\n" NONE "}\n",
	            "empty command"),
	      "an empty command");
	CHECK(fails("a {\n", "line"), "a grammar error says its line");

	parse(PROG("a", "25 50", "50 50", NONE));
	struct layout_rect r = fixed_rect(&c.p[0], 1272, 712);
	CHECK(r.x == 318 && r.y == 356 && r.w == 636 && r.h == 356,
	      "percents become a rectangle of the canvas");
	parse(PROG("a", "0 0", "33 100", NONE) PROG("b", "33 0", "67 100", NONE));
	struct layout_rect ra = fixed_rect(&c.p[0], 1000, 500);
	struct layout_rect rb = fixed_rect(&c.p[1], 1000, 500);
	CHECK(ra.x + ra.w == rb.x && rb.x + rb.w == 1000,
	      "neighbours touch after rounding");

	CHECK(fixed_privilege_of("zwlr_screencopy_manager_v1") == FIXED_CAPTURE &&
	          fixed_privilege_of("ext_image_copy_capture_manager_v1") ==
	              FIXED_CAPTURE,
	      "capturing the screen needs capture");
	CHECK(fixed_privilege_of("zwp_virtual_keyboard_manager_v1") ==
	              FIXED_VIRTUAL_INPUT &&
	          fixed_privilege_of("zwlr_virtual_pointer_manager_v1") ==
	              FIXED_VIRTUAL_INPUT,
	      "a virtual keyboard or pointer needs virtual-input");
	CHECK(fixed_privilege_of("zwlr_layer_shell_v1") == FIXED_OVERLAY,
	      "drawing over the rest needs overlay");
	CHECK(fixed_privilege_of("zwlr_foreign_toplevel_manager_v1") ==
	              FIXED_WINDOW_CONTROL &&
	          fixed_privilege_of("ext_foreign_toplevel_list_v1") ==
	              FIXED_WINDOW_CONTROL,
	      "seeing other windows needs window-control");
	CHECK(fixed_privilege_of("ext_session_lock_manager_v1") == FIXED_LOCK,
	      "locking the session needs lock");
	CHECK(fixed_privilege_of("wl_compositor") == 0 &&
	          fixed_privilege_of("xdg_wm_base") == 0 &&
	          fixed_privilege_of("wl_seat") == 0,
	      "the rest is for every client");

	struct fixed_deaths d = {0};
	CHECK(fixed_died(&d, 1000) == 1 && fixed_died(&d, 2000) == 1,
	      "the first two deaths restart it");
	CHECK(fixed_died(&d, 3000) == 0, "the third within 10 s covers it");
	struct fixed_deaths e = {0};
	fixed_died(&e, 0);
	fixed_died(&e, 6000);
	CHECK(fixed_died(&e, 12000) == 1,
	      "three deaths over more than 10 s do not");
	CHECK(fixed_died(&e, 13000) == 0, "but the next one close by does");

	return test_summary();
}
