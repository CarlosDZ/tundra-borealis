#ifndef TUNDRA_STATUS_H
#define TUNDRA_STATUS_H

#include "art.h"
#include "power.h"

struct server;
struct wl_event_source;

struct status {
	struct power power;
	struct art battery, brightness;
	const char *battery_state, *brightness_state;
	unsigned int drawn;
	int uevent_fd;
	struct wl_event_source *uevent_source, *frame_timer;
};

int status_init(struct server *s);
void status_finish(struct server *s);

void status_update(struct server *s);

void status_theme(struct server *s);

#endif
