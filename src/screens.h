#ifndef TUNDRA_SCREENS_H
#define TUNDRA_SCREENS_H

#include <stddef.h>

#define SCREENS_ERR_MAX 160

struct screens_conf {
	int mirror;
	int second_right;
};

void screens_default(struct screens_conf *c);

int screens_parse(const char *text, size_t len, struct screens_conf *c,
                  char *err);

int screens_mode(const char *word, int *mirror);

#endif
