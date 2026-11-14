#ifndef TUNDRA_SERVER_H
#define TUNDRA_SERVER_H

/* Runs the compositor until SIGTERM or SIGINT. 0, or -1 if it cannot start. */
int server_run(void);

#endif
