#include <stdio.h>
#include <stdlib.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/edges.h>

#include "input.h"
#include "xdg.h"

#define XDG_SHELL_VERSION 6

static const float border_color[4] = {0.24f, 0.22f, 0.21f, 1.0f};
static const float border_focused[4] = {0.51f, 0.65f, 0.60f, 1.0f};

struct popup {
	struct wlr_xdg_popup *xdg_popup;
	struct wl_listener commit, destroy;
};

struct window *xdg_window(struct server *s, unsigned int id) {
	struct window *w;
	wl_list_for_each(w, &s->windows, link) if (w->id == id) return w;
	return NULL;
}

static struct window *from_toplevel(struct server *s,
                                    struct wlr_xdg_toplevel *t) {
	struct window *w;
	wl_list_for_each(w, &s->windows, link) if (w->toplevel == t) return w;
	return NULL;
}

static void clip(struct window *w) {
	struct wlr_box g = w->toplevel->base->geometry;
	struct wlr_box c = {g.x, g.y, w->box.width, w->box.height};
	wlr_scene_subsurface_tree_set_clip(&w->surface->node, &c);
}

void xdg_place(struct window *w, int index, struct wlr_box r, int focused) {
	struct server *s = w->server;
	/* Half a gap around each slot and as a margin: one gap between windows and
	 * at the edges */
	int x = r.x + XDG_GAP, y = r.y + XDG_GAP;
	int ow = r.width - XDG_GAP, oh = r.height - XDG_GAP;
	int cw = ow - 2 * XDG_BORDER, ch = oh - 2 * XDG_BORDER;
	if (cw < 1)
		cw = 1;
	if (ch < 1)
		ch = 1;

	if (w->tree->node.parent != s->canvas[index])
		wlr_scene_node_reparent(&w->tree->node, s->canvas[index]);
	wlr_scene_node_set_position(&w->tree->node, x, y);
	wlr_scene_rect_set_size(w->border, ow, oh);
	wlr_scene_rect_set_color(w->border,
	                         focused ? border_focused : border_color);
	if (cw != w->box.width || ch != w->box.height)
		wlr_xdg_toplevel_set_size(w->toplevel, cw, ch);
	w->box.x = x + XDG_BORDER;
	w->box.y = y + XDG_BORDER;
	w->box.width = cw;
	w->box.height = ch;
	clip(w);
}

static void place_new(struct window *w) {
	struct server *s = w->server;
	char type = prefs_type(&s->prefs, w->toplevel->app_id);
	int c = desk_open(&s->desk, w->id, type);
	if (c < 0)
		return;
	w->placed = 1;
	server_arrange(s, c);
	/* No output yet, and the client waits for a configure */
	if (w->box.width == 0)
		wlr_xdg_toplevel_set_size(w->toplevel, 0, 0);
}

static void unplace(struct window *w) {
	if (!w->placed)
		return;
	struct server *s = w->server;
	int c = desk_close(&s->desk, w->id);
	w->placed = 0;
	w->box.width = w->box.height = 0;
	wlr_scene_node_reparent(&w->tree->node, s->limbo);
	if (c >= 0)
		server_arrange(s, c);
}

static void set_server_side(struct window *w) {
	if (w->decoration && w->toplevel->base->initialized)
		wlr_xdg_toplevel_decoration_v1_set_mode(
		    w->decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

static void window_commit(struct wl_listener *listener, void *data) {
	(void)data;
	struct window *w = wl_container_of(listener, w, commit);
	struct wlr_xdg_toplevel *t = w->toplevel;

	if (t->base->initial_commit) {
		wlr_xdg_toplevel_set_wm_capabilities(
		    t, WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN);
		wlr_xdg_toplevel_set_tiled(t, WLR_EDGE_TOP | WLR_EDGE_BOTTOM |
		                                  WLR_EDGE_LEFT | WLR_EDGE_RIGHT);
		set_server_side(w);
		place_new(w);
		if (!w->placed) {
			/* No room, and the client waits for a configure */
			wlr_xdg_toplevel_set_size(t, 0, 0);
			wlr_xdg_toplevel_send_close(t);
		}
		return;
	}
	if (w->placed)
		clip(w);
}

static void window_map(struct wl_listener *listener, void *data) {
	(void)data;
	struct window *w = wl_container_of(listener, w, map);
	wlr_scene_node_set_enabled(&w->tree->node, true);
	input_focus(w->server, 0);
}

static void window_unmap(struct wl_listener *listener, void *data) {
	(void)data;
	struct window *w = wl_container_of(listener, w, unmap);
	wlr_scene_node_set_enabled(&w->tree->node, false);
	if (w->server->active == w)
		w->server->active = NULL;
	unplace(w);
	if (!w->server->stopping)
		input_focus(w->server, 0);
}

/* The protocol wants a configure for every request */
static void refuse_state(struct window *w) {
	if (w->toplevel->base->initialized)
		wlr_xdg_surface_schedule_configure(w->toplevel->base);
}

static void window_request_fullscreen(struct wl_listener *listener,
                                      void *data) {
	(void)data;
	struct window *w = wl_container_of(listener, w, request_fullscreen);
	refuse_state(w);
}

static void window_request_maximize(struct wl_listener *listener, void *data) {
	(void)data;
	struct window *w = wl_container_of(listener, w, request_maximize);
	refuse_state(w);
}

static void decoration_forget(struct window *w) {
	if (!w->decoration)
		return;
	wl_list_remove(&w->decoration_mode.link);
	wl_list_remove(&w->decoration_destroy.link);
	w->decoration = NULL;
}

static void window_destroy(struct wl_listener *listener, void *data) {
	(void)data;
	struct window *w = wl_container_of(listener, w, destroy);
	if (w->server->active == w)
		w->server->active = NULL;
	unplace(w);
	decoration_forget(w);
	wl_list_remove(&w->map.link);
	wl_list_remove(&w->unmap.link);
	wl_list_remove(&w->commit.link);
	wl_list_remove(&w->destroy.link);
	wl_list_remove(&w->request_fullscreen.link);
	wl_list_remove(&w->request_maximize.link);
	wl_list_remove(&w->link);
	wlr_scene_node_destroy(&w->tree->node);
	free(w);
}

static void new_toplevel(struct wl_listener *listener, void *data) {
	struct server *s = wl_container_of(listener, s, new_toplevel);
	struct wlr_xdg_toplevel *t = data;

	struct window *w = calloc(1, sizeof(*w));
	if (!w) {
		perror("calloc error");
		return;
	}
	w->server = s;
	w->id = ++s->next_id;
	w->toplevel = t;
	w->tree = wlr_scene_tree_create(s->limbo);
	if (!w->tree) {
		free(w);
		return;
	}
	w->border = wlr_scene_rect_create(w->tree, 0, 0, border_color);
	w->surface = wlr_scene_xdg_surface_create(w->tree, t->base);
	w->popups = wlr_scene_tree_create(w->tree);
	if (!w->border || !w->surface || !w->popups) {
		wlr_scene_node_destroy(&w->tree->node);
		free(w);
		return;
	}
	wlr_scene_node_set_enabled(&w->tree->node, false);
	w->tree->node.data = w;
	wlr_scene_node_set_position(&w->surface->node, XDG_BORDER, XDG_BORDER);
	wlr_scene_node_set_position(&w->popups->node, XDG_BORDER, XDG_BORDER);
	t->base->data = w->popups;

	w->map.notify = window_map;
	wl_signal_add(&t->base->surface->events.map, &w->map);
	w->unmap.notify = window_unmap;
	wl_signal_add(&t->base->surface->events.unmap, &w->unmap);
	w->commit.notify = window_commit;
	wl_signal_add(&t->base->surface->events.commit, &w->commit);
	w->destroy.notify = window_destroy;
	wl_signal_add(&t->events.destroy, &w->destroy);
	w->request_fullscreen.notify = window_request_fullscreen;
	wl_signal_add(&t->events.request_fullscreen, &w->request_fullscreen);
	w->request_maximize.notify = window_request_maximize;
	wl_signal_add(&t->events.request_maximize, &w->request_maximize);
	wl_list_insert(s->windows.prev, &w->link);
}

static void popup_commit(struct wl_listener *listener, void *data) {
	(void)data;
	struct popup *p = wl_container_of(listener, p, commit);
	if (p->xdg_popup->base->initial_commit)
		wlr_xdg_surface_schedule_configure(p->xdg_popup->base);
}

static void popup_destroy(struct wl_listener *listener, void *data) {
	(void)data;
	struct popup *p = wl_container_of(listener, p, destroy);
	wl_list_remove(&p->commit.link);
	wl_list_remove(&p->destroy.link);
	free(p);
}

static void new_popup(struct wl_listener *listener, void *data) {
	(void)listener;
	struct wlr_xdg_popup *xp = data;
	struct wlr_xdg_surface *parent =
	    xp->parent ? wlr_xdg_surface_try_from_wlr_surface(xp->parent) : NULL;
	if (!parent || !parent->data)
		return;
	struct popup *p = calloc(1, sizeof(*p));
	if (!p) {
		perror("calloc error");
		return;
	}
	struct wlr_scene_tree *tree =
	    wlr_scene_xdg_surface_create(parent->data, xp->base);
	if (!tree) {
		free(p);
		return;
	}
	xp->base->data = tree;
	p->xdg_popup = xp;
	p->commit.notify = popup_commit;
	wl_signal_add(&xp->base->surface->events.commit, &p->commit);
	p->destroy.notify = popup_destroy;
	wl_signal_add(&xp->events.destroy, &p->destroy);
}

static void decoration_mode(struct wl_listener *listener, void *data) {
	(void)data;
	struct window *w = wl_container_of(listener, w, decoration_mode);
	set_server_side(w);
}

static void decoration_destroy(struct wl_listener *listener, void *data) {
	(void)data;
	struct window *w = wl_container_of(listener, w, decoration_destroy);
	decoration_forget(w);
}

static void new_decoration(struct wl_listener *listener, void *data) {
	struct server *s = wl_container_of(listener, s, new_decoration);
	struct wlr_xdg_toplevel_decoration_v1 *d = data;
	struct window *w = from_toplevel(s, d->toplevel);
	if (!w || w->decoration)
		return;
	w->decoration = d;
	w->decoration_mode.notify = decoration_mode;
	wl_signal_add(&d->events.request_mode, &w->decoration_mode);
	w->decoration_destroy.notify = decoration_destroy;
	wl_signal_add(&d->events.destroy, &w->decoration_destroy);
	set_server_side(w);
}

void xdg_init(struct server *s) {
	s->xdg_shell = wlr_xdg_shell_create(s->display, XDG_SHELL_VERSION);
	s->decorations = wlr_xdg_decoration_manager_v1_create(s->display);
	if (!s->xdg_shell || !s->decorations)
		return;
	s->new_toplevel.notify = new_toplevel;
	wl_signal_add(&s->xdg_shell->events.new_toplevel, &s->new_toplevel);
	s->new_popup.notify = new_popup;
	wl_signal_add(&s->xdg_shell->events.new_popup, &s->new_popup);
	s->new_decoration.notify = new_decoration;
	wl_signal_add(&s->decorations->events.new_toplevel_decoration,
	              &s->new_decoration);
}
