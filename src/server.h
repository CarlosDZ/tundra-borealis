#ifndef TUNDRA_SERVER_H
#define TUNDRA_SERVER_H

#include <wayland-server-core.h>

#include "desk.h"
#include "prefs.h"

struct server {
	struct wl_display *display;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_output_layout *output_layout;
	struct wlr_scene *scene;
	struct wlr_scene_output_layout *scene_layout;
	struct wlr_scene_rect *background;
	struct wlr_scene_tree *canvas[DESK_CANVASES];
	struct wlr_scene_tree *limbo;
	struct wlr_xdg_shell *xdg_shell;
	struct wlr_xdg_decoration_manager_v1 *decorations;
	struct wlr_seat *seat;

	struct wl_list outputs;
	struct wl_list windows;
	struct wl_listener new_output;
	struct wl_listener new_toplevel;
	struct wl_listener new_popup;
	struct wl_listener new_decoration;

	struct desk desk;
	struct prefs prefs;
	unsigned int next_id;
	int stopping;
};

int server_run(void);

void server_arrange(struct server *s, int index);
void server_arrange_all(struct server *s);

#endif
