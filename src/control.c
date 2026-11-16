#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "control.h"
#include "input.h"
#include "outputs.h"
#include "server.h"

#define CONTROL_LINE 64

struct conn {
	struct server *server;
	int fd;
	struct wl_event_source *source;
	char line[CONTROL_LINE];
	size_t len;
};

static int listen_fd = -1;
static struct wl_event_source *listen_source;
static char path[sizeof(((struct sockaddr_un *)0)->sun_path)];

int control_path(const char *display, char *out, size_t size) {
	const char *dir = getenv("XDG_RUNTIME_DIR");
	const char *slash = strrchr(display, '/');
	if (slash)
		display = slash + 1;
	if (!dir || !*dir)
		return -1;
	int n = snprintf(out, size, "%s/borealis-%s", dir, display);
	return n < 0 || (size_t)n >= size ? -1 : 0;
}

static void close_conn(struct conn *c) {
	wl_event_source_remove(c->source);
	close(c->fd);
	free(c);
}

static void reply(struct conn *c, const char *text) {
	size_t n = strlen(text);
	while (n) {
		ssize_t w = write(c->fd, text, n);
		if (w <= 0)
			return;
		text += w;
		n -= (size_t)w;
	}
}

static void order(struct conn *c) {
	struct server *s = c->server;
	char out[CONTROL_LINE];
	if (strncmp(c->line, "screens", 7) == 0 &&
	    (c->line[7] == '\0' || c->line[7] == ' ')) {
		if (c->line[7] == ' ') {
			int mirror;
			if (screens_mode(c->line + 8, &mirror) < 0) {
				reply(c, "error mirror or extended\n");
				return;
			}
			s->mirror = mirror;
			outputs_apply(s);
			input_focus(s, 0);
		}
		snprintf(out, sizeof(out), "screens %s\nok\n",
		         s->mirror ? "mirror" : "extended");
		reply(c, out);
		return;
	}
	reply(c, "error unknown order\n");
}

static int readable(int fd, uint32_t mask, void *data) {
	struct conn *c = data;
	if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
		close_conn(c);
		return 0;
	}
	ssize_t r = read(fd, c->line + c->len, sizeof(c->line) - 1 - c->len);
	if (r <= 0 && !(r < 0 && errno == EAGAIN)) {
		close_conn(c);
		return 0;
	}
	if (r > 0)
		c->len += (size_t)r;
	char *nl = memchr(c->line, '\n', c->len);
	if (!nl) {
		if (c->len == sizeof(c->line) - 1) {
			reply(c, "error line too long\n");
			close_conn(c);
		}
		return 0;
	}
	*nl = '\0';
	order(c);
	close_conn(c);
	return 0;
}

static int accepting(int fd, uint32_t mask, void *data) {
	(void)mask;
	struct server *s = data;
	int cfd = accept4(fd, NULL, NULL, SOCK_CLOEXEC | SOCK_NONBLOCK);
	if (cfd < 0)
		return 0;
	struct conn *c = calloc(1, sizeof(*c));
	if (!c) {
		close(cfd);
		return 0;
	}
	c->server = s;
	c->fd = cfd;
	c->source = wl_event_loop_add_fd(wl_display_get_event_loop(s->display), cfd,
	                                 WL_EVENT_READABLE, readable, c);
	if (!c->source) {
		close(cfd);
		free(c);
	}
	return 0;
}

int control_init(struct server *s, const char *display) {
	struct sockaddr_un a = {.sun_family = AF_UNIX};
	if (control_path(display, path, sizeof(path)) < 0) {
		fprintf(stderr, "no room for the control socket\n");
		return -1;
	}
	memcpy(a.sun_path, path, sizeof(path));
	unlink(path);
	listen_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
	if (listen_fd < 0 ||
	    bind(listen_fd, (struct sockaddr *)&a, sizeof(a)) < 0 ||
	    chmod(path, 0600) < 0 || listen(listen_fd, 8) < 0) {
		perror("control socket error");
		return -1;
	}
	listen_source =
	    wl_event_loop_add_fd(wl_display_get_event_loop(s->display), listen_fd,
	                         WL_EVENT_READABLE, accepting, s);
	return listen_source ? 0 : -1;
}

void control_finish(void) {
	if (listen_source)
		wl_event_source_remove(listen_source);
	if (listen_fd >= 0) {
		close(listen_fd);
		unlink(path);
	}
	listen_source = NULL;
	listen_fd = -1;
}
