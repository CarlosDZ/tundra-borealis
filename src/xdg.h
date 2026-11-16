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
	char type;
	int placed;
	struct wlr_xdg_toplevel *toplevel;
	struct wlr_xdg_toplevel_decoration_v1 *decoration;
	struct wlr_scene_tree *tree;
	struct wlr_scene_rect *border;
	struct wlr_scene_tree *surface;
	struct wlr_scene_tree *popups;
	struct wlr_box box;

	struct wl_listener map, unmap, commit, destroy;
	struct wl_listener request_fullscreen, request_maximize;
	struct wl_listener decoration_mode, decoration_destroy;
};

void xdg_init(struct server *s);
struct window *xdg_window(struct server *s, unsigned int id);

void xdg_place(struct window *w, int index, struct wlr_box r, int focused);

#endif
