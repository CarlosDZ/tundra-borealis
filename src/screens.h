#ifndef TUNDRA_SCREENS_H
#define TUNDRA_SCREENS_H

#include <stddef.h>

#define SCREENS_ERR_MAX 160

struct screens_conf {
	int mirror;
	int second_right;
};

void screens_default(struct screens_conf *c);

/* "mode = extended | mirror" and "second = left | right"; what is not there
 * keeps its default. 0, or -1 with err set and c at its defaults. */
int screens_parse(const char *text, size_t len, struct screens_conf *c,
                  char *err);

/* "mirror" or "extended": 0 and *mirror, or -1. */
int screens_mode(const char *word, int *mirror);

#endif
