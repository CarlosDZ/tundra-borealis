#define _GNU_SOURCE
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
	running = 0;
}

static const struct xdg_toplevel_listener toplevel_listener = {
    .configure = on_toplevel_configure,
    .close = on_toplevel_close,
};

static int open_window(struct wl_display *display, const char *app_id) {
	if (!compositor || !shm || !wm_base) {
		fprintf(stderr, "missing globals for a window\n");
		return 1;
	}
	xdg_wm_base_add_listener(wm_base, &wm_base_listener, NULL);
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
	if (window)
		return open_window(display, argv[2]);

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
