#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <wlr/types/wlr_scene.h>

#include "clients.h"
#include "server.h"
#include "spawn.h"

#define FIXED_FILE_MAX 65536

static uint64_t now_msec(void) {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint64_t)t.tv_sec * 1000 + (uint64_t)t.tv_nsec / 1000000;
}

static const char *fixed_dir(void) {
#ifdef TUNDRA_BOREALIS_TEST
	const char *dir = getenv("TUNDRA_BOREALIS_DIR");
	if (dir && *dir)
		return dir;
#endif
	return TUNDRA_BOREALIS_DIR;
}

static int trusted(const struct stat *st) {
#ifndef TUNDRA_BOREALIS_TEST
	if (st->st_uid != 0)
		return 0;
#endif
	return !(st->st_mode & (S_IWGRP | S_IWOTH));
}

static int read_canvas(int dir, const char *name, struct fixed_canvas *c,
                       char *err) {
	static char text[FIXED_FILE_MAX + 1];
	int fd = openat(dir, name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
	if (fd < 0) {
		snprintf(err, FIXED_ERR_MAX, "%s", strerror(errno));
		return -1;
	}
	struct stat st;
	if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode) || !trusted(&st)) {
		close(fd);
		snprintf(err, FIXED_ERR_MAX,
		         "not a file of root that only root can write");
		return -1;
	}
	size_t len = 0;
	ssize_t r;
	while (len <= FIXED_FILE_MAX &&
	       (r = read(fd, text + len, FIXED_FILE_MAX + 1 - len)) > 0)
		len += (size_t)r;
	close(fd);
	if (len > FIXED_FILE_MAX) {
		snprintf(err, FIXED_ERR_MAX, "bigger than %d bytes", FIXED_FILE_MAX);
		return -1;
	}
	return fixed_parse(text, len, c, err);
}

void clients_load(struct server *s) {
	const char *path = fixed_dir();
	DIR *dir = opendir(path);
	if (!dir) {
		if (errno != ENOENT)
			fprintf(stderr, "%s: %s\n", path, strerror(errno));
		return;
	}
	struct stat st;
	if (fstat(dirfd(dir), &st) < 0 || !trusted(&st)) {
		fprintf(stderr, "%s: only root may write it, ignored\n", path);
		closedir(dir);
		return;
	}

	int ok[DESK_CANVASES] = {0}, n = 0;
	struct dirent *e;
	while ((e = readdir(dir))) {
		int c = fixed_file_canvas(e->d_name);
		if (c < 0)
			continue;
		char err[FIXED_ERR_MAX];
		if (read_canvas(dirfd(dir), e->d_name, &s->fixed[c], err) < 0) {
			fprintf(stderr, "%s/%s: %s, it stays a working canvas\n", path,
			        e->d_name, err);
			continue;
		}
		ok[c] = 1;
		n++;
	}
	closedir(dir);
	if (n == DESK_CANVASES) {
		fprintf(stderr,
		        "%s: all 16 canvases fixed, none taken: one has to "
		        "stay a working canvas\n",
		        path);
		return;
	}
	for (int c = 0; c < DESK_CANVASES; c++)
		if (ok[c])
			desk_set_fixed(&s->desk, c, &s->fixed[c]);
}

static void launch(struct fixed_run *r);

static void relaunch(void *data) {
	struct fixed_run *r = data;
	r->restart = NULL;
	launch(r);
}

static void client_gone(struct wl_listener *listener, void *data) {
	(void)data;
	struct fixed_run *r = wl_container_of(listener, r, client_destroy);
	struct server *s = r->server;
	wl_list_remove(&r->client_destroy.link);
	r->client = NULL;
	if (s->stopping)
		return;
	if (fixed_died(&r->deaths, now_msec())) {
		if (!r->restart)
			r->restart = wl_event_loop_add_idle(
			    wl_display_get_event_loop(s->display), relaunch, r);
		return;
	}
	r->covered = 1;
	server_arrange(s, r->canvas);
}

static void launch(struct fixed_run *r) {
	struct server *s = r->server;
	const struct fixed_prog *p = &s->fixed[r->canvas].p[r->prog];
	int sv[2];
	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) < 0) {
		perror("socketpair error");
		return;
	}
	r->client = wl_client_create(s->display, sv[0]);
	if (!r->client) {
		fprintf(stderr, "%s: cannot make its connection\n", p->name);
		close(sv[0]);
		close(sv[1]);
		return;
	}
	r->client_destroy.notify = client_gone;
	wl_client_add_destroy_listener(r->client, &r->client_destroy);
	if (spawn(p->command, sv[1]) < 0)
		fprintf(stderr, "%s: cannot start it\n", p->name);
	close(sv[1]);
}

void clients_start(struct server *s) {
	for (int c = 0; c < DESK_CANVASES; c++) {
		if (!s->desk.fixed[c])
			continue;
		for (int i = 0; i < s->fixed[c].n; i++) {
			struct fixed_run *r = &s->runs[c][i];
			r->server = s;
			r->canvas = c;
			r->prog = i;
			launch(r);
		}
	}
}

void clients_finish(struct server *s) {
	for (int c = 0; c < DESK_CANVASES; c++) {
		for (int i = 0; i < FIXED_MAX; i++) {
			struct fixed_run *r = &s->runs[c][i];
			if (r->restart)
				wl_event_source_remove(r->restart);
			r->restart = NULL;
		}
	}
}

struct fixed_run *clients_run_of(struct server *s, struct wl_client *client) {
	for (int c = 0; c < DESK_CANVASES; c++)
		for (int i = 0; i < FIXED_MAX; i++)
			if (s->runs[c][i].client == client)
				return &s->runs[c][i];
	return NULL;
}
