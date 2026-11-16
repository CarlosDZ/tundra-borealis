#ifndef TUNDRA_CLIENTS_H
#define TUNDRA_CLIENTS_H

#include <wayland-server-core.h>

#include "fixed.h"

struct server;

struct fixed_run {
	struct server *server;
	int canvas, prog;
	struct wl_client *client; /* NULL while it is not running */
	struct wl_listener client_destroy;
	struct wl_event_source *restart;
	struct fixed_deaths deaths;
	int covered;
	struct wlr_scene_rect *cover;
};

/* Reads the fixed canvases and marks them on the desk. A file not owned by
 * root, or writable by others, is ignored. */
void clients_load(struct server *s);
void clients_start(struct server *s);
void clients_finish(struct server *s);

/* The program a client runs, or NULL for any other client. */
struct fixed_run *clients_run_of(struct server *s, struct wl_client *client);

/* From now on a client sees a privileged global only if it is the program of
 * a fixed canvas given that privilege. */
void clients_filter_globals(struct server *s);

#endif
