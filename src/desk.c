#include <string.h>

#include "desk.h"

int desk_index(struct grid_pos p) { return (p.h - 1) * GRID_SIDE + p.d - 1; }

struct grid_pos desk_pos(int index) {
	struct grid_pos p = {index / GRID_SIDE + 1, index % GRID_SIDE + 1};
	return p;
}

void desk_init(struct desk *d, int w, int h) {
	memset(d, 0, sizeof(*d));
	grid_init(&d->grid);
	for (int i = 0; i < DESK_CANVASES; i++)
		slots_init(&d->canvas[i]);
	d->w = w;
	d->h = h;
}

void desk_resize(struct desk *d, int w, int h) {
	d->w = w;
	d->h = h;
}

int desk_find(const struct desk *d, unsigned int id) {
	for (int c = 0; c < DESK_CANVASES; c++)
		for (int i = 0; i < d->canvas[c].n; i++)
			if (d->canvas[c].w[i].id == id)
				return c;
	return -1;
}

static int here(const struct desk *d) { return desk_index(d->grid.focus); }

unsigned int desk_focused(const struct desk *d) { return d->focus[here(d)]; }

int desk_place(const struct desk *d, int index, struct slots_place *out,
               struct layout *l) {
	return slots_place(&d->canvas[index], d->w, d->h, out, l);
}

static unsigned int after(const struct desk *d, int c, unsigned int id) {
	struct slots_place p[LAYOUT_MAX];
	struct layout l;
	int n = desk_place(d, c, p, &l);
	unsigned int next = n > 0 ? slots_next(p, n, id) : 0;
	return next == id ? 0 : next;
}

int desk_open(struct desk *d, unsigned int id, char type) {
	int c = here(d);
	if (id == 0 || desk_find(d, id) >= 0 ||
	    slots_add(&d->canvas[c], id, type) < 0)
		return -1;
	d->focus[c] = id;
	return c;
}

int desk_close(struct desk *d, unsigned int id) {
	int c = desk_find(d, id);
	if (c < 0)
		return -1;
	if (d->focus[c] == id)
		d->focus[c] = after(d, c, id);
	slots_remove(&d->canvas[c], id);
	return c;
}

int desk_focus_window(struct desk *d, unsigned int id) {
	int c = desk_find(d, id);
	if (c < 0)
		return -1;
	d->focus[c] = id;
	if (c != here(d))
		grid_goto(&d->grid, desk_pos(c));
	return 0;
}

int desk_focus_next(struct desk *d) {
	int c = here(d);
	unsigned int next = after(d, c, d->focus[c]);
	if (!next)
		return -1;
	d->focus[c] = next;
	return c;
}

int desk_move(struct desk *d) {
	int c = here(d);
	if (!d->focus[c] || slots_move(&d->canvas[c], d->focus[c]) < 0)
		return -1;
	return c;
}

int desk_rotate(struct desk *d) {
	int c = here(d);
	if (!d->focus[c] || slots_rotate(&d->canvas[c], d->focus[c]) < 0)
		return -1;
	return c;
}

int desk_step(struct desk *d, enum grid_dir dir) {
	struct grid_pos to;
	if (grid_step(&d->grid, dir, &to) < 0)
		return -1;
	return grid_goto(&d->grid, to);
}

int desk_goto(struct desk *d, struct grid_pos p) {
	return grid_goto(&d->grid, p);
}

int desk_send(struct desk *d, struct grid_pos to) {
	int from = here(d);
	unsigned int id = d->focus[from];
	if (!id || !grid_valid(to) || desk_index(to) == from)
		return -1;
	struct slots *src = &d->canvas[from], *dst = &d->canvas[desk_index(to)];
	if (dst->n >= LAYOUT_MAX)
		return -1;
	int i = 0;
	while (src->w[i].id != id)
		i++;
	char type = src->w[i].type;
	d->focus[from] = after(d, from, id);
	slots_remove(src, id);
	slots_add(dst, id, type);
	d->focus[desk_index(to)] = id;
	grid_goto(&d->grid, to);
	return 0;
}

int desk_send_step(struct desk *d, enum grid_dir dir) {
	struct grid_pos to;
	if (grid_step(&d->grid, dir, &to) < 0)
		return -1;
	return desk_send(d, to);
}
