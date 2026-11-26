#define _GNU_SOURCE
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#include "xdg-shell-client-protocol.h"

#define DEFAULT_W 320
#define DEFAULT_H 240

static const char *wanted[] = {"wl_compositor", "wl_subcompositor", "wl_shm",
                               "wl_data_device_manager", "xdg_wm_base"};
#define WANTED (sizeof(wanted) / sizeof(wanted[0]))

static int seen[WANTED];
static struct wl_compositor *compositor;
static struct wl_seat *seat;
static struct wl_shm *shm;
static struct xdg_wm_base *wm_base;
static struct wl_data_device_manager *ddm;
static struct wl_data_device *data_device;
static const char *copy_text;
static int want_paste, copied;
struct win {
	const char *tag;
	struct wl_surface *surface;
	struct xdg_surface *xsurface;
	struct xdg_toplevel *top;
	int cfg_w, cfg_h, min_w, min_h, want_w, want_h, drawn_w, drawn_h;
};

static struct win main_win = {.tag = ""}, dialog_win = {.tag = "dlg-"};
static const char *app;
static int want_full, want_dialog, want_menu, want_echo, listing, running = 1;
static struct wl_surface *menu_surface;

static void say(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	putchar('\n');
	fflush(stdout);
}

static void on_global(void *data, struct wl_registry *registry, uint32_t name,
                      const char *interface, uint32_t version) {
	(void)data;
	(void)version;
	for (size_t i = 0; i < WANTED; i++)
		if (strcmp(interface, wanted[i]) == 0)
			seen[i] = 1;
	if (listing)
		say("global %s", interface);
	if (strcmp(interface, "wl_compositor") == 0)
		compositor =
		    wl_registry_bind(registry, name, &wl_compositor_interface, 4);
	else if (strcmp(interface, "wl_shm") == 0)
		shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	else if (strcmp(interface, "xdg_wm_base") == 0)
		wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
	else if (strcmp(interface, "wl_data_device_manager") == 0)
		ddm = wl_registry_bind(registry, name,
		                       &wl_data_device_manager_interface, 1);
	else if (strcmp(interface, "wl_seat") == 0 && !seat)
		seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
}

static void on_global_remove(void *data, struct wl_registry *registry,
                             uint32_t name) {
	(void)data;
	(void)registry;
	(void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = on_global,
    .global_remove = on_global_remove,
};

static void kb_keymap(void *d, struct wl_keyboard *k, uint32_t format, int fd,
                      uint32_t size) {
	(void)d;
	(void)k;
	(void)format;
	(void)size;
	close(fd);
}

static void source_target(void *d, struct wl_data_source *src,
                          const char *mime) {
	(void)d;
	(void)src;
	(void)mime;
}

static void source_send(void *d, struct wl_data_source *src, const char *mime,
                        int fd) {
	(void)d;
	(void)src;
	(void)mime;
	if (write(fd, copy_text, strlen(copy_text)) < 0)
		perror("write error");
	close(fd);
}

static void source_cancelled(void *d, struct wl_data_source *src) {
	(void)d;
	wl_data_source_destroy(src);
	say("cancelled");
}

static const struct wl_data_source_listener source_listener = {
    .target = source_target,
    .send = source_send,
    .cancelled = source_cancelled,
};

static void kb_enter(void *d, struct wl_keyboard *k, uint32_t serial,
                     struct wl_surface *s, struct wl_array *keys) {
	(void)d;
	(void)k;
	(void)keys;
	say("kbd-enter%s", s == dialog_win.surface ? " dlg" : "");
	if (copy_text && data_device && !copied) {
		struct wl_data_source *src =
		    wl_data_device_manager_create_data_source(ddm);
		wl_data_source_add_listener(src, &source_listener, NULL);
		wl_data_source_offer(src, "text/plain;charset=utf-8");
		wl_data_device_set_selection(data_device, src, serial);
		copied = 1;
		say("copied");
	}
}

static void dd_offer(void *d, struct wl_data_device *dd,
                     struct wl_data_offer *o) {
	(void)d;
	(void)dd;
	(void)o;
}

static void dd_enter(void *d, struct wl_data_device *dd, uint32_t serial,
                     struct wl_surface *s, wl_fixed_t x, wl_fixed_t y,
                     struct wl_data_offer *o) {
	(void)d;
	(void)dd;
	(void)serial;
	(void)s;
	(void)x;
	(void)y;
	(void)o;
}

static void dd_leave(void *d, struct wl_data_device *dd) {
	(void)d;
	(void)dd;
}

static void dd_motion(void *d, struct wl_data_device *dd, uint32_t time,
                      wl_fixed_t x, wl_fixed_t y) {
	(void)d;
	(void)dd;
	(void)time;
	(void)x;
	(void)y;
}

static void dd_selection(void *d, struct wl_data_device *dd,
                         struct wl_data_offer *o) {
	(void)d;
	(void)dd;
	int fds[2];
	if (!want_paste || !o || pipe2(fds, O_CLOEXEC) < 0)
		return;
	wl_data_offer_receive(o, "text/plain;charset=utf-8", fds[1]);
	close(fds[1]);
	wl_display_flush(wl_proxy_get_display((struct wl_proxy *)dd));
	char buf[256];
	size_t n = 0;
	ssize_t r;
	while (n < sizeof(buf) - 1 &&
	       (r = read(fds[0], buf + n, sizeof(buf) - 1 - n)) > 0)
		n += (size_t)r;
	close(fds[0]);
	buf[n] = 0;
	say("pasted %s", buf);
	wl_data_offer_destroy(o);
}

static const struct wl_data_device_listener dd_listener = {
    .data_offer = dd_offer,
    .enter = dd_enter,
    .leave = dd_leave,
    .motion = dd_motion,
    .drop = dd_leave,
    .selection = dd_selection,
};

static void kb_leave(void *d, struct wl_keyboard *k, uint32_t serial,
                     struct wl_surface *s) {
	(void)d;
	(void)k;
	(void)serial;
	(void)s;
	say("kbd-leave");
}

static void kb_key(void *d, struct wl_keyboard *k, uint32_t serial,
                   uint32_t time, uint32_t key, uint32_t state) {
	(void)d;
	(void)k;
	(void)serial;
	(void)time;
	say("key %u %u", key, state);
	if (want_echo && state == WL_KEYBOARD_KEY_STATE_PRESSED && main_win.surface)
		wl_surface_commit(main_win.surface);
}

static void kb_modifiers(void *d, struct wl_keyboard *k, uint32_t serial,
                         uint32_t dep, uint32_t lat, uint32_t lock,
                         uint32_t group) {
	(void)d;
	(void)k;
	(void)serial;
	(void)dep;
	(void)lat;
	(void)lock;
	(void)group;
}

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = kb_keymap,
    .enter = kb_enter,
    .leave = kb_leave,
    .key = kb_key,
    .modifiers = kb_modifiers,
};

static void ptr_enter(void *d, struct wl_pointer *p, uint32_t serial,
                      struct wl_surface *s, wl_fixed_t x, wl_fixed_t y) {
	(void)d;
	(void)p;
	(void)serial;
	(void)s;
	(void)x;
	(void)y;
	say("ptr-enter");
}

static void ptr_leave(void *d, struct wl_pointer *p, uint32_t serial,
                      struct wl_surface *s) {
	(void)d;
	(void)p;
	(void)serial;
	(void)s;
	say("ptr-leave");
}

static void ptr_motion(void *d, struct wl_pointer *p, uint32_t time,
                       wl_fixed_t x, wl_fixed_t y) {
	(void)d;
	(void)p;
	(void)time;
	(void)x;
	(void)y;
}

static void ptr_button(void *d, struct wl_pointer *p, uint32_t serial,
                       uint32_t time, uint32_t button, uint32_t state) {
	(void)d;
	(void)p;
	(void)serial;
	(void)time;
	say("button %u %u", button, state);
}

static void ptr_axis(void *d, struct wl_pointer *p, uint32_t time,
                     uint32_t axis, wl_fixed_t value) {
	(void)d;
	(void)p;
	(void)time;
	say("axis %u %.0f", axis, wl_fixed_to_double(value));
}

static const struct wl_pointer_listener pointer_listener = {
    .enter = ptr_enter,
    .leave = ptr_leave,
    .motion = ptr_motion,
    .button = ptr_button,
    .axis = ptr_axis,
};

static void seat_caps(void *d, struct wl_seat *s, uint32_t caps) {
	(void)d;
	static struct wl_keyboard *kb;
	static struct wl_pointer *ptr;
	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !kb) {
		kb = wl_seat_get_keyboard(s);
		wl_keyboard_add_listener(kb, &keyboard_listener, NULL);
	}
	if ((caps & WL_SEAT_CAPABILITY_POINTER) && !ptr) {
		ptr = wl_seat_get_pointer(s);
		wl_pointer_add_listener(ptr, &pointer_listener, NULL);
	}
}

static const struct wl_seat_listener seat_listener = {.capabilities =
                                                          seat_caps};

static void on_ping(void *data, struct xdg_wm_base *b, uint32_t serial) {
	(void)data;
	xdg_wm_base_pong(b, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {.ping = on_ping};

static void on_release(void *data, struct wl_buffer *buffer) {
	(void)data;
	wl_buffer_destroy(buffer);
}

static const struct wl_buffer_listener buffer_listener = {.release =
                                                              on_release};

static struct wl_buffer *draw(int w, int h) {
	int stride = w * 4, size = stride * h;
	int fd = memfd_create("client", MFD_CLOEXEC);
	if (fd < 0 || ftruncate(fd, size) < 0)
		return NULL;
	uint32_t *px =
	    mmap(NULL, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (px == MAP_FAILED) {
		close(fd);
		return NULL;
	}
	for (int i = 0; i < w * h; i++)
		px[i] = 0xff4c7899;
	munmap(px, (size_t)size);
	struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
	struct wl_buffer *b = wl_shm_pool_create_buffer(pool, 0, w, h, stride,
	                                                WL_SHM_FORMAT_ARGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);
	wl_buffer_add_listener(b, &buffer_listener, NULL);
	return b;
}

static int draw_into(struct wl_surface *surface, int w, int h) {
	struct wl_buffer *b = draw(w, h);
	if (!b)
		return -1;
	wl_surface_attach(surface, b, 0, 0);
	wl_surface_damage_buffer(surface, 0, 0, w, h);
	wl_surface_commit(surface);
	return 0;
}

static void on_menu_configure(void *data, struct xdg_surface *s,
                              uint32_t serial) {
	(void)data;
	xdg_surface_ack_configure(s, serial);
	if (draw_into(menu_surface, 150, 100) < 0)
		running = 0;
}

static const struct xdg_surface_listener menu_surface_listener = {
    .configure = on_menu_configure,
};

static void on_popup_configure(void *data, struct xdg_popup *p, int32_t x,
                               int32_t y, int32_t w, int32_t h) {
	(void)data;
	(void)p;
	say("popup %d %d %d %d", x, y, w, h);
}

static void on_popup_done(void *data, struct xdg_popup *p) {
	(void)data;
	(void)p;
	say("popup-done");
}

static const struct xdg_popup_listener popup_listener = {
    .configure = on_popup_configure,
    .popup_done = on_popup_done,
};

static void open_menu(void) {
	struct xdg_positioner *pos = xdg_wm_base_create_positioner(wm_base);
	xdg_positioner_set_size(pos, 150, 100);
	xdg_positioner_set_anchor_rect(pos, main_win.drawn_w - 1,
	                               main_win.drawn_h - 1, 1, 1);
	xdg_positioner_set_anchor(pos, XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT);
	xdg_positioner_set_gravity(pos, XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT);
	xdg_positioner_set_constraint_adjustment(
	    pos, XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X |
	             XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y |
	             XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X |
	             XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y);
	menu_surface = wl_compositor_create_surface(compositor);
	struct xdg_surface *xs = xdg_wm_base_get_xdg_surface(wm_base, menu_surface);
	xdg_surface_add_listener(xs, &menu_surface_listener, NULL);
	struct xdg_popup *p = xdg_surface_get_popup(xs, main_win.xsurface, pos);
	xdg_popup_add_listener(p, &popup_listener, NULL);
	xdg_positioner_destroy(pos);
	wl_surface_commit(menu_surface);
}

static void open_toplevel(struct win *w, const char *app_id,
                          struct xdg_toplevel *parent);

static void on_surface_configure(void *data, struct xdg_surface *s,
                                 uint32_t serial) {
	struct win *win = data;
	xdg_surface_ack_configure(s, serial);
	int w = win->cfg_w > 0    ? win->cfg_w
	        : win->want_w > 0 ? win->want_w
	                          : DEFAULT_W;
	int h = win->cfg_h > 0    ? win->cfg_h
	        : win->want_h > 0 ? win->want_h
	                          : DEFAULT_H;
	if (w < win->min_w)
		w = win->min_w;
	if (h < win->min_h)
		h = win->min_h;
	int first = win->drawn_w == 0;
	if (draw_into(win->surface, w, h) < 0) {
		running = 0;
		return;
	}
	win->drawn_w = w;
	win->drawn_h = h;
	say("%sconfigure %d %d", win->tag, win->cfg_w, win->cfg_h);
	if (win != &main_win || !first)
		return;
	if (want_dialog) {
		char id[256];
		snprintf(id, sizeof(id), "%s-dialog", app);
		open_toplevel(&dialog_win, id, main_win.top);
	}
	if (want_menu)
		open_menu();
}

static const struct xdg_surface_listener surface_listener = {
    .configure = on_surface_configure,
};

static void on_toplevel_configure(void *data, struct xdg_toplevel *t, int32_t w,
                                  int32_t h, struct wl_array *st) {
	struct win *win = data;
	(void)t;
	(void)st;
	win->cfg_w = w;
	win->cfg_h = h;
}

static void on_toplevel_close(void *data, struct xdg_toplevel *t) {
	struct win *win = data;
	if (win == &main_win) {
		say("close");
		running = 0;
		return;
	}
	say("close dlg");
	xdg_toplevel_destroy(t);
	xdg_surface_destroy(win->xsurface);
	wl_surface_destroy(win->surface);
	win->surface = NULL;
}

static const struct xdg_toplevel_listener toplevel_listener = {
    .configure = on_toplevel_configure,
    .close = on_toplevel_close,
};

static void open_toplevel(struct win *w, const char *app_id,
                          struct xdg_toplevel *parent) {
	w->surface = wl_compositor_create_surface(compositor);
	w->xsurface = xdg_wm_base_get_xdg_surface(wm_base, w->surface);
	xdg_surface_add_listener(w->xsurface, &surface_listener, w);
	w->top = xdg_surface_get_toplevel(w->xsurface);
	xdg_toplevel_add_listener(w->top, &toplevel_listener, w);
	xdg_toplevel_set_app_id(w->top, app_id);
	if (parent)
		xdg_toplevel_set_parent(w->top, parent);
	if (w->min_w || w->min_h)
		xdg_toplevel_set_min_size(w->top, w->min_w, w->min_h);
	if (w == &main_win && want_full)
		xdg_toplevel_set_fullscreen(w->top, NULL);
	wl_surface_commit(w->surface);
}

static void inherited(void) {
	char line[128];
	FILE *f = fopen("/proc/self/status", "r");
	if (!f)
		return;
	while (fgets(line, sizeof(line), f))
		if (strncmp(line, "SigBlk:", 7) == 0 ||
		    strncmp(line, "SigIgn:", 7) == 0)
			fputs(line, stdout);
	fclose(f);
	fflush(stdout);
}

static int open_window(struct wl_display *display) {
	if (!compositor || !shm || !wm_base) {
		fprintf(stderr, "missing globals for a window\n");
		return 1;
	}
	xdg_wm_base_add_listener(wm_base, &wm_base_listener, NULL);
	if (seat)
		wl_seat_add_listener(seat, &seat_listener, NULL);
	if (seat && ddm) {
		data_device = wl_data_device_manager_get_data_device(ddm, seat);
		wl_data_device_add_listener(data_device, &dd_listener, NULL);
	}
	open_toplevel(&main_win, app, NULL);
	while (running && wl_display_dispatch(display) != -1)
		;
	return 0;
}

static int option(const char *o) {
	if (sscanf(o, "min=%dx%d", &main_win.min_w, &main_win.min_h) == 2)
		return 0;
	if (sscanf(o, "dialog=%dx%d", &dialog_win.want_w, &dialog_win.want_h) ==
	    2) {
		want_dialog = 1;
		return 0;
	}
	if (strcmp(o, "full") == 0) {
		want_full = 1;
		return 0;
	}
	if (strcmp(o, "menu") == 0) {
		want_menu = 1;
		return 0;
	}
	if (strncmp(o, "copy=", 5) == 0) {
		copy_text = o + 5;
		return 0;
	}
	if (strcmp(o, "paste") == 0) {
		want_paste = 1;
		return 0;
	}
	if (strcmp(o, "echo") == 0) {
		want_echo = 1;
		return 0;
	}
	return -1;
}

int main(int argc, char **argv) {
	int window = argc >= 3 && strcmp(argv[1], "window") == 0;
	listing = argc == 2 && strcmp(argv[1], "list") == 0;
	int bad = argc != 1 && !window && !listing;
	for (int i = 3; window && i < argc; i++)
		bad |= option(argv[i]) < 0;
	if (bad) {
		fprintf(stderr, "usage: client [list | window app_id [min=WxH] [full] "
		                "[dialog=WxH] [menu] [echo] [copy=TEXT] [paste]]\n");
		return 2;
	}
	if (window)
		app = argv[2];

	struct wl_display *display = wl_display_connect(NULL);
	if (!display) {
		fprintf(stderr, "cannot connect to the compositor\n");
		return 1;
	}
	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	if (wl_display_roundtrip(display) < 0) {
		fprintf(stderr, "the compositor closed the connection\n");
		return 1;
	}
	if (window) {
		inherited();
		return open_window(display);
	}
	if (listing) {
		say("listed");
		while (wl_display_dispatch(display) != -1)
			;
		return 0;
	}

	int rc = 0;
	for (size_t i = 0; i < WANTED; i++) {
		if (!seen[i]) {
			fprintf(stderr, "missing global %s\n", wanted[i]);
			rc = 1;
		}
	}
	wl_display_disconnect(display);
	return rc;
}
