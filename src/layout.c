#include <string.h>

#include "layout.h"

/* Smallest canvas side that still splits into six-way columns and thirds. */
#define LAYOUT_SIDE_MIN 60

struct build {
	struct layout *l;
	int w, h;
	int nh, nv, na;
};

static int X(const struct build *b, int num, int den) {
	return b->w * num / den;
}
static int Y(const struct build *b, int num, int den) {
	return b->h * num / den;
}

/* Splitting by edges, not by sizes, keeps neighbours touching after rounding.
 */
static int edge(int from, int to, int i, int k) {
	return from + (to - from) * i / k;
}

static void slot(struct build *b, char type, int x0, int y0, int x1, int y1) {
	struct layout_slot *s = &b->l->slots[b->l->n++];
	s->type = type;
	s->rank = type == LAYOUT_H ? b->nh++ : type == LAYOUT_V ? b->nv++ : b->na++;
	s->r.x = x0;
	s->r.y = y0;
	s->r.w = x1 - x0;
	s->r.h = y1 - y0;
}

static void gap(struct build *b, int x0, int y0, int x1, int y1) {
	if (x1 <= x0 || y1 <= y0)
		return;
	struct layout_rect *r = &b->l->free[b->l->nfree++];
	r->x = x0;
	r->y = y0;
	r->w = x1 - x0;
	r->h = y1 - y0;
}

static void columns(struct build *b, int k, char type, int x0, int y0, int x1,
                    int y1) {
	for (int i = 0; i < k; i++)
		slot(b, type, edge(x0, x1, i, k), y0, edge(x0, x1, i + 1, k), y1);
}

static void row_top(struct build *b, int cols, int x0, int y0, int x1, int y1) {
	columns(b, cols, LAYOUT_H, x0, y0, x1, y1);
}

/* Bottom row, right to left: clockwise. With one H short the bottom right
 * cell stays free. */
static void row_bottom(struct build *b, int cols, int filled, int x0, int y0,
                       int x1, int y1) {
	for (int i = cols - 1; i >= 0; i--) {
		int a = edge(x0, x1, i, cols), z = edge(x0, x1, i + 1, cols);
		if (i >= filled)
			gap(b, a, y0, z, y1);
		else
			slot(b, LAYOUT_H, a, y0, z, y1);
	}
}

static void columns_over(struct build *b, int nh, int nv) {
	int top = Y(b, 3, 5);
	columns(b, nv, LAYOUT_V, 0, 0, b->w, top);
	if (nh == 1) {
		slot(b, LAYOUT_H, X(b, 3, 10), top, X(b, 7, 10), b->h);
		gap(b, 0, top, X(b, 3, 10), b->h);
		gap(b, X(b, 7, 10), top, b->w, b->h);
		return;
	}
	slot(b, LAYOUT_H, X(b, 1, 2), top, X(b, 9, 10), b->h);
	slot(b, LAYOUT_H, X(b, 1, 10), top, X(b, 1, 2), b->h);
	gap(b, 0, top, X(b, 1, 10), b->h);
	gap(b, X(b, 9, 10), top, b->w, b->h);
}

static void grid_beside(struct build *b, int cols, int nh) {
	int right = X(b, 3, 4);
	/* cols columns of H keeping the canvas ratio in 3/4 of the width */
	int y0 = Y(b, 2 * cols - 3, 4 * cols);
	int y1 = b->h - y0;
	int mid = (y0 + y1) / 2;
	row_top(b, cols, 0, y0, right, mid);
	slot(b, LAYOUT_V, right, 0, b->w, b->h);
	row_bottom(b, cols, nh - cols, 0, mid, right, y1);
	gap(b, 0, 0, right, y0);
	gap(b, 0, y1, right, b->h);
}

static void with_block(struct build *b, int nv, int block) {
	int mx = X(b, 1, 2), my = Y(b, 1, 2);
	for (int cell = 0; cell < 4; cell++) {
		int x0 = cell == LAYOUT_TOP_LEFT || cell == LAYOUT_BOTTOM_LEFT ? 0 : mx;
		int y0 = cell == LAYOUT_TOP_LEFT || cell == LAYOUT_TOP_RIGHT ? 0 : my;
		int x1 = x0 ? b->w : mx, y1 = y0 ? b->h : my;
		if (cell == block)
			columns(b, nv, LAYOUT_V, x0, y0, x1, y1);
		else
			slot(b, LAYOUT_H, x0, y0, x1, y1);
	}
}

static void board(struct build *b, int n) {
	int cols = 1;
	while (cols * cols < n)
		cols++;
	int rows = (n + cols - 1) / cols;
	for (int i = 0; i < rows * cols; i++) {
		int c = i % cols, r = i / cols;
		int x0 = edge(0, b->w, c, cols), x1 = edge(0, b->w, c + 1, cols);
		int y0 = edge(0, b->h, r, rows), y1 = edge(0, b->h, r + 1, rows);
		if (i < n)
			slot(b, LAYOUT_ANY, x0, y0, x1, y1);
		else
			gap(b, x0, y0, x1, y1);
	}
}

int layout_has_block(int nh, int nv) { return nh == 3 && (nv == 2 || nv == 3); }

int layout_compute(int nh, int nv, int block, int w, int h,
                   struct layout *out) {
	int n = nh + nv;
	if (nh < 0 || nv < 0 || n < 1 || n > LAYOUT_MAX || w < LAYOUT_SIDE_MIN ||
	    h < LAYOUT_SIDE_MIN)
		return -1;
	if (layout_has_block(nh, nv) && (block < 0 || block > 3))
		return -1;

	memset(out, 0, sizeof(*out));
	struct build b = {.l = out, .w = w, .h = h};

	if (n > LAYOUT_CATALOG) {
		board(&b, n);
		return 0;
	}
	if (n == 1) {
		slot(&b, nh ? LAYOUT_H : LAYOUT_V, 0, 0, w, h);
		return 0;
	}
	if (nh == 0) {
		columns(&b, nv, LAYOUT_V, 0, 0, w, h);
		return 0;
	}
	if (layout_has_block(nh, nv)) {
		with_block(&b, nv, block);
		return 0;
	}

	int x2 = X(&b, 1, 2), y2 = Y(&b, 1, 2);
	int x4 = X(&b, 1, 4), x34 = X(&b, 3, 4);

	switch (nh * 10 + nv) {
	case 20:
		slot(&b, LAYOUT_H, x4, 0, x34, y2);
		slot(&b, LAYOUT_H, x4, y2, x34, h);
		gap(&b, 0, 0, x4, h);
		gap(&b, x34, 0, w, h);
		break;
	case 11:
		slot(&b, LAYOUT_H, 0, Y(&b, 1, 6), X(&b, 2, 3), Y(&b, 5, 6));
		slot(&b, LAYOUT_V, X(&b, 2, 3), 0, w, h);
		gap(&b, 0, 0, X(&b, 2, 3), Y(&b, 1, 6));
		gap(&b, 0, Y(&b, 5, 6), X(&b, 2, 3), h);
		break;
	case 30:
	case 40:
		row_top(&b, 2, 0, 0, w, y2);
		row_bottom(&b, 2, nh - 2, 0, y2, w, h);
		break;
	case 21:
		slot(&b, LAYOUT_H, 0, 0, x2, y2);
		slot(&b, LAYOUT_V, x2, 0, w, h);
		slot(&b, LAYOUT_H, 0, y2, x2, h);
		break;
	case 12:
		slot(&b, LAYOUT_V, 0, 0, x4, h);
		slot(&b, LAYOUT_H, x4, Y(&b, 1, 4), x34, Y(&b, 3, 4));
		slot(&b, LAYOUT_V, x34, 0, w, h);
		gap(&b, x4, 0, x34, Y(&b, 1, 4));
		gap(&b, x4, Y(&b, 3, 4), x34, h);
		break;
	case 22:
		slot(&b, LAYOUT_V, 0, 0, x4, h);
		slot(&b, LAYOUT_H, x4, 0, x34, y2);
		slot(&b, LAYOUT_V, x34, 0, w, h);
		slot(&b, LAYOUT_H, x4, y2, x34, h);
		break;
	case 31:
	case 41:
		grid_beside(&b, 2, nh);
		break;
	case 51:
		grid_beside(&b, 3, nh);
		break;
	case 13:
	case 14:
	case 15:
	case 24:
		columns_over(&b, nh, nv);
		break;
	case 42: {
		int l = X(&b, 23, 100), r = X(&b, 77, 100);
		int t = Y(&b, 23, 100), d = Y(&b, 77, 100);
		int my = (t + d) / 2;
		slot(&b, LAYOUT_V, 0, 0, l, h);
		row_top(&b, 2, l, t, r, my);
		slot(&b, LAYOUT_V, r, 0, w, h);
		row_bottom(&b, 2, 2, l, my, r, d);
		gap(&b, l, 0, r, t);
		gap(&b, l, d, r, h);
		break;
	}
	case 23: {
		int l = X(&b, 1, 3);
		slot(&b, LAYOUT_H, 0, Y(&b, 1, 6), l, y2);
		columns(&b, 3, LAYOUT_V, l, 0, w, h);
		slot(&b, LAYOUT_H, 0, y2, l, Y(&b, 5, 6));
		gap(&b, 0, 0, l, Y(&b, 1, 6));
		gap(&b, 0, Y(&b, 5, 6), l, h);
		break;
	}
	case 50:
	case 60: {
		int t = Y(&b, 1, 6), d = Y(&b, 5, 6);
		row_top(&b, 3, 0, t, w, y2);
		row_bottom(&b, 3, nh - 3, 0, y2, w, d);
		gap(&b, 0, 0, w, t);
		gap(&b, 0, d, w, h);
		break;
	}
	default:
		return -1;
	}
	return 0;
}
