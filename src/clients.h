#ifndef TUNDRA_CLIENTS_H
#define TUNDRA_CLIENTS_H

#include <wayland-server-core.h>

#include "fixed.h"

struct server;

struct fixed_run {
	struct server *server;
	int canvas, prog;
	struct wl_client *client;
	struct wl_listener client_destroy;
	struct wl_event_source *restart;
	struct fixed_deaths deaths;
	int covered;
	struct wlr_scene_rect *cover;
};

void clients_load(struct server *s);
void clients_start(struct server *s);
void clients_finish(struct server *s);

struct fixed_run *clients_run_of(struct server *s, struct wl_client *client);

#endif
