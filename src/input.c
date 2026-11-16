#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_pointer_gestures_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <xkbcommon/xkbcommon.h>

#include "input.h"
#include "spawn.h"
#include "xdg.h"

#define REPEAT_RATE 25
#define REPEAT_DELAY 600
#define CURSOR_SIZE 24
#define DEFAULT_TERMINAL "frost"

struct keyboard {
	struct wl_list link;
	struct server *server;
	struct wlr_keyboard *wlr;
	struct wl_listener modifiers, key, destroy;
};

static uint32_t now_msec(void) {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint32_t)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}

struct window *input_window_at(struct server *s, double lx, double ly,
                               struct wlr_surface **surface, double *sx,
                               double *sy) {
	*surface = NULL;
	struct wlr_scene_node *node =
	    wlr_scene_node_at(&s->scene->tree.node, lx, ly, sx, sy);
	if (!node)
		return NULL;
	if (node->type == WLR_SCENE_NODE_BUFFER) {
		struct wlr_scene_surface *ss =
		    wlr_scene_surface_try_from_buffer(wlr_scene_buffer_from_node(node));
		if (ss)
			*surface = ss->surface;
	}
	for (struct wlr_scene_tree *t = node->parent; t; t = t->node.parent)
		if (t->node.data)
			return t->node.data;
	return NULL;
}

static void pointer_update(struct server *s, uint32_t time, int follow) {
	struct wlr_surface *surface;
	double sx, sy;
	struct window *w = xdg_root(
	    input_window_at(s, s->cursor->x, s->cursor->y, &surface, &sx, &sy));
	if (follow && w && w->id != desk_focused(&s->desk) &&
	    desk_focus_window(&s->desk, w->id) == 0) {
		server_arrange_all(s);
		input_focus(s, 0);
	}
	if (!surface) {
		wlr_cursor_set_xcursor(s->cursor, s->cursor_mgr, "default");
		wlr_seat_pointer_clear_focus(s->seat);
		return;
	}
	wlr_seat_pointer_notify_enter(s->seat, surface, sx, sy);
	wlr_seat_pointer_notify_motion(s->seat, time, sx, sy);
}

void input_focus(struct server *s, int warp) {
	unsigned int id = desk_focused(&s->desk);
	struct window *w = id ? xdg_window(s, id) : NULL;
	if (w && !w->toplevel->base->surface->mapped)
		w = NULL;
	struct window *root = w;
	if (w)
		w = xdg_keyboard_window(w);
	if (s->active && s->active != w)
		wlr_xdg_toplevel_set_activated(s->active->toplevel, false);
	s->active = w;
	if (!w) {
		wlr_seat_keyboard_notify_clear_focus(s->seat);
		pointer_update(s, now_msec(), 0);
		return;
	}
	wlr_xdg_toplevel_set_activated(w->toplevel, true);
	struct wlr_keyboard *kb = wlr_seat_get_keyboard(s->seat);
	if (kb)
		wlr_seat_keyboard_notify_enter(s->seat, w->toplevel->base->surface,
		                               kb->keycodes, kb->num_keycodes,
		                               &kb->modifiers);
	else
		wlr_seat_keyboard_notify_enter(s->seat, w->toplevel->base->surface,
		                               NULL, 0, NULL);
	if (warp) {
		int ox, oy;
		server_canvas_origin(s, desk_find(&s->desk, root->id), &ox, &oy);
		wlr_cursor_warp(s->cursor, NULL, ox + w->box.x + w->box.width / 2.0,
		                oy + w->box.y + w->box.height / 2.0);
	}
	pointer_update(s, now_msec(), 0);
}

static void act(struct server *s, const struct keys_result *r) {
	struct desk *d = &s->desk;
	struct grid_pos to = {r->h, r->d};
	int done = -1;

	switch (r->action) {
	case KEYS_FOCUS_NEXT:
		done = desk_focus_next(d);
		break;
	case KEYS_SWAP_NEXT:
		done = desk_move(d);
		break;
	case KEYS_ROTATE:
		done = desk_rotate(d);
		break;
	case KEYS_LEFT:
		done = desk_step(d, GRID_LEFT);
		break;
	case KEYS_RIGHT:
		done = desk_step(d, GRID_RIGHT);
		break;
	case KEYS_DEEPER:
		done = desk_step(d, GRID_DEEPER);
		break;
	case KEYS_SHALLOWER:
		done = desk_step(d, GRID_SHALLOWER);
		break;
	case KEYS_SEND_LEFT:
		done = desk_send_step(d, GRID_LEFT);
		break;
	case KEYS_SEND_RIGHT:
		done = desk_send_step(d, GRID_RIGHT);
		break;
	case KEYS_SEND_DEEPER:
		done = desk_send_step(d, GRID_DEEPER);
		break;
	case KEYS_SEND_SHALLOWER:
		done = desk_send_step(d, GRID_SHALLOWER);
		break;
	case KEYS_GOTO:
		done = desk_goto(d, to);
		break;
	case KEYS_SEND_TO:
		done = desk_send(d, to);
		break;
	case KEYS_OTHER_SCREEN:
		done = grid_other_screen(&d->grid);
		break;
	case KEYS_CLOSE:
		if (s->active && !(s->active->run && !s->active->parent))
			wlr_xdg_toplevel_send_close(s->active->toplevel);
		return;
	case KEYS_FULLSCREEN: {
		unsigned int id = desk_focused(d);
		int c = desk_find(d, id);
		done = c < 0 ? -1 : desk_fullscreen(d, id, d->fullscreen[c] != id);
		break;
	}
	case KEYS_TERMINAL: {
		const char *cmd = getenv("TUNDRA_BOREALIS_TERMINAL");
		spawn(cmd && *cmd ? cmd : DEFAULT_TERMINAL, -1);
		return;
	}
	case KEYS_POINTER_LOCK:
		s->pointer_locked = !s->pointer_locked;
		return;
	default:
		return;
	}
	if (done < 0)
		return;
	server_arrange_all(s);
	input_focus(s, 1);
}

static int switch_vt(struct server *s, struct wlr_keyboard *kb,
                     uint32_t keycode) {
	if (!s->session)
		return 0;
	const xkb_keysym_t *syms;
	int n = xkb_state_key_get_syms(kb->xkb_state, keycode + 8, &syms);
	for (int i = 0; i < n; i++) {
		if (syms[i] >= XKB_KEY_XF86Switch_VT_1 &&
		    syms[i] <= XKB_KEY_XF86Switch_VT_12) {
			wlr_session_change_vt(s->session,
			                      syms[i] - XKB_KEY_XF86Switch_VT_1 + 1);
			return 1;
		}
	}
	return 0;
}

static unsigned int keys_mods(struct wlr_keyboard *kb) {
	uint32_t m = wlr_keyboard_get_modifiers(kb);
	return (m & WLR_MODIFIER_LOGO ? KEYS_SUPER : 0) |
	       (m & WLR_MODIFIER_SHIFT ? KEYS_SHIFT : 0) |
	       (m & WLR_MODIFIER_CTRL ? KEYS_CTRL : 0) |
	       (m & WLR_MODIFIER_ALT ? KEYS_ALT : 0);
}

static void keyboard_key(struct wl_listener *listener, void *data) {
	struct keyboard *k = wl_container_of(listener, k, key);
	struct server *s = k->server;
	struct wlr_keyboard_key_event *e = data;
	int pressed = e->state == WL_KEYBOARD_KEY_STATE_PRESSED;

	if (pressed && switch_vt(s, k->wlr, e->keycode))
		return;
	struct keys_result r;
	if (keys_feed(&s->keys, e->keycode, pressed, keys_mods(k->wlr), &r)) {
		if (r.action != KEYS_NONE)
			act(s, &r);
		return;
	}
	wlr_seat_set_keyboard(s->seat, k->wlr);
	wlr_seat_keyboard_notify_key(s->seat, e->time_msec, e->keycode, e->state);
}

static void keyboard_modifiers(struct wl_listener *listener, void *data) {
	(void)data;
	struct keyboard *k = wl_container_of(listener, k, modifiers);
	wlr_seat_set_keyboard(k->server->seat, k->wlr);
	wlr_seat_keyboard_notify_modifiers(k->server->seat, &k->wlr->modifiers);
}

static void update_caps(struct server *s) {
	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&s->keyboards))
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	wlr_seat_set_capabilities(s->seat, caps);
}

static void keyboard_destroy(struct wl_listener *listener, void *data) {
	(void)data;
	struct keyboard *k = wl_container_of(listener, k, destroy);
	struct server *s = k->server;
	wl_list_remove(&k->modifiers.link);
	wl_list_remove(&k->key.link);
	wl_list_remove(&k->destroy.link);
	wl_list_remove(&k->link);
	free(k);
	if (!s->stopping)
		update_caps(s);
}

static void add_keyboard(struct server *s, struct wlr_keyboard *wlr) {
	struct keyboard *k = calloc(1, sizeof(*k));
	if (!k) {
		perror("calloc error");
		return;
	}
	if (!wlr_input_device_get_virtual_keyboard(&wlr->base)) {
		struct xkb_rule_names names = {.options = "caps:escape"};
		struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
		struct xkb_keymap *map =
		    ctx ? xkb_keymap_new_from_names(ctx, &names,
		                                    XKB_KEYMAP_COMPILE_NO_FLAGS)
		        : NULL;
		int ok = map && wlr_keyboard_set_keymap(wlr, map);
		xkb_keymap_unref(map);
		xkb_context_unref(ctx);
		if (!ok) {
			fprintf(stderr, "%s: cannot set the keymap\n", wlr->base.name);
			free(k);
			return;
		}
		wlr_keyboard_set_repeat_info(wlr, REPEAT_RATE, REPEAT_DELAY);
	}

	k->server = s;
	k->wlr = wlr;
	k->modifiers.notify = keyboard_modifiers;
	wl_signal_add(&wlr->events.modifiers, &k->modifiers);
	k->key.notify = keyboard_key;
	wl_signal_add(&wlr->events.key, &k->key);
	k->destroy.notify = keyboard_destroy;
	wl_signal_add(&wlr->base.events.destroy, &k->destroy);
	wl_list_insert(&s->keyboards, &k->link);
	wlr_seat_set_keyboard(s->seat, wlr);
}

void input_add_device(struct server *s, struct wlr_input_device *dev) {
	switch (dev->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		add_keyboard(s, wlr_keyboard_from_input_device(dev));
		break;
	case WLR_INPUT_DEVICE_POINTER:
		wlr_cursor_attach_input_device(s->cursor, dev);
		break;
	default:
		break;
	}
	update_caps(s);
}

static void new_input(struct wl_listener *listener, void *data) {
	struct server *s = wl_container_of(listener, s, new_input);
	input_add_device(s, data);
}

static void new_virtual_keyboard(struct wl_listener *listener, void *data) {
	struct server *s = wl_container_of(listener, s, new_virtual_keyboard);
	struct wlr_virtual_keyboard_v1 *vk = data;
	input_add_device(s, &vk->keyboard.base);
}

static void new_virtual_pointer(struct wl_listener *listener, void *data) {
	struct server *s = wl_container_of(listener, s, new_virtual_pointer);
	struct wlr_virtual_pointer_v1_new_pointer_event *e = data;
	input_add_device(s, &e->new_pointer->pointer.base);
}

static void cursor_motion(struct wl_listener *listener, void *data) {
	struct server *s = wl_container_of(listener, s, cursor_motion);
	struct wlr_pointer_motion_event *e = data;
	if (s->pointer_locked)
		return;
	wlr_cursor_move(s->cursor, &e->pointer->base, e->delta_x, e->delta_y);
	pointer_update(s, e->time_msec, 1);
}

static void cursor_motion_absolute(struct wl_listener *listener, void *data) {
	struct server *s = wl_container_of(listener, s, cursor_motion_absolute);
	struct wlr_pointer_motion_absolute_event *e = data;
	if (s->pointer_locked)
		return;
	wlr_cursor_warp_absolute(s->cursor, &e->pointer->base, e->x, e->y);
	pointer_update(s, e->time_msec, 1);
}

static void cursor_button(struct wl_listener *listener, void *data) {
	struct server *s = wl_container_of(listener, s, cursor_button);
	struct wlr_pointer_button_event *e = data;
	if (s->pointer_locked)
		return;
	wlr_seat_pointer_notify_button(s->seat, e->time_msec, e->button, e->state);
}

static void cursor_axis(struct wl_listener *listener, void *data) {
	struct server *s = wl_container_of(listener, s, cursor_axis);
	struct wlr_pointer_axis_event *e = data;
	if (s->pointer_locked)
		return;
	wlr_seat_pointer_notify_axis(s->seat, e->time_msec, e->orientation,
	                             e->delta, e->delta_discrete, e->source,
	                             e->relative_direction);
}

static void cursor_frame(struct wl_listener *listener, void *data) {
	(void)data;
	struct server *s = wl_container_of(listener, s, cursor_frame);
	if (s->pointer_locked)
		return;
	wlr_seat_pointer_notify_frame(s->seat);
}

static void swipe_begin(struct wl_listener *listener, void *data) {
	struct server *s = wl_container_of(listener, s, swipe_begin);
	struct wlr_pointer_swipe_begin_event *e = data;
	s->swipe_fingers = s->pointer_locked ? 0 : e->fingers;
	s->swipe_dx = s->swipe_dy = 0;
	if (s->swipe_fingers && s->swipe_fingers != 3)
		wlr_pointer_gestures_v1_send_swipe_begin(s->gestures, s->seat,
		                                         e->time_msec, e->fingers);
}

static void swipe_update(struct wl_listener *listener, void *data) {
	struct server *s = wl_container_of(listener, s, swipe_update);
	struct wlr_pointer_swipe_update_event *e = data;
	if (!s->swipe_fingers)
		return;
	if (s->swipe_fingers == 3) {
		s->swipe_dx += e->dx;
		s->swipe_dy += e->dy;
		return;
	}
	wlr_pointer_gestures_v1_send_swipe_update(s->gestures, s->seat,
	                                          e->time_msec, e->dx, e->dy);
}

static void swipe_end(struct wl_listener *listener, void *data) {
	struct server *s = wl_container_of(listener, s, swipe_end);
	struct wlr_pointer_swipe_end_event *e = data;
	uint32_t fingers = s->swipe_fingers;
	s->swipe_fingers = 0;
	if (!fingers)
		return;
	if (fingers != 3) {
		wlr_pointer_gestures_v1_send_swipe_end(s->gestures, s->seat,
		                                       e->time_msec, e->cancelled);
		return;
	}
	enum grid_dir dir;
	if (e->cancelled || grid_swipe(s->swipe_dx, s->swipe_dy, &dir) < 0 ||
	    desk_step(&s->desk, dir) < 0)
		return;
	server_arrange_all(s);
	input_focus(s, 1);
}

static void pinch_begin(struct wl_listener *listener, void *data) {
	struct server *s = wl_container_of(listener, s, pinch_begin);
	struct wlr_pointer_pinch_begin_event *e = data;
	if (!s->pointer_locked)
		wlr_pointer_gestures_v1_send_pinch_begin(s->gestures, s->seat,
		                                         e->time_msec, e->fingers);
}

static void pinch_update(struct wl_listener *listener, void *data) {
	struct server *s = wl_container_of(listener, s, pinch_update);
	struct wlr_pointer_pinch_update_event *e = data;
	if (!s->pointer_locked)
		wlr_pointer_gestures_v1_send_pinch_update(s->gestures, s->seat,
		                                          e->time_msec, e->dx, e->dy,
		                                          e->scale, e->rotation);
}

static void pinch_end(struct wl_listener *listener, void *data) {
	struct server *s = wl_container_of(listener, s, pinch_end);
	struct wlr_pointer_pinch_end_event *e = data;
	if (!s->pointer_locked)
		wlr_pointer_gestures_v1_send_pinch_end(s->gestures, s->seat,
		                                       e->time_msec, e->cancelled);
}

static void hold_begin(struct wl_listener *listener, void *data) {
	struct server *s = wl_container_of(listener, s, hold_begin);
	struct wlr_pointer_hold_begin_event *e = data;
	if (!s->pointer_locked)
		wlr_pointer_gestures_v1_send_hold_begin(s->gestures, s->seat,
		                                        e->time_msec, e->fingers);
}

static void hold_end(struct wl_listener *listener, void *data) {
	struct server *s = wl_container_of(listener, s, hold_end);
	struct wlr_pointer_hold_end_event *e = data;
	if (!s->pointer_locked)
		wlr_pointer_gestures_v1_send_hold_end(s->gestures, s->seat,
		                                      e->time_msec, e->cancelled);
}

static void request_set_cursor(struct wl_listener *listener, void *data) {
	struct server *s = wl_container_of(listener, s, request_set_cursor);
	struct wlr_seat_pointer_request_set_cursor_event *e = data;
	if (e->seat_client == s->seat->pointer_state.focused_client)
		wlr_cursor_set_surface(s->cursor, e->surface, e->hotspot_x,
		                       e->hotspot_y);
}

#define LISTEN(signal, field, fn)                                              \
	do {                                                                       \
		s->field.notify = fn;                                                  \
		wl_signal_add(&(signal), &s->field);                                   \
	} while (0)

int input_init(struct server *s) {
	wl_list_init(&s->keyboards);
	keys_init(&s->keys);
	s->cursor = wlr_cursor_create();
	s->cursor_mgr = wlr_xcursor_manager_create(NULL, CURSOR_SIZE);
	s->gestures = wlr_pointer_gestures_v1_create(s->display);
	s->virtual_keyboards = wlr_virtual_keyboard_manager_v1_create(s->display);
	s->virtual_pointers = wlr_virtual_pointer_manager_v1_create(s->display);
	if (!s->cursor || !s->cursor_mgr || !s->gestures || !s->virtual_keyboards ||
	    !s->virtual_pointers)
		return -1;
	wlr_cursor_attach_output_layout(s->cursor, s->output_layout);

	LISTEN(s->backend->events.new_input, new_input, new_input);
	LISTEN(s->cursor->events.motion, cursor_motion, cursor_motion);
	LISTEN(s->cursor->events.motion_absolute, cursor_motion_absolute,
	       cursor_motion_absolute);
	LISTEN(s->cursor->events.button, cursor_button, cursor_button);
	LISTEN(s->cursor->events.axis, cursor_axis, cursor_axis);
	LISTEN(s->cursor->events.frame, cursor_frame, cursor_frame);
	LISTEN(s->cursor->events.swipe_begin, swipe_begin, swipe_begin);
	LISTEN(s->cursor->events.swipe_update, swipe_update, swipe_update);
	LISTEN(s->cursor->events.swipe_end, swipe_end, swipe_end);
	LISTEN(s->cursor->events.pinch_begin, pinch_begin, pinch_begin);
	LISTEN(s->cursor->events.pinch_update, pinch_update, pinch_update);
	LISTEN(s->cursor->events.pinch_end, pinch_end, pinch_end);
	LISTEN(s->cursor->events.hold_begin, hold_begin, hold_begin);
	LISTEN(s->cursor->events.hold_end, hold_end, hold_end);
	LISTEN(s->seat->events.request_set_cursor, request_set_cursor,
	       request_set_cursor);
	LISTEN(s->virtual_keyboards->events.new_virtual_keyboard,
	       new_virtual_keyboard, new_virtual_keyboard);
	LISTEN(s->virtual_pointers->events.new_virtual_pointer, new_virtual_pointer,
	       new_virtual_pointer);
	s->input_ready = 1;
	update_caps(s);
	return 0;
}

void input_finish(struct server *s) {
	if (s->input_ready) {
		wl_list_remove(&s->new_input.link);
		wl_list_remove(&s->cursor_motion.link);
		wl_list_remove(&s->cursor_motion_absolute.link);
		wl_list_remove(&s->cursor_button.link);
		wl_list_remove(&s->cursor_axis.link);
		wl_list_remove(&s->cursor_frame.link);
		wl_list_remove(&s->swipe_begin.link);
		wl_list_remove(&s->swipe_update.link);
		wl_list_remove(&s->swipe_end.link);
		wl_list_remove(&s->pinch_begin.link);
		wl_list_remove(&s->pinch_update.link);
		wl_list_remove(&s->pinch_end.link);
		wl_list_remove(&s->hold_begin.link);
		wl_list_remove(&s->hold_end.link);
		wl_list_remove(&s->request_set_cursor.link);
		wl_list_remove(&s->new_virtual_keyboard.link);
		wl_list_remove(&s->new_virtual_pointer.link);
		s->input_ready = 0;
	}
	if (s->cursor_mgr)
		wlr_xcursor_manager_destroy(s->cursor_mgr);
	if (s->cursor)
		wlr_cursor_destroy(s->cursor);
	s->cursor_mgr = NULL;
	s->cursor = NULL;
}
