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

int art_open(struct art *a, const char *path, const struct tdraw_theme *t) {
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

void art_user_dir(char *buf, unsigned long n) {
	const char *base = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
	if (base && *base)
		snprintf(buf, n, "%s/tundra/borealis/art", base);
	else if (home && *home)
		snprintf(buf, n, "%s/.config/tundra/borealis/art", home);
	else if (n)
		buf[0] = '\0';
}

const char *art_system_dir(void) {
	const char *sys = getenv("TUNDRA_BOREALIS_ART");
	return sys && *sys ? sys : TUNDRA_BOREALIS_ART;
}

void art_load(struct art *a, const char *name, const struct tdraw_theme *t) {
	char dir[4096], path[4200];
	int r = -1;

	memset(a, 0, sizeof(*a));
	art_user_dir(dir, sizeof(dir));
	if (dir[0]) {
		snprintf(path, sizeof(path), "%s/%s.toa", dir, name);
		r = art_open(a, path, t);
	}
	if (r < 0) {
		snprintf(path, sizeof(path), "%s/%s.toa", art_system_dir(), name);
		if (art_open(a, path, t) < 0)
			fprintf(stderr, "art: %s: no such file\n", path);
	}
}

void art_unload(struct art *a) {
	tdraw_art_free(a->art);
	if (a->map)
		munmap(a->map, a->len);
	memset(a, 0, sizeof(*a));
}
