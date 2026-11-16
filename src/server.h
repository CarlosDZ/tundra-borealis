#ifndef TUNDRA_SERVER_H
#define TUNDRA_SERVER_H

#include <wayland-server-core.h>

#include "clients.h"
#include "desk.h"
#include "keys.h"
#include "prefs.h"

struct window;

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
	struct wlr_scene_tree *limbo; /* windows not placed yet, never shown */
	struct wlr_xdg_shell *xdg_shell;
	struct wlr_xdg_decoration_manager_v1 *decorations;
	struct wlr_seat *seat;
	struct wlr_virtual_keyboard_manager_v1 *virtual_keyboards;
	struct wlr_virtual_pointer_manager_v1 *virtual_pointers;
	struct wlr_session *session; /* NULL without a seat to switch, headless */
	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *cursor_mgr;
	struct wlr_pointer_gestures_v1 *gestures;

	struct wl_list outputs; /* struct output.link */
	struct wl_list windows; /* struct window.link */
	struct wl_list popups;  /* struct popup.link */
	struct wl_listener new_output;
	struct wl_listener new_toplevel;
	struct wl_listener new_popup;
	struct wl_listener new_decoration;
	struct wl_list keyboards; /* struct keyboard.link */
	struct wl_listener new_input, request_set_cursor;
	struct wl_listener new_virtual_keyboard, new_virtual_pointer;
	struct wl_listener cursor_motion, cursor_motion_absolute, cursor_button,
	    cursor_axis, cursor_frame;
	struct wl_listener swipe_begin, swipe_update, swipe_end;
	struct wl_listener pinch_begin, pinch_update, pinch_end;
	struct wl_listener hold_begin, hold_end;
	int input_ready;

	struct desk desk;
	struct prefs prefs;
	struct fixed_canvas fixed[DESK_CANVASES];
	struct fixed_run runs[DESK_CANVASES][FIXED_MAX];
	unsigned int next_id;
	struct keys keys;
	struct window *active;
	int pointer_locked;
	uint32_t swipe_fingers; /* 0 when no swipe is going on */
	double swipe_dx, swipe_dy;
	int out_w, out_h;
	int stopping;
};

/* Runs the compositor until SIGTERM or SIGINT. 0, or -1 if it cannot start. */
int server_run(void);

void server_arrange(struct server *s, int index);
void server_arrange_all(struct server *s);

void server_canvas_origin(struct server *s, int index, int *x, int *y);

#endif
