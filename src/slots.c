#include <string.h>

#include "slots.h"

static int find(const struct slots *s, unsigned int id) {
	for (int i = 0; i < s->n; i++)
		if (s->w[i].id == id)
			return i;
	return -1;
}

static int count(const struct slots *s, char type) {
	int k = 0;
	for (int i = 0; i < s->n; i++)
		k += s->w[i].type == type;
	return k;
}

static int nth(const struct slots *s, char type, int rank) {
	for (int i = 0; i < s->n; i++)
		if (s->w[i].type == type && rank-- == 0)
			return i;
	return -1;
}

static int rank_of(const struct slots *s, int index) {
	int r = 0;
	for (int i = 0; i < index; i++)
		r += s->w[i].type == s->w[index].type;
	return r;
}

static int has_block(const struct slots *s) {
	return layout_has_block(count(s, LAYOUT_H), count(s, LAYOUT_V));
}

static void settle(struct slots *s, int had_block) {
	if (has_block(s) && !had_block)
		s->block = LAYOUT_BOTTOM_RIGHT;
}

static void swap(struct slots *s, int a, int b) {
	struct slots_win t = s->w[a];
	s->w[a] = s->w[b];
	s->w[b] = t;
}

void slots_init(struct slots *s) {
	memset(s, 0, sizeof(*s));
	s->block = LAYOUT_BOTTOM_RIGHT;
}

int slots_add(struct slots *s, unsigned int id, char type) {
	if (id == 0 || s->n == LAYOUT_MAX || find(s, id) >= 0 ||
	    (type != LAYOUT_H && type != LAYOUT_V))
		return -1;
	int had = has_block(s);
	s->w[s->n].id = id;
	s->w[s->n].type = type;
	s->n++;
	settle(s, had);
	return 0;
}

int slots_remove(struct slots *s, unsigned int id) {
	int i = find(s, id);
	if (i < 0)
		return -1;
	int had = has_block(s);
	memmove(&s->w[i], &s->w[i + 1], (size_t)(s->n - i - 1) * sizeof(s->w[0]));
	s->n--;
	settle(s, had);
	return 0;
}

int slots_rotate(struct slots *s, unsigned int id) {
	int i = find(s, id);
	if (i < 0)
		return -1;
	int had = has_block(s);
	s->w[i].type = s->w[i].type == LAYOUT_H ? LAYOUT_V : LAYOUT_H;
	settle(s, had);
	return 0;
}

/* The four clock cells hold H ids by rank, skipping the block's, marked 0. */
static int move_in_block(struct slots *s, int index) {
	unsigned int cells[4];
	for (int c = 0, r = 0; c < 4; c++)
		cells[c] = c == s->block ? 0 : s->w[nth(s, LAYOUT_H, r++)].id;

	int from = s->block;
	if (s->w[index].type == LAYOUT_H)
		for (int c = 0; c < 4; c++)
			if (cells[c] == s->w[index].id)
				from = c;
	int to = (from + 1) % 4;
	unsigned int t = cells[from];
	cells[from] = cells[to];
	cells[to] = t;

	struct slots_win h[4];
	int k = 0;
	for (int c = 0; c < 4; c++) {
		if (cells[c] == 0) {
			s->block = c;
			continue;
		}
		h[k].id = cells[c];
		h[k].type = LAYOUT_H;
		k++;
	}
	for (int i = 0, r = 0; i < s->n; i++)
		if (s->w[i].type == LAYOUT_H)
			s->w[i] = h[r++];
	return 0;
}

int slots_move(struct slots *s, unsigned int id) {
	int i = find(s, id);
	if (i < 0)
		return -1;
	if (s->n > LAYOUT_CATALOG) {
		swap(s, i, (i + 1) % s->n);
		return 0;
	}
	if (has_block(s))
		return move_in_block(s, i);
	char type = s->w[i].type;
	int k = count(s, type);
	if (k < 2)
		return -1;
	swap(s, i, nth(s, type, (rank_of(s, i) + 1) % k));
	return 0;
}

int slots_place(const struct slots *s, int w, int h, struct slots_place *out,
                struct layout *l) {
	if (s->n == 0) {
		if (layout_compute(1, 0, 0, w, h, l) < 0)
			return -1;
		l->n = 0;
		l->nfree = 1;
		l->free[0] = l->slots[0].r;
		return 0;
	}
	if (layout_compute(count(s, LAYOUT_H), count(s, LAYOUT_V), s->block, w, h,
	                   l) < 0)
		return -1;
	for (int i = 0; i < l->n; i++) {
		const struct layout_slot *sl = &l->slots[i];
		int index =
		    sl->type == LAYOUT_ANY ? sl->rank : nth(s, sl->type, sl->rank);
		out[i].id = s->w[index].id;
		out[i].r = sl->r;
	}
	return l->n;
}

unsigned int slots_next(const struct slots_place *p, int n, unsigned int id) {
	for (int i = 0; i < n; i++)
		if (p[i].id == id)
			return p[(i + 1) % n].id;
	return 0;
}
