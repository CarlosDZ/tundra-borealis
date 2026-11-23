#include <linux/netlink.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <tundra/draw.h>
#include <unistd.h>
#include <wayland-server-core.h>

#include "header.h"
#include "server.h"
#include "status.h"

static const char *sys_dir(void) {
	const char *d = getenv("TUNDRA_BOREALIS_SYS");
	return d && *d ? d : "/sys";
}

static uint64_t now_ms(void) {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint64_t)t.tv_sec * 1000 + (uint64_t)t.tv_nsec / 1000000;
}

static int same(const char *a, const char *b) {
	return a == b || (a && b && !strcmp(a, b));
}

static int advance(struct status *st) {
	struct tdraw_art *arts[2] = {st->battery.art, st->brightness.art};
	uint64_t now = now_ms();
	int next = -1;
	for (int i = 0; i < 2; i++) {
		struct tdraw_rect r;
		int ms;
		if (!arts[i])
			continue;
		ms = tdraw_art_advance(arts[i], now, &r);
		if (r.w && r.h)
			st->drawn++;
		if (ms >= 0 && (next < 0 || ms < next))
			next = ms;
	}
	return next;
}

static void schedule(struct status *st, int ms) {
	if (st->frame_timer)
		wl_event_source_timer_update(st->frame_timer, ms < 0   ? 0
		                                              : ms < 1 ? 1
		                                                       : ms);
}

static int on_frame(void *data) {
	struct server *s = data;
	schedule(&s->status, advance(&s->status));
	header_refresh(s);
	return 0;
}

static void play(struct art *a, const char *name, const char *state,
                 uint64_t now) {
	if (a->art && state && tdraw_art_play(a->art, state, now) < 0)
		fprintf(stderr, "art: %s has no animation %s\n", name, state);
}

void status_update(struct server *s) {
	struct status *st = &s->status;
	uint64_t now = now_ms();
	power_read(sys_dir(), &st->power);
	const char *b = power_battery_state(&st->power);
	const char *l = power_brightness_state(&st->power);
	if (!same(b, st->battery_state))
		play(&st->battery, "battery", b, now);
	if (!same(l, st->brightness_state))
		play(&st->brightness, "brightness", l, now);
	st->battery_state = b;
	st->brightness_state = l;
	schedule(st, advance(st));
	header_refresh(s);
}

void status_theme(struct server *s) {
	struct status *st = &s->status;
	if (st->battery.art && tdraw_art_theme(st->battery.art, s->theme) == 1)
		st->drawn++;
	if (st->brightness.art &&
	    tdraw_art_theme(st->brightness.art, s->theme) == 1)
		st->drawn++;
}

static int on_uevent(int fd, uint32_t mask, void *data) {
	struct server *s = data;
	char buf[8192];
	ssize_t n;
	int mine = 0;
	(void)mask;
	while ((n = recv(fd, buf, sizeof(buf), MSG_DONTWAIT)) > 0)
		mine |= power_uevent(buf, (size_t)n);
	if (mine)
		status_update(s);
	return 0;
}

static int open_uevent(void) {
	struct sockaddr_nl a = {.nl_family = AF_NETLINK, .nl_groups = 1};
	int fd = socket(AF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
	                NETLINK_KOBJECT_UEVENT);
	if (fd < 0)
		return -1;
	if (bind(fd, (struct sockaddr *)&a, sizeof(a)) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

int status_init(struct server *s) {
	struct status *st = &s->status;
	struct wl_event_loop *loop = wl_display_get_event_loop(s->display);
	memset(st, 0, sizeof(*st));
	st->uevent_fd = -1;
	art_load(&st->battery, "battery", s->theme);
	art_load(&st->brightness, "brightness", s->theme);
	st->frame_timer = wl_event_loop_add_timer(loop, on_frame, s);
	if (!st->frame_timer) {
		fprintf(stderr, "cannot time the icons\n");
		return -1;
	}
	st->uevent_fd = open_uevent();
	if (st->uevent_fd >= 0)
		st->uevent_source = wl_event_loop_add_fd(
		    loop, st->uevent_fd, WL_EVENT_READABLE, on_uevent, s);
	if (!st->uevent_source)
		fprintf(stderr, "uevents: not heard, battery and brightness read "
		                "each minute\n");
	status_update(s);
	return 0;
}

void status_finish(struct server *s) {
	struct status *st = &s->status;
	if (st->uevent_source)
		wl_event_source_remove(st->uevent_source);
	if (st->frame_timer)
		wl_event_source_remove(st->frame_timer);
	if (st->uevent_fd >= 0)
		close(st->uevent_fd);
	art_unload(&st->battery);
	art_unload(&st->brightness);
	st->uevent_source = st->frame_timer = NULL;
	st->uevent_fd = -1;
}
