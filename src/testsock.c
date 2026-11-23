#ifdef TUNDRA_BOREALIS_TEST
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/multi.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/interfaces/wlr_pointer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>

#include "fill.h"
#include "header.h"
#include "input.h"
#include "outputs.h"
#include "testsock.h"
#include "xdg.h"

#define TESTSOCK_NAME "borealis-test"
#define TESTSOCK_LINE 256

static const struct wlr_keyboard_impl keyboard_impl = {.name = "test"};
static const struct wlr_pointer_impl pointer_impl = {.name = "test"};
static struct wlr_keyboard keyboard;
static struct wlr_pointer pointer;
static int devices;

static int listen_fd = -1;
static struct wl_event_source *source;
static char path[sizeof(((struct sockaddr_un *)0)->sun_path)];

static char type_of(struct server *s, unsigned int id) {
	int c = desk_find(&s->desk, id);
	if (c < 0)
		return '-';
	if (s->desk.fixed[c])
		return 'F';
	for (int i = 0; i < s->desk.canvas[c].n; i++)
		if (s->desk.canvas[c].w[i].id == id)
			return s->desk.canvas[c].w[i].type;
	return '-';
}

static const char *app_of(struct window *w) {
	const char *app = w ? w->toplevel->app_id : NULL;
	return app && *app ? app : "-";
}

static void list_windows(struct server *s, FILE *f) {
	struct window *w;
	wl_list_for_each(w, &s->windows, link) {
		struct window *root = xdg_root(w);
		if (!root->placed || !w->toplevel->base->surface->mapped)
			continue;
		struct grid_pos p = desk_pos(desk_find(&s->desk, root->id));
		struct wlr_box g = w->toplevel->base->geometry;
		int sw = g.width < w->box.width ? g.width : w->box.width;
		int sh = g.height < w->box.height ? g.height : w->box.height;
		char type = w->parent ? 'D' : type_of(s, w->id);
		struct wlr_box o;
		outputs_canvas_box(s, desk_find(&s->desk, root->id), &o);
		fprintf(f, "window %u %s %d%d %c %d %d %d %d %d %d %d\n", w->id,
		        app_of(w), p.h, p.d, type, o.x + w->box.x, o.y + w->box.y,
		        w->box.width, w->box.height, sw, sh, w->fullscreen);
	}
}

static void show_output(struct server *s, FILE *f, const char *role,
                        struct wlr_output *o) {
	struct wlr_box b = {0};
	if (o)
		wlr_output_layout_get_box(s->output_layout, o, &b);
	fprintf(f, " %s %d %d %d %d", role, b.x, b.y, b.width, b.height);
}

static void show_screens(struct server *s, FILE *f) {
	static const char *modes[] = {"one", "mirror", "extended"};
	fprintf(f, "screens %s", modes[s->desk.grid.screens]);
	show_output(s, f, "main", outputs_main(s));
	show_output(s, f, "second", outputs_second(s));
	fprintf(f, "\n");
}

static void show_header(struct server *s, FILE *f, const char *role,
                        struct wlr_output *o) {
	struct header *h = o ? outputs_header(s, o) : NULL;
	struct header_info i;
	if (!h)
		return;
	header_info(h, &i);
	fprintf(f, "header %s %d %d %d %d %d %d %d\t%s\t%s\n", role, i.x, i.y, i.w,
	        i.h, i.line, i.shown, i.font, i.place, i.clock);
}

static void show_colors(struct server *s, FILE *f) {
	const struct colors *c = &s->colors;
	fprintf(f,
	        "colors background %08x surface %08x text %08x dim %08x border "
	        "%08x focus %08x accent %08x alert %08x warn %08x ok %08x fallen "
	        "%08x\n",
	        c->background, c->surface, c->text, c->dim, c->border, c->focus,
	        c->accent, c->alert, c->warn, c->ok, c->fallen);
}

static int ink(struct server *s, int x, int w) {
	struct header *h = outputs_header(s, outputs_main(s));
	struct header_info i;
	int n = 0;
	if (!h)
		return -1;
	header_info(h, &i);
	uint32_t back = header_pixel(h, 0, 0);
	for (int y = 0; y < i.h; y++)
		for (int c = x; c < x + w && c < i.w; c++)
			n += header_pixel(h, c, y) != back;
	return n;
}

static int count(struct server *s, uint32_t argb) {
	struct header *h = outputs_header(s, outputs_main(s));
	struct header_info i;
	int n = 0;
	if (!h)
		return -1;
	header_info(h, &i);
	for (int y = 0; y < i.h; y++)
		for (int x = 0; x < i.w; x++)
			n += header_pixel(h, x, y) == argb;
	return n;
}

static void list_fills(struct server *s, FILE *f) {
	static const char *kinds[] = {"empty", "free", "fallen"};
	struct fill_info i;
	for (int c = 0; c < DESK_CANVASES; c++) {
		struct grid_pos p = desk_pos(c);
		int x, y;
		server_canvas_origin(s, c, &x, &y);
		for (int k = 0; fill_info(s, c, k, &i) == 0; k++)
			fprintf(f, "fill %d%d %s %s %d %d %d %d %d %d %d %d %u\n", p.h, p.d,
			        kinds[i.kind], i.name, x + i.r.x, y + i.r.y, i.r.w, i.r.h,
			        i.w, i.h, i.scale, i.in_view, i.frames);
	}
}

static void show_status(struct server *s, FILE *f) {
	const struct status *st = &s->status;
	fprintf(f, "status battery %d %s %d brightness %d %s %d\n",
	        st->power.battery, st->battery_state ? st->battery_state : "-",
	        st->battery.art != NULL, st->power.brightness,
	        st->brightness_state ? st->brightness_state : "-",
	        st->brightness.art != NULL);
}

static void find_headless(struct wlr_backend *b, void *data) {
	if (wlr_backend_is_headless(b))
		*(struct wlr_backend **)data = b;
}

static int plug(struct server *s) {
	struct wlr_backend *headless = NULL;
	wlr_multi_for_each_backend(s->backend, find_headless, &headless);
	return headless && wlr_headless_add_output(headless, 1280, 720) ? 0 : -1;
}

static int unplug(struct server *s) {
	struct wlr_output *o = outputs_second(s);
	if (!o)
		return -1;
	wlr_output_destroy(o);
	return 0;
}

static void list_fixed(struct server *s, FILE *f) {
	for (int c = 0; c < DESK_CANVASES; c++) {
		if (!s->desk.fixed[c])
			continue;
		struct grid_pos p = desk_pos(c);
		for (int i = 0; i < s->desk.fixed[c]->n; i++) {
			struct fixed_run *r = &s->runs[c][i];
			fprintf(f, "fixed %d%d %s %s %u\n", p.h, p.d,
			        s->desk.fixed[c]->p[i].name,
			        r->covered  ? "covered"
			        : r->client ? "running"
			                    : "starting",
			        s->desk.pinned[c][i]);
		}
	}
}

static void list_popups(struct server *s, FILE *f) {
	struct popup *p;
	wl_list_for_each(p, &s->popups, link) {
		int x, y;
		if (!p->xdg_popup->base->surface->mapped ||
		    !wlr_scene_node_coords(&p->tree->node, &x, &y))
			continue;
		struct wlr_box g = p->xdg_popup->base->geometry;
		fprintf(f, "popup %s %d %d %d %d\n", app_of(xdg_popup_owner(p)), x, y,
		        g.width, g.height);
	}
}

static uint32_t now_msec(void) {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint32_t)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}

static void show_focus(struct server *s, FILE *f) {
	struct grid_pos p = s->desk.grid.focus;
	fprintf(f, "focus %u %s %d%d\n", s->active ? s->active->id : 0,
	        app_of(s->active), p.h, p.d);
}

static void frame(void) {
	wl_signal_emit_mutable(&pointer.events.frame, &pointer);
}

static int key(int code, const char *how) {
	struct wlr_keyboard_key_event e = {.time_msec = now_msec(),
	                                   .keycode = (uint32_t)code,
	                                   .update_state = true};
	if (strcmp(how, "down") == 0)
		e.state = WL_KEYBOARD_KEY_STATE_PRESSED;
	else if (strcmp(how, "up") == 0)
		e.state = WL_KEYBOARD_KEY_STATE_RELEASED;
	else
		return -1;
	wlr_keyboard_notify_key(&keyboard, &e);
	return 0;
}

static void move(struct server *s, double x, double y) {
	struct wlr_pointer_motion_event e = {
	    .pointer = &pointer,
	    .time_msec = now_msec(),
	    .delta_x = x - s->cursor->x,
	    .delta_y = y - s->cursor->y,
	};
	e.unaccel_dx = e.delta_x;
	e.unaccel_dy = e.delta_y;
	wl_signal_emit_mutable(&pointer.events.motion, &e);
	frame();
}

static int button(const char *how) {
	struct wlr_pointer_button_event e = {
	    .pointer = &pointer, .time_msec = now_msec(), .button = BTN_LEFT};
	if (strcmp(how, "down") == 0)
		e.state = WL_POINTER_BUTTON_STATE_PRESSED;
	else if (strcmp(how, "up") == 0)
		e.state = WL_POINTER_BUTTON_STATE_RELEASED;
	else
		return -1;
	wl_signal_emit_mutable(&pointer.events.button, &e);
	frame();
	return 0;
}

static void scroll(double value) {
	struct wlr_pointer_axis_event e = {
	    .pointer = &pointer,
	    .time_msec = now_msec(),
	    .source = WL_POINTER_AXIS_SOURCE_FINGER,
	    .orientation = WL_POINTER_AXIS_VERTICAL_SCROLL,
	    .relative_direction = WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL,
	    .delta = value,
	};
	wl_signal_emit_mutable(&pointer.events.axis, &e);
	frame();
}

static void swipe(unsigned int fingers, double dx, double dy) {
	uint32_t t = now_msec();
	struct wlr_pointer_swipe_begin_event b = {&pointer, t, fingers};
	struct wlr_pointer_swipe_update_event u = {&pointer, t, fingers, dx, dy};
	struct wlr_pointer_swipe_end_event e = {&pointer, t, false};
	wl_signal_emit_mutable(&pointer.events.swipe_begin, &b);
	wl_signal_emit_mutable(&pointer.events.swipe_update, &u);
	wl_signal_emit_mutable(&pointer.events.swipe_end, &e);
}

static int run(struct server *s, const char *line, FILE *f) {
	char how[16];
	int code;
	double x, y;
	unsigned int fingers;
	if (strcmp(line, "windows") == 0) {
		list_windows(s, f);
		return 0;
	}
	if (strcmp(line, "screens") == 0) {
		show_screens(s, f);
		return 0;
	}
	if (strcmp(line, "plug") == 0)
		return plug(s);
	if (strcmp(line, "unplug") == 0)
		return unplug(s);
	if (strcmp(line, "fixed") == 0) {
		list_fixed(s, f);
		return 0;
	}
	if (strcmp(line, "popups") == 0) {
		list_popups(s, f);
		return 0;
	}
	if (strcmp(line, "focus") == 0) {
		show_focus(s, f);
		return 0;
	}
	if (strcmp(line, "header") == 0) {
		show_header(s, f, "main", outputs_main(s));
		show_header(s, f, "second", outputs_second(s));
		return 0;
	}
	unsigned int argb;
	if (sscanf(line, "count %x", &argb) == 1) {
		fprintf(f, "count %d\n", count(s, argb));
		return 0;
	}
	if (strcmp(line, "fills") == 0) {
		list_fills(s, f);
		return 0;
	}
	if (strcmp(line, "status") == 0) {
		show_status(s, f);
		return 0;
	}
	if (strcmp(line, "uevent") == 0) {
		status_update(s);
		return 0;
	}
	if (strcmp(line, "colors") == 0) {
		show_colors(s, f);
		return 0;
	}
	if (sscanf(line, "ink %lf %lf", &x, &y) == 2) {
		fprintf(f, "ink %d\n", ink(s, (int)x, (int)y));
		return 0;
	}
	if (sscanf(line, "pixel %lf %lf", &x, &y) == 2) {
		fprintf(
		    f, "pixel %08x\n",
		    header_pixel(outputs_header(s, outputs_main(s)), (int)x, (int)y));
		return 0;
	}
	if (strcmp(line, "pointer") == 0) {
		fprintf(f, "pointer %.0f %.0f %d\n", s->cursor->x, s->cursor->y,
		        s->pointer_locked);
		return 0;
	}
	if (sscanf(line, "key %d %15s", &code, how) == 2)
		return key(code, how);
	if (sscanf(line, "move %lf %lf", &x, &y) == 2) {
		move(s, x, y);
		return 0;
	}
	if (sscanf(line, "button %15s", how) == 1)
		return button(how);
	if (sscanf(line, "scroll %lf", &x) == 1) {
		scroll(x);
		return 0;
	}
	if (sscanf(line, "swipe %u %lf %lf", &fingers, &x, &y) == 3) {
		swipe(fingers, x, y);
		return 0;
	}
	return -1;
}

static void answer(struct server *s, int fd) {
	char line[TESTSOCK_LINE];
	size_t len = 0;
	while (len < sizeof(line) - 1) {
		ssize_t r = read(fd, line + len, 1);
		if (r <= 0 || line[len] == '\n')
			break;
		len++;
	}
	line[len] = '\0';

	int out = fcntl(fd, F_DUPFD_CLOEXEC, 0);
	FILE *f = out < 0 ? NULL : fdopen(out, "w");
	if (!f) {
		if (out >= 0)
			close(out);
		return;
	}
	if (run(s, line, f) == 0)
		fprintf(f, "ok\n");
	else
		fprintf(f, "error: %s\n", line);
	fclose(f);
}

static int on_accept(int fd, uint32_t mask, void *data) {
	(void)mask;
	int c = accept4(fd, NULL, NULL, SOCK_CLOEXEC);
	if (c < 0)
		return 0;
	struct timeval tv = {1, 0};
	setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	answer(data, c);
	close(c);
	return 0;
}

int testsock_init(struct server *s) {
	const char *dir = getenv("XDG_RUNTIME_DIR");
	struct sockaddr_un a = {.sun_family = AF_UNIX};
	if (!dir || snprintf(path, sizeof(path), "%s/%s", dir, TESTSOCK_NAME) >=
	                (int)sizeof(path)) {
		fprintf(stderr, "no room for the test socket\n");
		return -1;
	}
	memcpy(a.sun_path, path, sizeof(path));
	unlink(path);
	listen_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
	if (listen_fd < 0 ||
	    bind(listen_fd, (struct sockaddr *)&a, sizeof(a)) < 0 ||
	    listen(listen_fd, 8) < 0) {
		perror("test socket error");
		return -1;
	}
	source = wl_event_loop_add_fd(wl_display_get_event_loop(s->display),
	                              listen_fd, WL_EVENT_READABLE, on_accept, s);
	if (!source)
		return -1;
	wlr_keyboard_init(&keyboard, &keyboard_impl, "test-keyboard");
	wlr_pointer_init(&pointer, &pointer_impl, "test-pointer");
	input_add_device(s, &keyboard.base);
	input_add_device(s, &pointer.base);
	devices = 1;
	return 0;
}

void testsock_finish(void) {
	if (devices) {
		wlr_keyboard_finish(&keyboard);
		wlr_pointer_finish(&pointer);
		devices = 0;
	}
	if (source)
		wl_event_source_remove(source);
	if (listen_fd >= 0) {
		close(listen_fd);
		unlink(path);
	}
	source = NULL;
	listen_fd = -1;
}
#endif
