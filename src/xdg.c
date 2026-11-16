#include <stdio.h>
#include <stdlib.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/edges.h>

#include "input.h"
#include "xdg.h"

#define XDG_SHELL_VERSION 6

static const float border_color[4] = {0.24f, 0.22f, 0.21f, 1.0f};
static const float border_focused[4] = {0.51f, 0.65f, 0.60f, 1.0f};
static const float backdrop[4] = {0.0f, 0.0f, 0.0f, 1.0f};

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

struct window *xdg_root(struct window *w) {
	while (w && w->parent)
		w = w->parent;
	return w;
}

struct window *xdg_keyboard_window(struct window *root) {
	struct window *at = root;
	for (;;) {
		struct window *best = NULL, *d;
		wl_list_for_each(d, &at->server->windows, link) {
			if (d->parent == at && d->toplevel->base->surface->mapped &&
			    (!best || d->id > best->id))
				best = d;
		}
		if (!best)
			return at;
		at = best;
	}
}

struct window *xdg_popup_owner(struct popup *p) {
	struct wlr_xdg_surface *x = p->xdg_popup->base;
	while (x && x->role == WLR_XDG_SURFACE_ROLE_POPUP)
		x = x->popup->parent
		        ? wlr_xdg_surface_try_from_wlr_surface(x->popup->parent)
		        : NULL;
	if (!x || x->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL)
		return NULL;
	return from_toplevel(p->server, x->toplevel);
}

static void clip(struct window *w) {
	struct wlr_box g = w->toplevel->base->geometry;
	struct wlr_box c = {g.x, g.y, w->box.width, w->box.height};
	wlr_scene_subsurface_tree_set_clip(&w->surface->node, &c);
}

static void place_children(struct window *p);

static void place_dialog(struct window *d) {
	struct window *p = d->parent;
	struct wlr_box g = d->toplevel->base->geometry;
	struct layout_rect r = layout_dialog(p->box.width, p->box.height, g.width,
	                                     g.height, XDG_BORDER);
	int iw = r.w - 2 * XDG_BORDER, ih = r.h - 2 * XDG_BORDER;

	wlr_scene_node_set_position(&d->tree->node, p->surface->node.x + r.x,
	                            p->surface->node.y + r.y);
	wlr_scene_rect_set_size(d->border, r.w, r.h);
	wlr_scene_rect_set_color(d->border, border_focused);
	if ((g.width > iw || g.height > ih) &&
	    (iw != d->box.width || ih != d->box.height))
		wlr_xdg_toplevel_set_size(d->toplevel, iw, ih);
	d->box.x = p->box.x + r.x + XDG_BORDER;
	d->box.y = p->box.y + r.y + XDG_BORDER;
	d->box.width = iw;
	d->box.height = ih;
	clip(d);
	place_children(d);
}

static void place_children(struct window *p) {
	struct window *d;
	wl_list_for_each(d, &p->server->windows, link) {
		if (d->parent == p && d->toplevel->base->surface->mapped)
			place_dialog(d);
	}
}

static void put_in(struct window *w, int index) {
	struct server *s = w->server;
	if (w->tree->node.parent != s->canvas[index])
		wlr_scene_node_reparent(&w->tree->node, s->canvas[index]);
}

static void inset(struct window *w, int at) {
	wlr_scene_node_set_position(&w->surface->node, at, at);
	wlr_scene_node_set_position(&w->popups->node, at, at);
}

void xdg_place(struct window *w, int index, struct wlr_box r, int focused) {
	/* Half a gap around each slot and as a margin: one gap between windows and
	 * at the edges */
	int x = r.x + XDG_GAP, y = r.y + XDG_GAP;
	int ow = r.width - XDG_GAP, oh = r.height - XDG_GAP;
	int cw = ow - 2 * XDG_BORDER, ch = oh - 2 * XDG_BORDER;
	if (cw < 1)
		cw = 1;
	if (ch < 1)
		ch = 1;

	put_in(w, index);
	if (w->fullscreen) {
		w->fullscreen = 0;
		wlr_xdg_toplevel_set_fullscreen(w->toplevel, false);
		inset(w, XDG_BORDER);
	}
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
	place_children(w);
}

void xdg_fullscreen(struct window *w, int index, int width, int height) {
	put_in(w, index);
	wlr_scene_node_raise_to_top(&w->tree->node);
	wlr_scene_node_set_position(&w->tree->node, 0, 0);
	/* The border is the black behind a smaller client */
	wlr_scene_rect_set_size(w->border, width, height);
	wlr_scene_rect_set_color(w->border, backdrop);
	inset(w, 0);
	if (!w->fullscreen) {
		w->fullscreen = 1;
		wlr_xdg_toplevel_set_fullscreen(w->toplevel, true);
	}
	if (width != w->box.width || height != w->box.height)
		wlr_xdg_toplevel_set_size(w->toplevel, width, height);
	w->box.x = w->box.y = 0;
	w->box.width = width;
	w->box.height = height;
	clip(w);
	place_children(w);
}

static void place_new(struct window *w) {
	struct server *s = w->server;
	char type = prefs_type(&s->prefs, w->toplevel->app_id);
	int c = desk_open(&s->desk, w->id, type);
	if (c < 0)
		return;
	w->placed = 1;
	if (w->toplevel->requested.fullscreen)
		desk_fullscreen(&s->desk, w->id, 1);
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

static void become_dialog(struct window *w, struct window *p) {
	unplace(w);
	w->parent = p;
	w->box.width = w->box.height = 0;
	wlr_scene_node_reparent(&w->tree->node, p->tree);
	if (w->toplevel->base->surface->mapped)
		place_dialog(w);
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

	if (!t->base->initial_commit) {
		if (w->parent && t->base->surface->mapped)
			place_dialog(w);
		else if (w->placed)
			clip(w);
		return;
	}

	wlr_xdg_toplevel_set_wm_capabilities(
	    t, WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN);
	set_server_side(w);
	struct window *p = t->parent ? from_toplevel(w->server, t->parent) : NULL;
	if (p) {
		become_dialog(w, p);
		int bw = p->box.width - 2 * XDG_BORDER;
		int bh = p->box.height - 2 * XDG_BORDER;
		if (bw > 0 && bh > 0)
			wlr_xdg_toplevel_set_bounds(t, bw, bh);
		wlr_xdg_toplevel_set_size(t, 0, 0);
		return;
	}
	wlr_xdg_toplevel_set_tiled(t, WLR_EDGE_TOP | WLR_EDGE_BOTTOM |
	                                  WLR_EDGE_LEFT | WLR_EDGE_RIGHT);
	place_new(w);
	if (!w->placed) {
		/* No room, and the client waits for a configure */
		wlr_xdg_toplevel_set_size(t, 0, 0);
		wlr_xdg_toplevel_send_close(t);
	}
}

static void window_map(struct wl_listener *listener, void *data) {
	(void)data;
	struct window *w = wl_container_of(listener, w, map);
	if (w->parent)
		place_dialog(w);
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

/* wlroots gives a dialog its grandparent, or none, when its parent unmaps */
static void window_set_parent(struct wl_listener *listener, void *data) {
	(void)data;
	struct window *w = wl_container_of(listener, w, set_parent);
	struct server *s = w->server;
	if (!w->toplevel->base->initialized)
		return;
	struct wlr_xdg_toplevel *t = w->toplevel;
	struct window *p = t->parent ? from_toplevel(s, t->parent) : NULL;
	if (p) {
		if (p != w->parent)
			become_dialog(w, p);
	} else if (!w->placed) {
		w->parent = NULL;
		wlr_scene_node_reparent(&w->tree->node, s->limbo);
		wlr_xdg_toplevel_set_tiled(t, WLR_EDGE_TOP | WLR_EDGE_BOTTOM |
		                                  WLR_EDGE_LEFT | WLR_EDGE_RIGHT);
		place_new(w);
	}
	if (!s->stopping)
		input_focus(s, 0);
}

/* The protocol wants a configure for every request */
static void answer_state(struct window *w) {
	if (w->toplevel->base->initialized)
		wlr_xdg_surface_schedule_configure(w->toplevel->base);
}

static void window_request_fullscreen(struct wl_listener *listener,
                                      void *data) {
	(void)data;
	struct window *w = wl_container_of(listener, w, request_fullscreen);
	struct server *s = w->server;
	/* Before the initial commit, place_new reads the request */
	if (w->placed && desk_fullscreen(&s->desk, w->id,
	                                 w->toplevel->requested.fullscreen) >= 0) {
		server_arrange_all(s);
		input_focus(s, 0);
	}
	answer_state(w);
}

static void window_request_maximize(struct wl_listener *listener, void *data) {
	(void)data;
	struct window *w = wl_container_of(listener, w, request_maximize);
	answer_state(w);
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
	struct server *s = w->server;
	if (s->active == w)
		s->active = NULL;
	unplace(w);
	/* Dialogs still inside its tree would go with it */
	struct window *d;
	wl_list_for_each(d, &s->windows, link) {
		if (d->parent == w) {
			d->parent = NULL;
			wlr_scene_node_reparent(&d->tree->node, s->limbo);
		}
	}
	decoration_forget(w);
	wl_list_remove(&w->map.link);
	wl_list_remove(&w->unmap.link);
	wl_list_remove(&w->commit.link);
	wl_list_remove(&w->destroy.link);
	wl_list_remove(&w->set_parent.link);
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
	inset(w, XDG_BORDER);
	t->base->data = w->popups;

	w->map.notify = window_map;
	wl_signal_add(&t->base->surface->events.map, &w->map);
	w->unmap.notify = window_unmap;
	wl_signal_add(&t->base->surface->events.unmap, &w->unmap);
	w->commit.notify = window_commit;
	wl_signal_add(&t->base->surface->events.commit, &w->commit);
	w->destroy.notify = window_destroy;
	wl_signal_add(&t->events.destroy, &w->destroy);
	w->set_parent.notify = window_set_parent;
	wl_signal_add(&t->events.set_parent, &w->set_parent);
	w->request_fullscreen.notify = window_request_fullscreen;
	wl_signal_add(&t->events.request_fullscreen, &w->request_fullscreen);
	w->request_maximize.notify = window_request_maximize;
	wl_signal_add(&t->events.request_maximize, &w->request_maximize);
	wl_list_insert(s->windows.prev, &w->link);
}

static void popup_commit(struct wl_listener *listener, void *data) {
	(void)data;
	struct popup *p = wl_container_of(listener, p, commit);
	struct wlr_xdg_popup *xp = p->xdg_popup;
	if (!xp->base->initial_commit)
		return;
	struct window *owner = xdg_popup_owner(p);
	if (owner && owner->box.width > 0) {
		struct wlr_box g = owner->toplevel->base->geometry;
		struct wlr_box box = {g.x, g.y, owner->box.width, owner->box.height};
		wlr_xdg_popup_unconstrain_from_box(xp, &box);
	}
	wlr_xdg_surface_schedule_configure(xp->base);
}

static void popup_destroy(struct wl_listener *listener, void *data) {
	(void)data;
	struct popup *p = wl_container_of(listener, p, destroy);
	wl_list_remove(&p->commit.link);
	wl_list_remove(&p->destroy.link);
	wl_list_remove(&p->link);
	free(p);
}

static void new_popup(struct wl_listener *listener, void *data) {
	struct server *s = wl_container_of(listener, s, new_popup);
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
	p->tree = wlr_scene_xdg_surface_create(parent->data, xp->base);
	if (!p->tree) {
		free(p);
		return;
	}
	xp->base->data = p->tree;
	p->server = s;
	p->xdg_popup = xp;
	p->commit.notify = popup_commit;
	wl_signal_add(&xp->base->surface->events.commit, &p->commit);
	p->destroy.notify = popup_destroy;
	wl_signal_add(&xp->events.destroy, &p->destroy);
	wl_list_insert(s->popups.prev, &p->link);
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
	wl_list_init(&s->popups);
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
