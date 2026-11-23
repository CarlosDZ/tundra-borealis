#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tundra/draw.h>
#include <unistd.h>
#include <wlr/types/wlr_scene.h>

#include "art.h"
#include "fill.h"
#include "pick.h"
#include "pixbuf.h"
#include "server.h"

#define FILL_DRAWINGS 64
#define FILL_NAME 64

struct drawing {
	char name[FILL_NAME];
	struct art art;
	int w, h;
	struct pixbuf *buf[2];
	int on;
	int users;
	int in_view;
	unsigned int frames;
};

struct hole {
	int used;
	enum fill_kind kind;
	struct layout_rect r;
	struct drawing *d;
	int scale;
	struct wlr_scene_buffer *node;
};

struct fills {
	struct drawing kinds[FILL_KINDS][FILL_DRAWINGS];
	int n[FILL_KINDS];
	struct hole free[DESK_CANVASES][LAYOUT_FREE_MAX];
	struct hole fallen[DESK_CANVASES][FIXED_MAX];
	struct wl_event_source *timer;
	uint32_t seed;
};

static const char *const kind_dir[FILL_KINDS] = {"empty", "free", "fallen"};

static uint64_t now_ms(void) {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint64_t)t.tv_sec * 1000 + (uint64_t)t.tv_nsec / 1000000;
}

static unsigned int next_rand(struct fills *f) {
	f->seed ^= f->seed << 13;
	f->seed ^= f->seed >> 17;
	f->seed ^= f->seed << 5;
	return f->seed;
}

static int same_rect(struct layout_rect a, struct layout_rect b) {
	return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

static void put_on_holes(struct fills *f, struct drawing *d) {
	struct pixbuf *b = d->on >= 0 ? d->buf[d->on] : NULL;
	if (!b)
		return;
	for (int c = 0; c < DESK_CANVASES; c++) {
		for (int i = 0; i < LAYOUT_FREE_MAX; i++)
			if (f->free[c][i].d == d && f->free[c][i].node)
				wlr_scene_buffer_set_buffer(f->free[c][i].node, &b->base);
		for (int i = 0; i < FIXED_MAX; i++)
			if (f->fallen[c][i].d == d && f->fallen[c][i].node)
				wlr_scene_buffer_set_buffer(f->fallen[c][i].node, &b->base);
	}
}

static void paint(struct fills *f, struct drawing *d) {
	struct pixbuf *b = pixbuf_spare(d->buf, &d->on, d->w, d->h);
	if (!b)
		return;
	memset(b->px, 0, b->stride * (size_t)d->h);
	tdraw_art(b->canvas, d->art.art, 0, 0, 1, 0);
	d->frames++;
	put_on_holes(f, d);
}

static int advance(struct fills *f, uint64_t now) {
	int next = -1;
	for (int k = 0; k < FILL_KINDS; k++)
		for (int i = 0; i < f->n[k]; i++) {
			struct drawing *d = &f->kinds[k][i];
			struct tdraw_rect r;
			int ms;
			if (!d->users || !d->in_view)
				continue;
			ms = tdraw_art_advance(d->art.art, now, &r);
			if (r.w && r.h)
				paint(f, d);
			if (ms >= 0 && (next < 0 || ms < next))
				next = ms;
		}
	return next;
}

static void schedule(struct fills *f, int ms) {
	if (f->timer)
		wl_event_source_timer_update(f->timer, ms < 0 ? 0 : ms < 1 ? 1 : ms);
}

static int on_timer(void *data) {
	struct fills *f = data;
	schedule(f, advance(f, now_ms()));
	return 0;
}

static void start(struct fills *f, struct drawing *d, uint64_t now) {
	const char *anim = tdraw_art_anim(d->art.art, 0);
	if (anim)
		tdraw_art_play(d->art.art, anim, now);
	tdraw_art_advance(d->art.art, now, NULL);
	paint(f, d);
}

static void leave(struct hole *h) {
	if (h->d)
		h->d->users--;
	h->d = NULL;
	h->scale = 0;
}

static void release(struct hole *h) {
	leave(h);
	if (h->node)
		wlr_scene_node_destroy(&h->node->node);
	memset(h, 0, sizeof(*h));
}

static void choose(struct fills *f, struct hole *h, enum fill_kind kind,
                   struct layout_rect r) {
	struct pick_size size[FILL_DRAWINGS];
	int scale = 0, i;
	leave(h);
	h->used = 1;
	h->kind = kind;
	h->r = r;
	for (i = 0; i < f->n[kind]; i++)
		size[i] = (struct pick_size){f->kinds[kind][i].w, f->kinds[kind][i].h};
	i = pick(size, f->n[kind], r.w, r.h, next_rand(f), &scale);
	if (i < 0)
		return;
	h->d = &f->kinds[kind][i];
	h->scale = scale;
	if (h->d->users++ == 0)
		start(f, h->d, now_ms());
}

static void show(struct fills *f, struct hole *h,
                 struct wlr_scene_tree *parent) {
	int dw, dh;
	struct pixbuf *b;
	(void)f;
	if (!h->d) {
		if (h->node)
			wlr_scene_node_destroy(&h->node->node);
		h->node = NULL;
		return;
	}
	if (!h->node) {
		h->node = wlr_scene_buffer_create(parent, NULL);
		if (!h->node)
			return;
		wlr_scene_buffer_set_filter_mode(h->node, WLR_SCALE_FILTER_NEAREST);
	} else if (h->node->node.parent != parent) {
		wlr_scene_node_reparent(&h->node->node, parent);
	}
	dw = h->d->w * h->scale;
	dh = h->d->h * h->scale;
	wlr_scene_buffer_set_dest_size(h->node, dw, dh);
	wlr_scene_node_set_position(&h->node->node, h->r.x + (h->r.w - dw) / 2,
	                            h->r.y + (h->r.h - dh) / 2);
	b = h->d->on >= 0 ? h->d->buf[h->d->on] : NULL;
	if (b)
		wlr_scene_buffer_set_buffer(h->node, &b->base);
}

void fill_holes(struct server *s, int index, const struct layout_rect *r, int n,
                enum fill_kind kind) {
	struct fills *f = s->fills;
	struct hole *old, next[LAYOUT_FREE_MAX];
	if (!f)
		return;
	old = f->free[index];
	if (n > LAYOUT_FREE_MAX)
		n = LAYOUT_FREE_MAX;
	memset(next, 0, sizeof(next));
	for (int j = 0; j < n; j++)
		for (int k = 0; k < LAYOUT_FREE_MAX; k++)
			if (old[k].used && old[k].kind == kind &&
			    same_rect(old[k].r, r[j])) {
				next[j] = old[k];
				memset(&old[k], 0, sizeof(old[k]));
				break;
			}
	for (int k = 0; k < LAYOUT_FREE_MAX; k++)
		if (old[k].used)
			release(&old[k]);
	for (int j = 0; j < n; j++)
		if (!next[j].used)
			choose(f, &next[j], kind, r[j]);
	memcpy(old, next, sizeof(next));
	for (int j = 0; j < n; j++)
		show(f, &old[j], s->canvas[index]);
}

void fill_fallen(struct server *s, int index, int prog, struct layout_rect r,
                 int fallen) {
	struct fills *f = s->fills;
	struct hole *h;
	if (!f || prog < 0 || prog >= FIXED_MAX)
		return;
	h = &f->fallen[index][prog];
	if (!fallen) {
		if (h->used)
			release(h);
		return;
	}
	if (!h->used || !same_rect(h->r, r))
		choose(f, h, FILL_FALLEN, r);
	show(f, h, s->canvas[index]);
	if (h->node)
		wlr_scene_node_raise_to_top(&h->node->node);
}

void fill_view(struct server *s) {
	struct fills *f = s->fills;
	int woke = 0;
	if (!f)
		return;
	for (int k = 0; k < FILL_KINDS; k++)
		for (int i = 0; i < f->n[k]; i++)
			f->kinds[k][i].in_view = 0;
	for (int c = 0; c < DESK_CANVASES; c++) {
		int seen =
		    grid_is_shown(&s->desk.grid, desk_pos(c)) && !s->desk.fullscreen[c];
		for (int i = 0; i < LAYOUT_FREE_MAX + FIXED_MAX && seen; i++) {
			struct hole *h = i < LAYOUT_FREE_MAX
			                     ? &f->free[c][i]
			                     : &f->fallen[c][i - LAYOUT_FREE_MAX];
			if (h->d && !h->d->in_view) {
				h->d->in_view = 1;
				woke = 1;
			}
		}
	}
	if (woke)
		schedule(f, advance(f, now_ms()));
}

void fill_theme(struct server *s) {
	struct fills *f = s->fills;
	if (!f)
		return;
	for (int k = 0; k < FILL_KINDS; k++)
		for (int i = 0; i < f->n[k]; i++) {
			struct drawing *d = &f->kinds[k][i];
			if (tdraw_art_theme(d->art.art, s->theme) == 1 && d->users)
				paint(f, d);
		}
}

static int by_name(const void *a, const void *b) {
	return strcmp(((const struct drawing *)a)->name,
	              ((const struct drawing *)b)->name);
}

static void load_dir(struct server *s, enum fill_kind k, const char *dir) {
	struct fills *f = s->fills;
	struct dirent *e;
	DIR *d = opendir(dir);
	if (!d)
		return;
	while ((e = readdir(d))) {
		size_t len = strlen(e->d_name);
		char name[FILL_NAME], path[4200];
		struct art a;
		int i;
		if (len < 5 || strcmp(e->d_name + len - 4, ".toa") ||
		    len - 4 >= sizeof(name))
			continue;
		memcpy(name, e->d_name, len - 4);
		name[len - 4] = '\0';
		for (i = 0; i < f->n[k] && strcmp(f->kinds[k][i].name, name); i++)
			;
		if (i == FILL_DRAWINGS) {
			fprintf(stderr, "art: %s: more than %d drawings, %s left out\n",
			        dir, FILL_DRAWINGS, e->d_name);
			continue;
		}
		snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
		if (art_open(&a, path, s->theme))
			continue;
		if (i < f->n[k])
			art_unload(&f->kinds[k][i].art);
		else
			f->n[k]++;
		memset(&f->kinds[k][i], 0, sizeof(f->kinds[k][i]));
		snprintf(f->kinds[k][i].name, FILL_NAME, "%s", name);
		f->kinds[k][i].art = a;
		f->kinds[k][i].on = -1;
		tdraw_art_size(a.art, &f->kinds[k][i].w, &f->kinds[k][i].h);
	}
	closedir(d);
}

int fill_init(struct server *s) {
	struct wl_event_loop *loop = wl_display_get_event_loop(s->display);
	const char *seed = getenv("TUNDRA_BOREALIS_SEED");
	char user[4096], dir[4200];
	struct fills *f = calloc(1, sizeof(*f));
	if (!f) {
		perror("calloc error");
		return -1;
	}
	s->fills = f;
	f->seed = seed && *seed ? (uint32_t)strtoul(seed, NULL, 10)
	                        : (uint32_t)time(NULL) ^ (uint32_t)getpid();
	if (!f->seed)
		f->seed = 1;
	art_user_dir(user, sizeof(user));
	for (int k = 0; k < FILL_KINDS; k++) {
		snprintf(dir, sizeof(dir), "%s/%s", art_system_dir(), kind_dir[k]);
		load_dir(s, k, dir);
		if (user[0]) {
			snprintf(dir, sizeof(dir), "%s/%s", user, kind_dir[k]);
			load_dir(s, k, dir);
		}
		qsort(f->kinds[k], (size_t)f->n[k], sizeof(f->kinds[k][0]), by_name);
	}
	f->timer = wl_event_loop_add_timer(loop, on_timer, f);
	if (!f->timer) {
		fprintf(stderr, "cannot time the drawings\n");
		return -1;
	}
	return 0;
}

void fill_finish(struct server *s) {
	struct fills *f = s->fills;
	if (!f)
		return;
	if (f->timer)
		wl_event_source_remove(f->timer);
	for (int c = 0; c < DESK_CANVASES; c++) {
		for (int i = 0; i < LAYOUT_FREE_MAX; i++)
			release(&f->free[c][i]);
		for (int i = 0; i < FIXED_MAX; i++)
			release(&f->fallen[c][i]);
	}
	for (int k = 0; k < FILL_KINDS; k++)
		for (int i = 0; i < f->n[k]; i++) {
			pixbuf_drop(f->kinds[k][i].buf[0]);
			pixbuf_drop(f->kinds[k][i].buf[1]);
			art_unload(&f->kinds[k][i].art);
		}
	free(f);
	s->fills = NULL;
}

#ifdef TUNDRA_BOREALIS_TEST
int fill_info(struct server *s, int index, int i, struct fill_info *out) {
	struct fills *f = s->fills;
	const struct hole *h = NULL;
	for (int k = 0, seen = 0; f && k < LAYOUT_FREE_MAX + FIXED_MAX; k++) {
		const struct hole *c = k < LAYOUT_FREE_MAX
		                           ? &f->free[index][k]
		                           : &f->fallen[index][k - LAYOUT_FREE_MAX];
		if (c->used && seen++ == i) {
			h = c;
			break;
		}
	}
	if (!h)
		return -1;
	out->kind = h->kind;
	out->name = h->d ? h->d->name : "-";
	out->r = h->r;
	out->w = h->d ? h->d->w : 0;
	out->h = h->d ? h->d->h : 0;
	out->scale = h->scale;
	out->in_view = h->d ? h->d->in_view : 0;
	out->frames = h->d ? h->d->frames : 0;
	return 0;
}
#endif
