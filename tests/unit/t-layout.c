#include "layout.c"
#include "test.h"

#include <stdlib.h>

#define W1 1920
#define H1 1048
#define W2 1366
#define H2 736

static struct layout l;

static int overlap(const struct layout_rect *a, const struct layout_rect *b) {
	return a->x < b->x + b->w && b->x < a->x + a->w && a->y < b->y + b->h &&
	       b->y < a->y + a->h;
}

static int inside(const struct layout_rect *r, int w, int h) {
	return r->w > 0 && r->h > 0 && r->x >= 0 && r->y >= 0 && r->x + r->w <= w &&
	       r->y + r->h <= h;
}

static int all(struct layout_rect *out) {
	int k = 0;
	for (int i = 0; i < l.n; i++)
		out[k++] = l.slots[i].r;
	for (int i = 0; i < l.nfree; i++)
		out[k++] = l.free[i];
	return k;
}

static int tiles(int w, int h) {
	struct layout_rect r[LAYOUT_MAX + LAYOUT_FREE_MAX];
	int k = all(r);
	long area = 0;
	for (int i = 0; i < k; i++) {
		if (!inside(&r[i], w, h))
			return 0;
		area += (long)r[i].w * r[i].h;
		for (int j = i + 1; j < k; j++)
			if (overlap(&r[i], &r[j]))
				return 0;
	}
	return area == (long)w * h;
}

static int ranks(char type, int count) {
	int seen[LAYOUT_MAX] = {0};
	int got = 0;
	for (int i = 0; i < l.n; i++) {
		if (l.slots[i].type != type)
			continue;
		int r = l.slots[i].rank;
		if (r < 0 || r >= count || seen[r]++)
			return 0;
		got++;
	}
	return got == count;
}

static int ratio_kept(int w, int h) {
	for (int i = 0; i < l.n; i++) {
		const struct layout_rect *r = &l.slots[i].r;
		if (l.slots[i].type == LAYOUT_H &&
		    labs((long)r->w * h - (long)r->h * w) > 2L * (w + h))
			return 0;
	}
	return 1;
}

static const struct layout_slot *find(char type, int rank) {
	for (int i = 0; i < l.n; i++)
		if (l.slots[i].type == type && l.slots[i].rank == rank)
			return &l.slots[i];
	return NULL;
}

static int at(char type, int rank, int x, int y, int w, int h) {
	const struct layout_slot *s = find(type, rank);
	return s && s->r.x == x && s->r.y == y && s->r.w == w && s->r.h == h;
}

static int free_expected(int nh, int nv) {
	switch (nh * 10 + nv) {
	case 20:
	case 11:
	case 12:
	case 41:
	case 13:
	case 14:
	case 15:
	case 24:
	case 42:
	case 23:
	case 60:
		return 2;
	case 30:
		return 1;
	case 31:
	case 51:
	case 50:
		return 3;
	}
	return 0;
}

static void catalog_case(int nh, int nv, int block, int w, int h) {
	char d[96];
	int r = layout_compute(nh, nv, block, w, h, &l);
	snprintf(d, sizeof(d), "%dH + %dV (block %d) on %dx%d", nh, nv, block, w,
	         h);
	CHECK(r == 0 && l.n == nh + nv && tiles(w, h) && ranks(LAYOUT_H, nh) &&
	          ranks(LAYOUT_V, nv) && ratio_kept(w, h) &&
	          l.nfree == free_expected(nh, nv),
	      d);
}

static void board_case(int n, int w, int h) {
	char d[64];
	int r = layout_compute(n - 2, 2, 0, w, h, &l);
	snprintf(d, sizeof(d), "%d windows are a board on %dx%d", n, w, h);
	int any = 1;
	for (int i = 0; i < l.n; i++)
		any &= l.slots[i].type == LAYOUT_ANY;
	CHECK(r == 0 && l.n == n && any && tiles(w, h) && ranks(LAYOUT_ANY, n), d);
}

int main(void) {
	for (int n = 1; n <= LAYOUT_CATALOG; n++) {
		for (int nh = 0; nh <= n; nh++) {
			int nv = n - nh;
			int blocks = layout_has_block(nh, nv) ? 4 : 1;
			for (int b = 0; b < blocks; b++) {
				catalog_case(nh, nv, b, W1, H1);
				catalog_case(nh, nv, b, W2, H2);
			}
		}
	}
	for (int n = 7; n <= 12; n++)
		board_case(n, W1, H1);
	board_case(LAYOUT_MAX, W1, H1);

	layout_compute(1, 0, 0, W1, H1, &l);
	CHECK(at(LAYOUT_H, 0, 0, 0, W1, H1), "one H takes the whole canvas");
	layout_compute(0, 1, 0, W1, H1, &l);
	CHECK(at(LAYOUT_V, 0, 0, 0, W1, H1), "one V takes the whole canvas");

	layout_compute(2, 0, 0, W1, H1, &l);
	CHECK(at(LAYOUT_H, 0, 480, 0, 960, 524) &&
	          at(LAYOUT_H, 1, 480, 524, 960, 524),
	      "2H: stacked at half scale in the middle");

	layout_compute(1, 1, 0, W1, H1, &l);
	CHECK(at(LAYOUT_V, 0, 1280, 0, 640, H1), "1H + 1V: the V has a third");
	CHECK(at(LAYOUT_H, 0, 0, 174, 1280, 699),
	      "1H + 1V: the H fills the rest, centred in height");

	layout_compute(3, 0, 0, W1, H1, &l);
	CHECK(l.slots[0].r.x == 0 && l.slots[0].r.y == 0 && l.slots[1].r.x == 960 &&
	          l.slots[1].r.y == 0 && l.slots[2].r.x == 0 &&
	          l.slots[2].r.y == 524,
	      "3H: top left, top right, bottom left, clockwise");
	CHECK(l.free[0].x == 960 && l.free[0].y == 524,
	      "3H: the bottom right cell is free");

	layout_compute(2, 1, 0, W1, H1, &l);
	CHECK(l.slots[0].type == LAYOUT_H && l.slots[1].type == LAYOUT_V &&
	          l.slots[2].type == LAYOUT_H && at(LAYOUT_V, 0, 960, 0, 960, H1),
	      "2H + 1V: H, the V on the right, H");

	layout_compute(1, 2, 0, W1, H1, &l);
	CHECK(at(LAYOUT_V, 0, 0, 0, 480, H1) && at(LAYOUT_V, 1, 1440, 0, 480, H1) &&
	          at(LAYOUT_H, 0, 480, 262, 960, 524),
	      "2V + 1H: a V on each side, the H centred at half scale");

	layout_compute(2, 2, 0, W1, H1, &l);
	CHECK(l.slots[0].type == LAYOUT_V && l.slots[1].type == LAYOUT_H &&
	          l.slots[2].type == LAYOUT_V && l.slots[3].type == LAYOUT_H,
	      "2H + 2V: clockwise is V, H, V, H");

	layout_compute(4, 1, 0, W1, H1, &l);
	CHECK(at(LAYOUT_V, 0, 1440, 0, 480, H1), "4H + 1V: the V has 25 %");
	CHECK(find(LAYOUT_H, 0)->r.y == 131 && find(LAYOUT_H, 3)->r.y == 524,
	      "4H + 1V: the grid is centred in height");

	layout_compute(1, 4, 0, W1, H1, &l);
	CHECK(at(LAYOUT_H, 0, 576, 628, 768, 420),
	      "4V + 1H: the H has 40 % of the height, centred");
	CHECK(find(LAYOUT_V, 3)->r.h == 628, "4V + 1H: the V fill the top");

	layout_compute(2, 4, 0, W1, H1, &l);
	CHECK(at(LAYOUT_H, 0, 960, 628, 768, 420) &&
	          at(LAYOUT_H, 1, 192, 628, 768, 420),
	      "4V + 2H: the first H clockwise is the bottom right one");

	layout_compute(4, 2, 0, W1, H1, &l);
	CHECK(at(LAYOUT_V, 0, 0, 0, 441, H1) && at(LAYOUT_V, 1, 1478, 0, 442, H1),
	      "4H + 2V: a V on each side with 23 %");

	layout_compute(3, 2, LAYOUT_BOTTOM_RIGHT, W1, H1, &l);
	CHECK(at(LAYOUT_V, 0, 960, 524, 480, 524) &&
	          at(LAYOUT_V, 1, 1440, 524, 480, 524),
	      "3H + 2V: the block shares the bottom right cell");
	layout_compute(3, 2, LAYOUT_TOP_RIGHT, W1, H1, &l);
	CHECK(at(LAYOUT_V, 0, 960, 0, 480, 524) &&
	          at(LAYOUT_H, 1, 960, 524, 960, 524),
	      "3H + 2V: with the block top right, the second H goes bottom right");
	CHECK(l.slots[0].type == LAYOUT_H && l.slots[1].type == LAYOUT_V &&
	          l.slots[2].type == LAYOUT_V && l.slots[3].type == LAYOUT_H,
	      "3H + 2V: the block keeps its place in the clock");
	layout_compute(3, 3, LAYOUT_BOTTOM_LEFT, W1, H1, &l);
	CHECK(at(LAYOUT_V, 2, 640, 524, 320, 524),
	      "3H + 3V: three V in the bottom left cell");

	layout_compute(2, 3, 0, W1, H1, &l);
	CHECK(at(LAYOUT_H, 0, 0, 174, 640, 350) &&
	          at(LAYOUT_H, 1, 0, 524, 640, 349),
	      "3V + 2H: the H stacked at a third, centred in height");
	CHECK(l.slots[0].type == LAYOUT_H && l.slots[4].type == LAYOUT_H,
	      "3V + 2H: clockwise is H, the three V, H");

	layout_compute(5, 0, 0, W1, H1, &l);
	CHECK(find(LAYOUT_H, 2)->r.x == 1280 && find(LAYOUT_H, 3)->r.x == 640 &&
	          find(LAYOUT_H, 4)->r.x == 0,
	      "5H: the bottom row runs right to left, past the free cell");

	layout_compute(5, 1, 0, W1, H1, &l);
	CHECK(at(LAYOUT_V, 0, 1440, 0, 480, H1) && find(LAYOUT_H, 0)->r.y == 262,
	      "5H + 1V: the V has 25 %, the 3x2 grid centred");

	layout_compute(5, 2, 0, W1, H1, &l);
	CHECK(l.n == 7 && l.nfree == 2 && l.slots[0].r.w == 640,
	      "7 windows: a 3x3 board with two free cells");

	CHECK(layout_compute(0, 0, 0, W1, H1, &l) == -1, "no windows is an error");
	CHECK(layout_compute(LAYOUT_MAX, 1, 0, W1, H1, &l) == -1,
	      "more than LAYOUT_MAX is an error");
	CHECK(layout_compute(1, 0, 0, 59, H1, &l) == -1, "a canvas too narrow");
	CHECK(layout_compute(3, 2, 4, W1, H1, &l) == -1, "a block past the clock");
	CHECK(layout_compute(4, 2, 9, W1, H1, &l) == 0,
	      "the block is ignored without one");

	struct layout_rect dr = layout_dialog(800, 600, 200, 100, 2);
	CHECK(dr.x == 298 && dr.y == 248 && dr.w == 204 && dr.h == 104,
	      "a dialog is centred over its parent, border included");
	dr = layout_dialog(800, 600, 2000, 2000, 2);
	CHECK(dr.x == 0 && dr.y == 0 && dr.w == 800 && dr.h == 600,
	      "a dialog bigger than its parent stays inside it");
	dr = layout_dialog(800, 600, 0, 0, 2);
	CHECK(dr.w == 800 && dr.h == 600, "a dialog with no size takes the room");
	dr = layout_dialog(3, 3, 50, 50, 2);
	CHECK(dr.x == 0 && dr.y == 0 && dr.w == 5 && dr.h == 5,
	      "a parent too small still gives a dialog of one pixel");

	return test_summary();
}
