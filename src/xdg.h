#ifndef TUNDRA_XDG_H
#define TUNDRA_XDG_H

#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/box.h>

#include "server.h"

#define XDG_GAP 8
#define XDG_BORDER 2

struct window {
	struct wl_list link;
	struct server *server;
	unsigned int id;
	int placed;
	int fullscreen;
	struct window *parent;
	struct fixed_run *run;
	struct wlr_xdg_toplevel *toplevel;
	struct wlr_xdg_toplevel_decoration_v1 *decoration;
	struct wlr_scene_tree *tree;
	struct wlr_scene_rect *border;
	struct wlr_scene_tree *surface;
	struct wlr_scene_tree *popups;
	struct wlr_box box;

	struct wl_listener map, unmap, commit, destroy, set_parent;
	struct wl_listener request_fullscreen, request_maximize;
	struct wl_listener decoration_mode, decoration_destroy;
};

struct popup {
	struct wl_list link;
	struct server *server;
	struct wlr_xdg_popup *xdg_popup;
	struct wlr_scene_tree *tree;
	struct wl_listener commit, destroy;
};

void xdg_init(struct server *s);
struct window *xdg_window(struct server *s, unsigned int id);

struct window *xdg_root(struct window *w);
struct window *xdg_keyboard_window(struct window *root);

struct window *xdg_popup_owner(struct popup *p);

void xdg_place(struct window *w, int index, struct wlr_box r, int focused);
void xdg_fullscreen(struct window *w, int index, int width, int height);

#endif
