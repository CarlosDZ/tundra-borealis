#ifndef TUNDRA_HEADER_H
#define TUNDRA_HEADER_H

#include <stdint.h>

struct server;
struct header;

int header_init(struct server *s);
void header_finish(struct server *s);

struct header *header_create(struct server *s);
void header_destroy(struct header *h, int scene_gone);

void header_place(struct header *h, int x, int y, int w, int screen_h,
                  int screen);
void header_show(struct header *h, int shown);
int header_height(const struct header *h);

void header_refresh(struct server *s);

#ifdef TUNDRA_BOREALIS_TEST
struct header_info {
	int x, y, w, h, line, shown;
	const char *font, *place, *clock;
};

void header_info(const struct header *h, struct header_info *i);
uint32_t header_pixel(const struct header *h, int x, int y);
#endif

#endif
