#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <tundra/draw.h>
#include <unistd.h>

#include "art.h"

#ifndef TUNDRA_BOREALIS_ART
#define TUNDRA_BOREALIS_ART "/usr/share/tundra/borealis/art"
#endif

static int try_open(struct art *a, const char *path,
                    const struct tdraw_theme *t) {
	const char *err;
	struct stat st;
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		if (errno == ENOENT)
			return -1;
		fprintf(stderr, "art: %s: %s\n", path, strerror(errno));
		return 1;
	}
	if (fstat(fd, &st) < 0 || st.st_size <= 0) {
		fprintf(stderr, "art: %s: empty or unreadable\n", path);
		close(fd);
		return 1;
	}
	a->len = (size_t)st.st_size;
	a->map = mmap(NULL, a->len, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (a->map == MAP_FAILED) {
		a->map = NULL;
		fprintf(stderr, "art: %s: %s\n", path, strerror(errno));
		return 1;
	}
	if (!(a->art = tdraw_art_open(a->map, a->len, t, &err))) {
		fprintf(stderr, "art: %s: %s\n", path, err);
		munmap(a->map, a->len);
		a->map = NULL;
		return 1;
	}
	return 0;
}

void art_load(struct art *a, const char *name, const struct tdraw_theme *t) {
	const char *base = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
	const char *sys = getenv("TUNDRA_BOREALIS_ART");
	char path[4096];
	int r = -1;

	memset(a, 0, sizeof(*a));
	if (base && *base)
		snprintf(path, sizeof(path), "%s/tundra/borealis/art/%s.toa", base,
		         name);
	else if (home && *home)
		snprintf(path, sizeof(path), "%s/.config/tundra/borealis/art/%s.toa",
		         home, name);
	else
		path[0] = '\0';
	if (path[0])
		r = try_open(a, path, t);
	if (r < 0) {
		snprintf(path, sizeof(path), "%s/%s.toa",
		         sys && *sys ? sys : TUNDRA_BOREALIS_ART, name);
		if (try_open(a, path, t) < 0)
			fprintf(stderr, "art: %s: no such file\n", path);
	}
}

void art_unload(struct art *a) {
	tdraw_art_free(a->art);
	if (a->map)
		munmap(a->map, a->len);
	memset(a, 0, sizeof(*a));
}
