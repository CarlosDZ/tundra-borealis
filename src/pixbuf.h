#ifndef TUNDRA_PIXBUF_H
#define TUNDRA_PIXBUF_H

#include <stddef.h>
#include <stdint.h>
#include <wlr/types/wlr_buffer.h>

struct tdraw_canvas;

struct pixbuf {
	struct wlr_buffer base;
	uint32_t *px;
	size_t stride;
	struct tdraw_canvas *canvas;
};

struct pixbuf *pixbuf_new(int w, int h);
void pixbuf_drop(struct pixbuf *b);

struct pixbuf *pixbuf_spare(struct pixbuf *pair[2], int *on, int w, int h);

#endif
