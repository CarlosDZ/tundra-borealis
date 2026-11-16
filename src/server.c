#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/util/log.h>

#include "input.h"
#include "server.h"
#include "testsock.h"
#include "xdg.h"

#define COMPOSITOR_VERSION 6
#define PREFS_FILE_MAX 65536

static const float background_color[4] = {0.09f, 0.10f, 0.12f, 1.0f};
static const float cover_color[4] = {0.35f, 0.12f, 0.12f, 1.0f};

struct output {
	struct wl_list link;
	struct server *server;
	struct wlr_output *wlr_output;
	struct wl_listener frame, request_state, destroy;
};

static struct output *main_output(struct server *s) {
	if (wl_list_empty(&s->outputs))
		return NULL;
	struct output *o = wl_container_of(s->outputs.next, o, link);
	return o;
}

static void arrange_fixed(struct server *s, int index) {
	const struct fixed_canvas *fc = s->desk.fixed[index];
	for (int i = 0; i < fc->n; i++) {
		struct fixed_run *r = &s->runs[index][i];
		struct layout_rect lr = fixed_rect(&fc->p[i], s->desk.w, s->desk.h);
		struct wlr_box b = {lr.x, lr.y, lr.w, lr.h};
		unsigned int id = s->desk.pinned[index][i];
		struct window *w = id ? xdg_window(s, id) : NULL;
		if (w && id != s->desk.fullscreen[index]) {
			xdg_place(w, index, b, id == s->desk.focus[index]);
			wlr_scene_node_raise_to_top(&w->tree->node);
		}
		if (!r->covered) {
			if (r->cover)
				wlr_scene_node_set_enabled(&r->cover->node, false);
			continue;
		}
		if (!r->cover)
			r->cover =
			    wlr_scene_rect_create(s->canvas[index], 0, 0, cover_color);
		if (!r->cover)
			continue;
		wlr_scene_node_set_position(&r->cover->node, b.x + XDG_GAP,
		                            b.y + XDG_GAP);
		wlr_scene_rect_set_size(r->cover, b.width - XDG_GAP,
		                        b.height - XDG_GAP);
		wlr_scene_node_set_enabled(&r->cover->node, true);
		wlr_scene_node_raise_to_top(&r->cover->node);
	}
}

void server_arrange(struct server *s, int index) {
	if (s->desk.fixed[index]) {
		arrange_fixed(s, index);
	} else {
		struct slots_place p[LAYOUT_MAX];
		struct layout l;
		int n = desk_place(&s->desk, index, p, &l);
		for (int i = 0; i < n; i++) {
			struct window *w = xdg_window(s, p[i].id);
			if (!w || p[i].id == s->desk.fullscreen[index])
				continue;
			struct wlr_box r = {p[i].r.x, p[i].r.y, p[i].r.w, p[i].r.h};
			xdg_place(w, index, r, p[i].id == s->desk.focus[index]);
		}
	}
	struct window *full = s->desk.fullscreen[index]
	                          ? xdg_window(s, s->desk.fullscreen[index])
	                          : NULL;
	if (full)
		xdg_fullscreen(full, index, s->out_w, s->out_h);
	for (int c = 0; c < DESK_CANVASES; c++)
		wlr_scene_node_set_enabled(&s->canvas[c]->node,
		                           grid_is_shown(&s->desk.grid, desk_pos(c)));
}

void server_canvas_origin(struct server *s, int index, int *x, int *y) {
	(void)s;
	(void)index;
	*x = 0;
	*y = 0;
}

void server_arrange_all(struct server *s) {
	for (int c = 0; c < DESK_CANVASES; c++)
		server_arrange(s, c);
}

static void update_size(struct server *s) {
	struct output *o = main_output(s);
	int w = 0, h = 0;
	if (o)
		wlr_output_effective_resolution(o->wlr_output, &w, &h);
	s->out_w = w;
	s->out_h = h;
	wlr_scene_rect_set_size(s->background, w, h);
	desk_resize(&s->desk, w - XDG_GAP, h - XDG_GAP);
	server_arrange_all(s);
}

static void output_frame(struct wl_listener *listener, void *data) {
	(void)data;
	struct output *o = wl_container_of(listener, o, frame);
	struct wlr_scene_output *so =
	    wlr_scene_get_scene_output(o->server->scene, o->wlr_output);
	if (!so)
		return;
	wlr_scene_output_commit(so, NULL);
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(so, &now);
}

static void output_request_state(struct wl_listener *listener, void *data) {
	struct output *o = wl_container_of(listener, o, request_state);
	const struct wlr_output_event_request_state *event = data;
	wlr_output_commit_state(o->wlr_output, event->state);
	update_size(o->server);
}

static void output_destroy(struct wl_listener *listener, void *data) {
	(void)data;
	struct output *o = wl_container_of(listener, o, destroy);
	struct server *s = o->server;
	wl_list_remove(&o->frame.link);
	wl_list_remove(&o->request_state.link);
	wl_list_remove(&o->destroy.link);
	wl_list_remove(&o->link);
	free(o);
	if (!s->stopping)
		update_size(s);
}

static void new_output(struct wl_listener *listener, void *data) {
	struct server *s = wl_container_of(listener, s, new_output);
	struct wlr_output *wlr_output = data;

	if (!wlr_output_init_render(wlr_output, s->allocator, s->renderer)) {
		fprintf(stderr, "%s: cannot render to it\n", wlr_output->name);
		return;
	}
	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, true);
	struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
	if (mode)
		wlr_output_state_set_mode(&state, mode);
	int ok = wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);
	if (!ok) {
		fprintf(stderr, "%s: cannot enable it\n", wlr_output->name);
		return;
	}

	struct output *o = calloc(1, sizeof(*o));
	if (!o) {
		perror("calloc error");
		return;
	}
	o->server = s;
	o->wlr_output = wlr_output;
	o->frame.notify = output_frame;
	wl_signal_add(&wlr_output->events.frame, &o->frame);
	o->request_state.notify = output_request_state;
	wl_signal_add(&wlr_output->events.request_state, &o->request_state);
	o->destroy.notify = output_destroy;
	wl_signal_add(&wlr_output->events.destroy, &o->destroy);
	wl_list_insert(s->outputs.prev, &o->link);

	struct wlr_output_layout_output *lo =
	    wlr_output_layout_add_auto(s->output_layout, wlr_output);
	struct wlr_scene_output *so = wlr_scene_output_create(s->scene, wlr_output);
	if (lo && so)
		wlr_scene_output_layout_add_output(s->scene_layout, lo, so);
	update_size(s);
}

static void load_prefs(struct server *s) {
	char path[4096];
	const char *base = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");
	int n;
	if (base && *base)
		n = snprintf(path, sizeof(path), "%s/tundra/borealis/windows", base);
	else if (home && *home)
		n = snprintf(path, sizeof(path), "%s/.config/tundra/borealis/windows",
		             home);
	else
		return;
	if (n < 0 || (size_t)n >= sizeof(path))
		return;

	int fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		if (errno != ENOENT)
			fprintf(stderr, "%s: %s\n", path, strerror(errno));
		return;
	}
	static char text[PREFS_FILE_MAX + 1];
	size_t len = 0;
	ssize_t r;
	while (len <= PREFS_FILE_MAX &&
	       (r = read(fd, text + len, PREFS_FILE_MAX + 1 - len)) > 0)
		len += (size_t)r;
	close(fd);
	if (len > PREFS_FILE_MAX) {
		fprintf(stderr, "%s: bigger than %d bytes, ignored\n", path,
		        PREFS_FILE_MAX);
		return;
	}
	char err[PREFS_ERR_MAX];
	if (prefs_parse(text, len, &s->prefs, err) < 0)
		fprintf(stderr, "%s: %s, ignored\n", path, err);
}

static int on_signal(int sig, void *data) {
	(void)sig;
	wl_display_terminate(data);
	return 0;
}

int server_run(void) {
	static struct server s;
	struct wl_event_source *sigterm = NULL, *sigint = NULL;
	int rc = -1;

	wlr_log_init(WLR_ERROR, NULL);
	wl_list_init(&s.outputs);
	wl_list_init(&s.windows);
	wl_list_init(&s.new_output.link);
	wl_list_init(&s.new_toplevel.link);
	wl_list_init(&s.new_popup.link);
	wl_list_init(&s.new_decoration.link);
	wl_list_init(&s.keyboards);
	desk_init(&s.desk, 0, 0);
	load_prefs(&s);
	clients_load(&s);

	s.display = wl_display_create();
	if (!s.display) {
		fprintf(stderr, "cannot create the display\n");
		return -1;
	}
	struct wl_event_loop *loop = wl_display_get_event_loop(s.display);

	s.backend = wlr_backend_autocreate(loop, &s.session);
	if (!s.backend) {
		fprintf(stderr, "cannot create the backend\n");
		goto out;
	}
	s.renderer = wlr_renderer_autocreate(s.backend);
	if (!s.renderer) {
		fprintf(stderr, "cannot create the renderer\n");
		goto out;
	}
	if (!wlr_renderer_init_wl_display(s.renderer, s.display)) {
		fprintf(stderr, "cannot offer buffers to clients\n");
		goto out;
	}
	s.allocator = wlr_allocator_autocreate(s.backend, s.renderer);
	if (!s.allocator) {
		fprintf(stderr, "cannot create the allocator\n");
		goto out;
	}
	if (!wlr_compositor_create(s.display, COMPOSITOR_VERSION, s.renderer) ||
	    !wlr_subcompositor_create(s.display) ||
	    !wlr_data_device_manager_create(s.display)) {
		fprintf(stderr, "cannot create the core globals\n");
		goto out;
	}

	s.output_layout = wlr_output_layout_create(s.display);
	s.scene = wlr_scene_create();
	if (!s.output_layout || !s.scene) {
		fprintf(stderr, "cannot create the scene\n");
		goto out;
	}
	s.scene_layout = wlr_scene_attach_output_layout(s.scene, s.output_layout);
	s.background =
	    wlr_scene_rect_create(&s.scene->tree, 0, 0, background_color);
	for (int c = 0; c < DESK_CANVASES; c++)
		s.canvas[c] = wlr_scene_tree_create(&s.scene->tree);
	s.limbo = wlr_scene_tree_create(&s.scene->tree);
	if (!s.scene_layout || !s.background || !s.canvas[DESK_CANVASES - 1] ||
	    !s.limbo) {
		fprintf(stderr, "cannot create the scene\n");
		goto out;
	}
	wlr_scene_node_set_enabled(&s.limbo->node, false);

	s.seat = wlr_seat_create(s.display, "seat0");
	if (!s.seat) {
		fprintf(stderr, "cannot create the seat\n");
		goto out;
	}
	xdg_init(&s);
	if (!s.xdg_shell || !s.decorations) {
		fprintf(stderr, "cannot create the window globals\n");
		goto out;
	}
	if (input_init(&s) < 0) {
		fprintf(stderr, "cannot create the cursor\n");
		goto out;
	}
	s.new_output.notify = new_output;
	wl_signal_add(&s.backend->events.new_output, &s.new_output);

	const char *socket = wl_display_add_socket_auto(s.display);
	if (!socket) {
		fprintf(stderr, "cannot open a wayland socket\n");
		goto out;
	}
	if (!wlr_backend_start(s.backend)) {
		fprintf(stderr, "cannot start the backend\n");
		goto out;
	}
	setenv("WAYLAND_DISPLAY", socket, 1);
	clients_start(&s);

	sigterm = wl_event_loop_add_signal(loop, SIGTERM, on_signal, s.display);
	sigint = wl_event_loop_add_signal(loop, SIGINT, on_signal, s.display);
	if (!sigterm || !sigint) {
		fprintf(stderr, "cannot watch the signals\n");
		goto out;
	}
	if (testsock_init(&s) < 0)
		goto out;

	fprintf(stderr, "listening on %s\n", socket);
	wl_display_run(s.display);
	rc = 0;

out:
	s.stopping = 1;
	clients_finish(&s);
	testsock_finish();
	if (sigterm)
		wl_event_source_remove(sigterm);
	if (sigint)
		wl_event_source_remove(sigint);
	wl_display_destroy_clients(s.display);
	wl_list_remove(&s.new_output.link);
	wl_list_remove(&s.new_toplevel.link);
	wl_list_remove(&s.new_popup.link);
	wl_list_remove(&s.new_decoration.link);
	input_finish(&s);
	if (s.scene)
		wlr_scene_node_destroy(&s.scene->tree.node);
	if (s.allocator)
		wlr_allocator_destroy(s.allocator);
	if (s.renderer)
		wlr_renderer_destroy(s.renderer);
	if (s.backend)
		wlr_backend_destroy(s.backend);
	wl_display_destroy(s.display);
	return rc;
}
