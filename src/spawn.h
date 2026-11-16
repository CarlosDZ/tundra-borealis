#ifndef TUNDRA_SPAWN_H
#define TUNDRA_SPAWN_H

/* Starts cmd through the shell, detached. With wayland >= 0, that socket is
 * its own connection to borealis, as WAYLAND_SOCKET. 0, or -1. */
int spawn(const char *cmd, int wayland);

#endif
