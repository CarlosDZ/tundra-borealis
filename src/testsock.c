#ifdef TUNDRA_BOREALIS_TEST
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include "testsock.h"
#include "xdg.h"

#define TESTSOCK_NAME "borealis-test"
#define TESTSOCK_LINE 256

static int listen_fd = -1;
static struct wl_event_source *source;
static char path[sizeof(((struct sockaddr_un *)0)->sun_path)];

static char type_of(struct server *s, unsigned int id) {
	int c = desk_find(&s->desk, id);
	if (c < 0)
		return '-';
	for (int i = 0; i < s->desk.canvas[c].n; i++)
		if (s->desk.canvas[c].w[i].id == id)
			return s->desk.canvas[c].w[i].type;
	return '-';
}

static void list_windows(struct server *s, FILE *f) {
	struct window *w;
	wl_list_for_each(w, &s->windows, link) {
		if (!w->placed || !w->toplevel->base->surface->mapped)
			continue;
		struct grid_pos p = desk_pos(desk_find(&s->desk, w->id));
		struct wlr_box g = w->toplevel->base->geometry;
		int sw = g.width < w->box.width ? g.width : w->box.width;
		int sh = g.height < w->box.height ? g.height : w->box.height;
		const char *app = w->toplevel->app_id;
		fprintf(f, "window %u %s %d%d %c %d %d %d %d %d %d\n", w->id,
		        app && *app ? app : "-", p.h, p.d, type_of(s, w->id), w->box.x,
		        w->box.y, w->box.width, w->box.height, sw, sh);
	}
}

static void answer(struct server *s, int fd) {
	char line[TESTSOCK_LINE];
	size_t len = 0;
	while (len < sizeof(line) - 1) {
		ssize_t r = read(fd, line + len, 1);
		if (r <= 0 || line[len] == '\n')
			break;
		len++;
	}
	line[len] = '\0';

	FILE *f = fdopen(dup(fd), "w");
	if (!f)
		return;
	if (strcmp(line, "windows") == 0) {
		list_windows(s, f);
		fprintf(f, "ok\n");
	} else {
		fprintf(f, "error unknown command: %s\n", line);
	}
	fclose(f);
}

static int on_accept(int fd, uint32_t mask, void *data) {
	(void)mask;
	int c = accept4(fd, NULL, NULL, SOCK_CLOEXEC);
	if (c < 0)
		return 0;
	struct timeval tv = {1, 0};
	setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	answer(data, c);
	close(c);
	return 0;
}

int testsock_init(struct server *s) {
	const char *dir = getenv("XDG_RUNTIME_DIR");
	struct sockaddr_un a = {.sun_family = AF_UNIX};
	if (!dir || snprintf(path, sizeof(path), "%s/%s", dir, TESTSOCK_NAME) >=
	                (int)sizeof(path)) {
		fprintf(stderr, "no room for the test socket\n");
		return -1;
	}
	memcpy(a.sun_path, path, sizeof(path));
	unlink(path);
	listen_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
	if (listen_fd < 0 ||
	    bind(listen_fd, (struct sockaddr *)&a, sizeof(a)) < 0 ||
	    listen(listen_fd, 8) < 0) {
		perror("test socket error");
		return -1;
	}
	source = wl_event_loop_add_fd(wl_display_get_event_loop(s->display),
	                              listen_fd, WL_EVENT_READABLE, on_accept, s);
	return source ? 0 : -1;
}

void testsock_finish(void) {
	if (source)
		wl_event_source_remove(source);
	if (listen_fd >= 0) {
		close(listen_fd);
		unlink(path);
	}
	source = NULL;
	listen_fd = -1;
}
#endif
