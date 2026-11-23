#include <drm_fourcc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timerfd.h>
#include <time.h>
#include <tundra/draw.h>
#include <unistd.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_scene.h>

#include "header.h"
#include "outputs.h"
#include "server.h"

struct hbuf {
	struct wlr_buffer base;
	uint32_t *px;
	size_t stride;
	struct tdraw_canvas *canvas;
};

struct header {
	struct wl_list link;
	struct server *server;
	struct wlr_scene_buffer *node;
	struct hbuf *buf[2];
	int on;
	struct tdraw_font *font;
	int x, y, w, screen_h, line, height, screen, shown;
	int stale;
	char place[HEAD_TEXT], clock[HEAD_TEXT];
	uint32_t surface, accent, text;
	int battery, brightness;
	unsigned int drawn;
};

static void hbuf_destroy(struct wlr_buffer *base) {
	struct hbuf *b = wl_container_of(base, b, base);
	tdraw_canvas_free(b->canvas);
	free(b->px);
	free(b);
}

static bool hbuf_begin(struct wlr_buffer *base, uint32_t flags, void **data,
                       uint32_t *format, size_t *stride) {
	struct hbuf *b = wl_container_of(base, b, base);
	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE)
		return false;
	*data = b->px;
	*format = DRM_FORMAT_ARGB8888;
	*stride = b->stride;
	return true;
}

static void hbuf_end(struct wlr_buffer *base) { (void)base; }

static const struct wlr_buffer_impl hbuf_impl = {
    .destroy = hbuf_destroy,
    .begin_data_ptr_access = hbuf_begin,
    .end_data_ptr_access = hbuf_end,
};

static struct hbuf *hbuf_new(int w, int h) {
	struct hbuf *b = calloc(1, sizeof(*b));
	if (!b)
		return NULL;
	b->stride = (size_t)w * 4;
	b->px = calloc((size_t)h, b->stride);
	b->canvas = b->px ? tdraw_canvas_new(b->px, w, h, (int)b->stride) : NULL;
	if (!b->canvas) {
		free(b->px);
		free(b);
		return NULL;
	}
	wlr_buffer_init(&b->base, &hbuf_impl, w, h);
	return b;
}

static void hbuf_drop(struct hbuf *b) {
	if (b)
		wlr_buffer_drop(&b->base);
}

static struct hbuf *spare(struct header *h) {
	int i = h->on == 0 ? 1 : 0;
	struct hbuf *b = h->buf[i];
	if (b && (b->base.width != h->w || b->base.height != h->height ||
	          b->base.n_locks > 0)) {
		hbuf_drop(b);
		b = h->buf[i] = NULL;
	}
	if (!b)
		b = h->buf[i] = hbuf_new(h->w, h->height);
	if (b)
		h->on = i;
	return b;
}

static void paint_status(struct header *h, struct tdraw_canvas *c) {
	struct server *s = h->server;
	const struct status *st = &s->status;
	struct tdraw_art *art[2];
	char num[2][16];
	int value[2] = {st->power.battery, st->power.brightness}, n = 0;
	int scale[2], aw[2], ah[2], iw[2], nw[2], ix[2], nx[2];
	struct tdraw_art *all[2] = {st->battery.art, st->brightness.art};
	for (int i = 0; i < 2; i++) {
		if (value[i] < 0)
			continue;
		art[n] = all[i];
		aw[n] = ah[n] = 0;
		if (art[n])
			tdraw_art_size(art[n], &aw[n], &ah[n]);
		scale[n] = head_icon_scale(h->line, ah[n]);
		iw[n] = art[n] ? aw[n] * scale[n] : h->line;
		snprintf(num[n], sizeof(num[n]), "%d %%", value[i]);
		nw[n] = h->font ? tdraw_text_width(h->font, num[n], strlen(num[n])) : 0;
		if (nw[n] < 0)
			nw[n] = 0;
		n++;
	}
	head_indicators(h->w, h->line, n, iw, nw, ix, nx);
	for (int i = 0; i < n; i++) {
		if (art[i])
			tdraw_art(c, art[i], ix[i], (h->height - ah[i] * scale[i]) / 2,
			          scale[i], 0);
		if (h->font)
			tdraw_text(c, h->font, nx[i], head_pad(h->line), num[i],
			           strlen(num[i]), s->colors.text);
	}
}

static void paint(struct header *h, const char *place) {
	struct server *s = h->server;
	const struct colors *c = &s->colors;
	struct hbuf *b = spare(h);
	if (!b)
		return;
	tdraw_fill(b->canvas, 0, 0, h->w, h->height, c->surface);
	if (h->font) {
		size_t pl = strlen(place), cl = strlen(s->clock);
		int pw = tdraw_text_width(h->font, place, pl);
		int cw = tdraw_text_width(h->font, s->clock, cl);
		struct head_layout l;
		head_layout(h->w, h->line, pw > 0 ? pw : 0, cw > 0 ? cw : 0, &l);
		tdraw_text(b->canvas, h->font, l.place_x, l.y, place, pl, c->accent);
		tdraw_text(b->canvas, h->font, l.clock_x, l.y, s->clock, cl, c->text);
	}
	paint_status(h, b->canvas);
	wlr_scene_buffer_set_buffer(h->node, &b->base);
	snprintf(h->place, sizeof(h->place), "%s", place);
	snprintf(h->clock, sizeof(h->clock), "%s", s->clock);
	h->surface = c->surface;
	h->accent = c->accent;
	h->text = c->text;
	h->battery = s->status.power.battery;
	h->brightness = s->status.power.brightness;
	h->drawn = s->status.drawn;
	h->stale = 0;
}

void header_refresh(struct server *s) {
	struct header *h;
	wl_list_for_each(h, &s->headers, link) {
		char place[HEAD_TEXT];
		if (!h->shown || h->w < 1)
			continue;
		head_place(grid_shown(&s->desk.grid, h->screen), place);
		if (h->stale || strcmp(place, h->place) || strcmp(s->clock, h->clock) ||
		    h->surface != s->colors.surface || h->accent != s->colors.accent ||
		    h->text != s->colors.text ||
		    h->battery != s->status.power.battery ||
		    h->brightness != s->status.power.brightness ||
		    h->drawn != s->status.drawn)
			paint(h, place);
	}
}

struct header *header_create(struct server *s) {
	struct header *h = calloc(1, sizeof(*h));
	if (!h)
		return NULL;
	h->node = wlr_scene_buffer_create(s->header_tree, NULL);
	if (!h->node) {
		free(h);
		return NULL;
	}
	h->server = s;
	h->on = -1;
	h->shown = 1;
	h->stale = 1;
	wl_list_insert(s->headers.prev, &h->link);
	return h;
}

void header_destroy(struct header *h, int scene_gone) {
	if (!h)
		return;
	wl_list_remove(&h->link);
	if (!scene_gone)
		wlr_scene_node_destroy(&h->node->node);
	hbuf_drop(h->buf[0]);
	hbuf_drop(h->buf[1]);
	tdraw_font_free(h->font);
	free(h);
}

static void set_line(struct header *h, int screen_h) {
	const char *err;
	int target = head_target(screen_h);
	int line = tdraw_font_exact("ui", target, &err);
	if (line < 0)
		fprintf(stderr, "header: %s\n", err);
	if (line <= 0)
		line = target;
	if (line != h->line || !h->font) {
		tdraw_font_free(h->font);
		h->font = tdraw_font_open("ui", line, &err);
		if (!h->font)
			fprintf(stderr, "header: %s\n", err);
	}
	h->screen_h = screen_h;
	h->line = line;
	h->height = head_height(line);
}

void header_place(struct header *h, int x, int y, int w, int screen_h,
                  int screen) {
	if (!h)
		return;
	if (screen_h != h->screen_h)
		set_line(h, screen_h);
	h->x = x;
	h->y = y;
	h->w = w;
	h->screen = screen;
	h->stale = 1;
	wlr_scene_node_set_position(&h->node->node, x, y);
}

void header_show(struct header *h, int shown) {
	if (!h)
		return;
	h->shown = shown;
	h->stale = 1;
	wlr_scene_node_set_enabled(&h->node->node, shown);
}

int header_height(const struct header *h) { return h ? h->height : 0; }

static void tick(struct server *s) {
	time_t now = time(NULL);
	struct tm tm;
	if (localtime_r(&now, &tm))
		head_clock(&tm, s->clock);
}

static void arm(struct server *s) {
	struct itimerspec at = {0};
	at.it_value.tv_sec = head_next_minute(time(NULL));
	if (timerfd_settime(s->minute_fd,
	                    TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET, &at,
	                    NULL) < 0)
		perror("timerfd_settime error");
}

static int on_minute(int fd, uint32_t mask, void *data) {
	struct server *s = data;
	uint64_t n;
	(void)mask;
	if (read(fd, &n, sizeof(n)) < 0)
		n = 0;
	tick(s);
	arm(s);
	status_update(s);
	return 0;
}

static int load_theme(struct server *s) {
	const char *err;
	int r = tdraw_theme_load(s->theme, &err);
	if (r == -1)
		fprintf(stderr, "theme: out of memory\n");
	else if (r == -2)
		fprintf(stderr, "theme: %s, the colors stay\n", err);
	if (r == 1)
		colors_load(&s->colors, s->theme);
	return r == 1;
}

static int on_theme(int fd, uint32_t mask, void *data) {
	struct server *s = data;
	(void)fd;
	(void)mask;
	if (load_theme(s)) {
		status_theme(s);
		outputs_recolor(s);
		server_arrange_all(s);
	}
	return 0;
}

int header_init(struct server *s) {
	struct wl_event_loop *loop = wl_display_get_event_loop(s->display);
	colors_load(&s->colors, NULL);
	tick(s);
	s->theme = tdraw_theme_open();
	if (s->theme) {
		load_theme(s);
		int fd = tdraw_theme_fd(s->theme);
		if (fd >= 0)
			s->theme_source =
			    wl_event_loop_add_fd(loop, fd, WL_EVENT_READABLE, on_theme, s);
		if (!s->theme_source)
			fprintf(stderr, "theme: not watched, changes need a restart\n");
	}
	s->minute_fd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
	if (s->minute_fd < 0) {
		perror("timerfd_create error");
		return -1;
	}
	s->minute_source = wl_event_loop_add_fd(loop, s->minute_fd,
	                                        WL_EVENT_READABLE, on_minute, s);
	if (!s->minute_source) {
		fprintf(stderr, "cannot watch the clock\n");
		return -1;
	}
	arm(s);
	return 0;
}

void header_finish(struct server *s) {
	if (s->theme_source)
		wl_event_source_remove(s->theme_source);
	if (s->minute_source)
		wl_event_source_remove(s->minute_source);
	if (s->minute_fd >= 0)
		close(s->minute_fd);
	tdraw_theme_free(s->theme);
	s->theme_source = s->minute_source = NULL;
	s->minute_fd = -1;
	s->theme = NULL;
}

#ifdef TUNDRA_BOREALIS_TEST
void header_info(const struct header *h, struct header_info *i) {
	i->x = h->x;
	i->y = h->y;
	i->w = h->w;
	i->h = h->height;
	i->line = h->line;
	i->shown = h->shown;
	i->font = h->font != NULL;
	i->place = h->place;
	i->clock = h->clock;
}

uint32_t header_pixel(const struct header *h, int x, int y) {
	const struct hbuf *b = h && h->on >= 0 ? h->buf[h->on] : NULL;
	if (!b || x < 0 || y < 0 || x >= b->base.width || y >= b->base.height)
		return 0;
	return b->px[(size_t)y * (b->stride / 4) + (size_t)x];
}
#endif
