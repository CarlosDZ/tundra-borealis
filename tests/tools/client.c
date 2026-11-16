#define _GNU_SOURCE
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
static struct wl_surface *surface;
static struct xdg_surface *xsurface;
static int cfg_w, cfg_h, min_w, min_h, running = 1;

static void on_global(void *data, struct wl_registry *registry, uint32_t name,
                      const char *interface, uint32_t version) {
	(void)data;
	(void)version;
	for (size_t i = 0; i < WANTED; i++)
		if (strcmp(interface, wanted[i]) == 0)
			seen[i] = 1;
	if (strcmp(interface, "wl_compositor") == 0)
		compositor =
		    wl_registry_bind(registry, name, &wl_compositor_interface, 4);
	else if (strcmp(interface, "wl_shm") == 0)
		shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	else if (strcmp(interface, "xdg_wm_base") == 0)
		wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
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

static void say(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	putchar('\n');
	fflush(stdout);
}

static void kb_keymap(void *d, struct wl_keyboard *k, uint32_t format, int fd,
                      uint32_t size) {
	(void)d;
	(void)k;
	(void)format;
	(void)size;
	close(fd);
}

static void kb_enter(void *d, struct wl_keyboard *k, uint32_t serial,
                     struct wl_surface *s, struct wl_array *keys) {
	(void)d;
	(void)k;
	(void)serial;
	(void)s;
	(void)keys;
	say("kbd-enter");
}

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

static void on_surface_configure(void *data, struct xdg_surface *s,
                                 uint32_t serial) {
	(void)data;
	xdg_surface_ack_configure(s, serial);
	int w = cfg_w > 0 ? cfg_w : DEFAULT_W, h = cfg_h > 0 ? cfg_h : DEFAULT_H;
	if (w < min_w)
		w = min_w;
	if (h < min_h)
		h = min_h;
	struct wl_buffer *b = draw(w, h);
	if (!b) {
		running = 0;
		return;
	}
	wl_surface_attach(surface, b, 0, 0);
	wl_surface_damage_buffer(surface, 0, 0, w, h);
	wl_surface_commit(surface);
	printf("configure %d %d\n", cfg_w, cfg_h);
	fflush(stdout);
}

static const struct xdg_surface_listener surface_listener = {
    .configure = on_surface_configure,
};

static void on_toplevel_configure(void *data, struct xdg_toplevel *t, int32_t w,
                                  int32_t h, struct wl_array *st) {
	(void)data;
	(void)t;
	(void)st;
	cfg_w = w;
	cfg_h = h;
}

static void on_toplevel_close(void *data, struct xdg_toplevel *t) {
	(void)data;
	(void)t;
	say("close");
	running = 0;
}

static const struct xdg_toplevel_listener toplevel_listener = {
    .configure = on_toplevel_configure,
    .close = on_toplevel_close,
};

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

static int open_window(struct wl_display *display, const char *app_id) {
	if (!compositor || !shm || !wm_base) {
		fprintf(stderr, "missing globals for a window\n");
		return 1;
	}
	xdg_wm_base_add_listener(wm_base, &wm_base_listener, NULL);
	if (seat)
		wl_seat_add_listener(seat, &seat_listener, NULL);
	surface = wl_compositor_create_surface(compositor);
	xsurface = xdg_wm_base_get_xdg_surface(wm_base, surface);
	xdg_surface_add_listener(xsurface, &surface_listener, NULL);
	struct xdg_toplevel *top = xdg_surface_get_toplevel(xsurface);
	xdg_toplevel_add_listener(top, &toplevel_listener, NULL);
	xdg_toplevel_set_app_id(top, app_id);
	if (min_w || min_h)
		xdg_toplevel_set_min_size(top, min_w, min_h);
	wl_surface_commit(surface);
	while (running && wl_display_dispatch(display) != -1)
		;
	return 0;
}

int main(int argc, char **argv) {
	int window = argc >= 3 && strcmp(argv[1], "window") == 0;
	if (window && argc == 5) {
		min_w = atoi(argv[3]);
		min_h = atoi(argv[4]);
	} else if (argc != 1 && !(window && argc == 3)) {
		fprintf(stderr, "usage: client [window app_id [min_w min_h]]\n");
		return 2;
	}

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
		return open_window(display, argv[2]);
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
