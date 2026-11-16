#ifndef TUNDRA_CONTROL_H
#define TUNDRA_CONTROL_H

#include <stddef.h>

struct server;

/* $XDG_RUNTIME_DIR/borealis-<display>, for tundra-borealis screens. Its one
 * order is "screens [mirror | extended]"; the answer is "screens <mode>" and
 * "ok", or "error <why>". 0, or -1. */
int control_init(struct server *s, const char *display);
void control_finish(void);

/* The path of that socket for a display, or -1 if it does not fit. */
int control_path(const char *display, char *out, size_t size);

#endif
