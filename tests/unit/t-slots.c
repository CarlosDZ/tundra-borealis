#include "layout.c"
#include "slots.c"
#include "test.h"

#define W 1920
#define H 1048

static struct slots s;
static struct slots_place p[LAYOUT_MAX];
static struct layout l;
static int n;

static void place(void) { n = slots_place(&s, W, H, p, &l); }

static const struct layout_rect *where(unsigned int id) {
	for (int i = 0; i < n; i++)
		if (p[i].id == id)
			return &p[i].r;
	return NULL;
}

static int at(unsigned int id, int x, int y) {
	const struct layout_rect *r = where(id);
	return r && r->x == x && r->y == y;
}

static const char *order(void) {
	static char buf[LAYOUT_MAX + 1];
	for (int i = 0; i < n; i++)
		buf[i] = (char)('0' + p[i].id % 10);
	buf[n] = '\0';
	return buf;
}

static void fill(const char *types) {
	slots_init(&s);
	for (unsigned int i = 0; types[i]; i++)
		slots_add(&s, i + 1, types[i]);
	place();
}

int main(void) {
	slots_init(&s);
	place();
	CHECK(n == 0 && l.nfree == 1 && l.free[0].w == W && l.free[0].h == H,
	      "an empty canvas is one free rectangle");

	CHECK(slots_add(&s, 1, 'H') == 0, "add a window");
	CHECK(slots_add(&s, 1, 'V') == -1, "an id already there is refused");
	CHECK(slots_add(&s, 0, 'H') == -1, "id 0 is refused");
	CHECK(slots_add(&s, 2, 'X') == -1, "a type that is neither H nor V");
	CHECK(slots_remove(&s, 7) == -1, "removing an unknown id");

	fill("HVH");
	CHECK(at(1, 0, 0) && at(2, 960, 0) && at(3, 0, 524),
	      "2H + 1V: the H take their places in opening order");
	CHECK(strcmp(order(), "123") == 0, "clockwise: H, the V, H");
	CHECK(slots_next(p, n, 1) == 2 && slots_next(p, n, 3) == 1,
	      "Super + Tab follows the clock and wraps");
	CHECK(slots_next(p, n, 9) == 0, "the next of an unknown id is 0");

	fill("HHHH");
	slots_move(&s, 1);
	place();
	CHECK(at(1, 960, 0) && at(2, 0, 0), "4H: moving the first H swaps it with "
	                                    "the next clockwise");
	slots_move(&s, 4);
	place();
	CHECK(at(4, 0, 0) && at(2, 0, 524),
	      "4H: the last H clockwise goes to the first place");

	fill("VVVV");
	slots_move(&s, 2);
	place();
	CHECK(strcmp(order(), "1324") == 0, "4V: a V moves one to the right");
	slots_move(&s, 4);
	place();
	CHECK(strcmp(order(), "4321") == 0,
	      "4V: the last V goes round to the first place");

	fill("VHVH");
	slots_move(&s, 2);
	place();
	CHECK(at(2, 480, 524) && at(4, 480, 0) && at(1, 0, 0) && at(3, 1440, 0),
	      "2H + 2V: an H moves among the H only");
	CHECK(slots_move(&s, 1) == 0 && (place(), at(1, 1440, 0)),
	      "2H + 2V: a V moves among the V only");
	fill("HV");
	CHECK(slots_move(&s, 1) == -1, "a window alone in its type does not move");
	CHECK(slots_move(&s, 9) == -1, "an unknown id does not move");

	fill("HHHVV");
	CHECK(s.block == LAYOUT_BOTTOM_RIGHT && at(4, 960, 524) && at(5, 1440, 524),
	      "3H + 2V: the block starts bottom right");
	CHECK(strcmp(order(), "12453") == 0, "3H + 2V: clockwise H, H, V, V, H");
	slots_move(&s, 2);
	place();
	CHECK(s.block == LAYOUT_TOP_RIGHT && at(2, 960, 524) && at(4, 960, 0),
	      "3H + 2V: moving H2 takes the block to its cell");
	slots_move(&s, 4);
	place();
	CHECK(s.block == LAYOUT_BOTTOM_RIGHT && at(2, 960, 0) && at(4, 960, 524),
	      "3H + 2V: moving a V moves the whole block clockwise");
	CHECK(at(4, 960, 524) && at(5, 1440, 524),
	      "3H + 2V: V1 and V2 keep their order in the block");
	slots_move(&s, 3);
	place();
	CHECK(at(3, 0, 0) && at(1, 0, 524),
	      "3H + 2V: the H before the top left swaps with it");
	slots_move(&s, 5);
	slots_move(&s, 5);
	place();
	CHECK(s.block == LAYOUT_TOP_LEFT && at(4, 0, 0) && at(2, 960, 0) &&
	          at(1, 960, 524) && at(3, 0, 524),
	      "3H + 2V: the block goes round from bottom left to top left");

	slots_add(&s, 6, 'V');
	place();
	CHECK(s.block == LAYOUT_TOP_LEFT && at(6, 640, 0),
	      "3H + 3V: a V added to the block keeps its cell");
	slots_remove(&s, 6);
	slots_remove(&s, 5);
	place();
	CHECK(l.n == 4 && find(&s, 4) >= 0, "3H + 1V: no block any more");
	slots_add(&s, 7, 'V');
	CHECK(s.block == LAYOUT_BOTTOM_RIGHT, "a block that appears again starts "
	                                      "bottom right");

	fill("HHV");
	slots_rotate(&s, 3);
	place();
	CHECK(l.n == 3 && l.nfree == 1 && at(3, 0, 524),
	      "rotating the V makes 3H, in its opening place");
	slots_rotate(&s, 3);
	place();
	CHECK(at(3, 960, 0), "rotating it back is 2H + 1V again");
	CHECK(slots_rotate(&s, 9) == -1, "rotating an unknown id");

	fill("HVHVHVH");
	CHECK(n == 7 && strcmp(order(), "1234567") == 0,
	      "7 windows: a board in opening order");
	slots_move(&s, 7);
	place();
	CHECK(strcmp(order(), "7234561") == 0,
	      "on a board the last window swaps with the first");

	slots_init(&s);
	for (unsigned int i = 1; i <= LAYOUT_MAX; i++)
		slots_add(&s, i, 'H');
	CHECK(slots_add(&s, 999, 'H') == -1, "a full canvas refuses one more");

	return test_summary();
}
