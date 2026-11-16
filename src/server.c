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
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/util/log.h>

#include "control.h"
#include "input.h"
#include "outputs.h"
#include "server.h"
#include "testsock.h"
#include "xdg.h"

#define COMPOSITOR_VERSION 6
#define USER_FILE_MAX 65536

static const float cover_color[4] = {0.35f, 0.12f, 0.12f, 1.0f};

static void arrange_fixed(struct server *s, int index) {
	const struct fixed_canvas *fc = s->desk.fixed[index];
	for (int i = 0; i < fc->n; i++) {
		struct fixed_run *r = &s->runs[index][i];
		int cw, ch;
		desk_canvas_size(&s->desk, index, &cw, &ch);
		struct layout_rect lr = fixed_rect(&fc->p[i], cw, ch);
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
	struct wlr_box box;
	outputs_canvas_box(s, index, &box);
	struct window *full = s->desk.fullscreen[index]
	                          ? xdg_window(s, s->desk.fullscreen[index])
	                          : NULL;
	if (full)
		xdg_fullscreen(full, index, box.width, box.height);
	for (int c = 0; c < DESK_CANVASES; c++) {
		outputs_canvas_box(s, c, &box);
		wlr_scene_node_set_position(&s->canvas[c]->node, box.x, box.y);
		wlr_scene_node_set_enabled(&s->canvas[c]->node,
		                           grid_is_shown(&s->desk.grid, desk_pos(c)));
	}
}

void server_canvas_origin(struct server *s, int index, int *x, int *y) {
	struct wlr_box box;
	outputs_canvas_box(s, index, &box);
	*x = box.x;
	*y = box.y;
}

void server_arrange_all(struct server *s) {
	for (int c = 0; c < DESK_CANVASES; c++)
		server_arrange(s, c);
}

/* A file of $XDG_CONFIG_HOME/tundra/borealis, or of ~/.config. Its length,
 * or -1 when there is none or it cannot be read, which is reported. */
static long user_file(const char *name, char *text, size_t max) {
	char path[4096];
	const char *base = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");
	int n;
	if (base && *base)
		n = snprintf(path, sizeof(path), "%s/tundra/borealis/%s", base, name);
	else if (home && *home)
		n = snprintf(path, sizeof(path), "%s/.config/tundra/borealis/%s", home,
		             name);
	else
		return -1;
	if (n < 0 || (size_t)n >= sizeof(path))
		return -1;
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		if (errno != ENOENT)
			fprintf(stderr, "%s: %s\n", path, strerror(errno));
		return -1;
	}
	size_t len = 0;
	ssize_t r;
	while (len <= max && (r = read(fd, text + len, max + 1 - len)) > 0)
		len += (size_t)r;
	close(fd);
	if (len > max) {
		fprintf(stderr, "%s: bigger than %zu bytes, ignored\n", path, max);
		return -1;
	}
	return (long)len;
}

static void load_config(struct server *s) {
	static char text[USER_FILE_MAX + 1];
	char err[PREFS_ERR_MAX > SCREENS_ERR_MAX ? PREFS_ERR_MAX : SCREENS_ERR_MAX];
	long n = user_file("windows", text, USER_FILE_MAX);
	if (n >= 0 && prefs_parse(text, (size_t)n, &s->prefs, err) < 0)
		fprintf(stderr, "windows: %s, ignored\n", err);
	screens_default(&s->screens);
	n = user_file("screens", text, USER_FILE_MAX);
	if (n >= 0 && screens_parse(text, (size_t)n, &s->screens, err) < 0)
		fprintf(stderr, "screens: %s, ignored\n", err);
	s->mirror = s->screens.mirror;
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
	load_config(&s);
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
	s.backgrounds = wlr_scene_tree_create(&s.scene->tree);
	for (int c = 0; c < DESK_CANVASES; c++)
		s.canvas[c] = wlr_scene_tree_create(&s.scene->tree);
	s.limbo = wlr_scene_tree_create(&s.scene->tree);
	if (!s.scene_layout || !s.backgrounds || !s.canvas[DESK_CANVASES - 1] ||
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
	clients_filter_globals(&s);
	if (!wlr_screencopy_manager_v1_create(s.display)) {
		fprintf(stderr, "cannot create the privileged globals\n");
		goto out;
	}
	outputs_init(&s);

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
	if (control_init(&s, socket) < 0)
		goto out;
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
	control_finish();
	clients_finish(&s);
	testsock_finish();
	if (sigterm)
		wl_event_source_remove(sigterm);
	if (sigint)
		wl_event_source_remove(sigint);
	wl_display_destroy_clients(s.display);
	/* wlroots asserts that no listener is left */
	outputs_finish(&s);
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
