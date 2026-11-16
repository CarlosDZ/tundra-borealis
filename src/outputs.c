#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>

#include "outputs.h"
#include "server.h"
#include "xdg.h"

static const float background_color[4] = {0.09f, 0.10f, 0.12f, 1.0f};

struct output {
	struct wl_list link;
	struct server *server;
	struct wlr_output *wlr_output;
	struct wlr_scene_rect *background;
	int in_layout;
	struct wl_listener frame, request_state, destroy;
};

static int is_panel(struct wlr_output *o) {
	return strncmp(o->name, "eDP", 3) == 0 || strncmp(o->name, "LVDS", 4) == 0;
}

static struct output *find_main(struct server *s) {
	struct output *o;
	wl_list_for_each(o, &s->outputs,
	                 link) if (is_panel(o->wlr_output)) return o;
	if (wl_list_empty(&s->outputs))
		return NULL;
	return wl_container_of(s->outputs.next, o, link);
}

static struct output *find_second(struct server *s) {
	struct output *m = find_main(s), *o;
	wl_list_for_each(o, &s->outputs, link) if (o != m) return o;
	return NULL;
}

struct wlr_output *outputs_main(struct server *s) {
	struct output *o = find_main(s);
	return o ? o->wlr_output : NULL;
}

struct wlr_output *outputs_second(struct server *s) {
	struct output *o = find_second(s);
	return o ? o->wlr_output : NULL;
}

static void put(struct server *s, struct output *o, int x, int y) {
	struct wlr_output_layout_output *lo =
	    wlr_output_layout_add(s->output_layout, o->wlr_output, x, y);
	if (!o->in_layout && lo) {
		struct wlr_scene_output *so =
		    wlr_scene_output_create(s->scene, o->wlr_output);
		if (so) {
			wlr_scene_output_layout_add_output(s->scene_layout, lo, so);
			o->in_layout = 1;
		}
	}
	int w, h;
	wlr_output_effective_resolution(o->wlr_output, &w, &h);
	wlr_scene_node_set_position(&o->background->node, x, y);
	wlr_scene_rect_set_size(o->background, w, h);
	wlr_scene_node_set_enabled(&o->background->node, true);
}

void outputs_apply(struct server *s) {
	struct output *m = find_main(s), *sec = find_second(s), *o;
	enum grid_screens mode = !sec        ? GRID_ONE
	                         : s->mirror ? GRID_MIRROR
	                                     : GRID_EXTENDED;
	grid_set_screens(&s->desk.grid, mode);

	wl_list_for_each(o, &s->outputs, link) {
		if (o != m && o != sec) {
			wlr_output_layout_remove(s->output_layout, o->wlr_output);
			wlr_scene_node_set_enabled(&o->background->node, false);
		}
	}
	int mw = 0, mh = 0, sw = 0, sh = 0;
	if (m)
		wlr_output_effective_resolution(m->wlr_output, &mw, &mh);
	if (sec)
		wlr_output_effective_resolution(sec->wlr_output, &sw, &sh);
	if (m && sec && mode == GRID_EXTENDED && !s->screens.second_right) {
		put(s, sec, 0, 0);
		put(s, m, sw, 0);
	} else if (m && sec && mode == GRID_EXTENDED) {
		put(s, m, 0, 0);
		put(s, sec, mw, 0);
	} else {
		if (m)
			put(s, m, 0, 0);
		if (sec)
			put(s, sec, 0, 0);
	}
	desk_resize(&s->desk, mw - XDG_GAP, mh - XDG_GAP);
	desk_resize_second(&s->desk, sw - XDG_GAP, sh - XDG_GAP);
	server_arrange_all(s);
}

void outputs_canvas_box(struct server *s, int index, struct wlr_box *box) {
	struct wlr_output *o = outputs_main(s);
	if (s->desk.grid.screens == GRID_EXTENDED && desk_pos(index).h % 2)
		o = outputs_second(s);
	memset(box, 0, sizeof(*box));
	if (o)
		wlr_output_layout_get_box(s->output_layout, o, box);
}

int outputs_screen_at(struct server *s, double x, double y) {
	struct wlr_output *o = wlr_output_layout_output_at(s->output_layout, x, y);
	if (!o)
		return -1;
	return o == outputs_second(s) && s->desk.grid.screens == GRID_EXTENDED
	           ? GRID_SECOND
	           : GRID_MAIN;
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
	outputs_apply(o->server);
}

static void output_destroy(struct wl_listener *listener, void *data) {
	(void)data;
	struct output *o = wl_container_of(listener, o, destroy);
	struct server *s = o->server;
	wl_list_remove(&o->frame.link);
	wl_list_remove(&o->request_state.link);
	wl_list_remove(&o->destroy.link);
	wl_list_remove(&o->link);
	if (!s->stopping)
		wlr_scene_node_destroy(&o->background->node);
	free(o);
	if (!s->stopping)
		outputs_apply(s);
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
	o->background =
	    wlr_scene_rect_create(s->backgrounds, 0, 0, background_color);
	if (!o->background) {
		free(o);
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
	outputs_apply(s);
}

void outputs_init(struct server *s) {
	s->new_output.notify = new_output;
	wl_signal_add(&s->backend->events.new_output, &s->new_output);
}

void outputs_finish(struct server *s) { wl_list_remove(&s->new_output.link); }
