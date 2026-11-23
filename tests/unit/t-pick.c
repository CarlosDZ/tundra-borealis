#include "pick.c"
#include "test.h"

int main(void) {
	static const struct pick_size empty[] = {{32, 32}, {48, 24}};
	static const struct pick_size free_[] = {{16, 16}, {12, 24}, {24, 12}};
	static const struct pick_size twins[] = {{10, 10}, {10, 10}};
	static const struct pick_size near[] = {{10, 10}, {9, 9}, {8, 8}};
	int s = 0, ok = 1;

	CHECK(pick_scale(12, 24, 100, 50) == 2 && pick_scale(10, 10, 5, 50) == 0 &&
	          pick_scale(0, 10, 50, 50) == 0,
	      "the largest whole scale that fits");
	for (unsigned int r = 0; r < 8; r++)
		ok &= pick(empty, 2, 1264, 682, r, &s) == 1 && s == 26;
	CHECK(ok, "a wide hole takes the wide drawing, whatever the draw");
	CHECK(pick(free_, 3, 300, 600, 7, &s) == 1 && s == 25 &&
	          pick(free_, 3, 600, 300, 7, &s) == 2 && s == 25 &&
	          pick(free_, 3, 400, 400, 7, &s) == 0 && s == 25,
	      "a tall one the tall, a wide one the wide, a square one the square");
	CHECK(pick(twins, 2, 10, 10, 0, &s) == 0 &&
	          pick(twins, 2, 10, 10, 1, &s) == 1 &&
	          pick(twins, 2, 10, 10, 2, &s) == 0,
	      "the draw chooses among those that fit as well");
	CHECK(
	    pick(near, 3, 10, 10, 1, &s) == 1 && pick(near, 3, 10, 10, 2, &s) == 0,
	    "one that covers 4/5 of the best goes in the draw, and less does not");
	CHECK(pick(empty, 2, 20, 20, 0, &s) == -1 &&
	          pick(empty, 0, 99, 99, 0, &s) == -1,
	      "none if none fits, or there is none");
	return test_summary();
}
