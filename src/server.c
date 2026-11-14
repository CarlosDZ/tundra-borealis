#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/util/log.h>

#include "server.h"

#define COMPOSITOR_VERSION 6

static int on_signal(int sig, void *data) {
	(void)sig;
	wl_display_terminate(data);
	return 0;
}

int server_run(void) {
	struct wlr_backend *backend = NULL;
	struct wlr_renderer *renderer = NULL;
	struct wlr_allocator *allocator = NULL;
	struct wl_event_source *sigterm = NULL, *sigint = NULL;
	int rc = -1;

	wlr_log_init(WLR_ERROR, NULL);
	struct wl_display *display = wl_display_create();
	if (!display) {
		fprintf(stderr, "cannot create the display\n");
		return -1;
	}
	struct wl_event_loop *loop = wl_display_get_event_loop(display);

	backend = wlr_backend_autocreate(loop, NULL);
	if (!backend) {
		fprintf(stderr, "cannot create the backend\n");
		goto out;
	}
	renderer = wlr_renderer_autocreate(backend);
	if (!renderer) {
		fprintf(stderr, "cannot create the renderer\n");
		goto out;
	}
	if (!wlr_renderer_init_wl_display(renderer, display)) {
		fprintf(stderr, "cannot offer buffers to clients\n");
		goto out;
	}
	allocator = wlr_allocator_autocreate(backend, renderer);
	if (!allocator) {
		fprintf(stderr, "cannot create the allocator\n");
		goto out;
	}

	if (!wlr_compositor_create(display, COMPOSITOR_VERSION, renderer) ||
	    !wlr_subcompositor_create(display) ||
	    !wlr_data_device_manager_create(display)) {
		fprintf(stderr, "cannot create the core globals\n");
		goto out;
	}

	const char *socket = wl_display_add_socket_auto(display);
	if (!socket) {
		fprintf(stderr, "cannot open a wayland socket\n");
		goto out;
	}
	if (!wlr_backend_start(backend)) {
		fprintf(stderr, "cannot start the backend\n");
		goto out;
	}
	setenv("WAYLAND_DISPLAY", socket, 1);

	sigterm = wl_event_loop_add_signal(loop, SIGTERM, on_signal, display);
	sigint = wl_event_loop_add_signal(loop, SIGINT, on_signal, display);
	if (!sigterm || !sigint) {
		fprintf(stderr, "cannot watch the signals\n");
		goto out;
	}

	fprintf(stderr, "listening on %s\n", socket);
	wl_display_run(display);
	rc = 0;

out:
	if (sigterm)
		wl_event_source_remove(sigterm);
	if (sigint)
		wl_event_source_remove(sigint);
	wl_display_destroy_clients(display);
	if (allocator)
		wlr_allocator_destroy(allocator);
	if (renderer)
		wlr_renderer_destroy(renderer);
	if (backend)
		wlr_backend_destroy(backend);
	wl_display_destroy(display);
	return rc;
}
