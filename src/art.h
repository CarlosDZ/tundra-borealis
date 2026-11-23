#ifndef TUNDRA_ART_H
#define TUNDRA_ART_H

#include <stddef.h>

struct tdraw_art;
struct tdraw_theme;

struct art {
	void *map;
	size_t len;
	struct tdraw_art *art;
};

void art_load(struct art *a, const char *name, const struct tdraw_theme *t);
void art_unload(struct art *a);

int art_open(struct art *a, const char *path, const struct tdraw_theme *t);

void art_user_dir(char *buf, unsigned long n);
const char *art_system_dir(void);

#endif
