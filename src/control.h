#ifndef TUNDRA_CONTROL_H
#define TUNDRA_CONTROL_H

#include <stddef.h>

struct server;

int control_init(struct server *s, const char *display);
void control_finish(void);

int control_path(const char *display, char *out, size_t size);

#endif
