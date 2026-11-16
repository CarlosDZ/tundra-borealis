#ifndef TUNDRA_TESTSOCK_H
#define TUNDRA_TESTSOCK_H

struct server;

#ifdef TUNDRA_BOREALIS_TEST
int testsock_init(struct server *s);
void testsock_finish(void);
#else
static inline int testsock_init(struct server *s) {
	(void)s;
	return 0;
}
static inline void testsock_finish(void) {}
#endif

#endif
