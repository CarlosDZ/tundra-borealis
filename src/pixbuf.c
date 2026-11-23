#include <drm_fourcc.h>
#include <stdlib.h>
#include <tundra/draw.h>
#include <wlr/interfaces/wlr_buffer.h>

#include "pixbuf.h"

static void destroy(struct wlr_buffer *base) {
	struct pixbuf *b = wl_container_of(base, b, base);
	tdraw_canvas_free(b->canvas);
	free(b->px);
	free(b);
}

static bool begin(struct wlr_buffer *base, uint32_t flags, void **data,
                  uint32_t *format, size_t *stride) {
	struct pixbuf *b = wl_container_of(base, b, base);
	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE)
		return false;
	*data = b->px;
	*format = DRM_FORMAT_ARGB8888;
	*stride = b->stride;
	return true;
}

static void end(struct wlr_buffer *base) { (void)base; }

static const struct wlr_buffer_impl impl = {
    .destroy = destroy,
    .begin_data_ptr_access = begin,
    .end_data_ptr_access = end,
};

struct pixbuf *pixbuf_new(int w, int h) {
	struct pixbuf *b = calloc(1, sizeof(*b));
	if (!b)
		return NULL;
	b->stride = (size_t)w * 4;
	b->px = calloc((size_t)h, b->stride);
	b->canvas = b->px ? tdraw_canvas_new(b->px, w, h, (int)b->stride) : NULL;
	if (!b->canvas) {
		free(b->px);
		free(b);
		return NULL;
	}
	wlr_buffer_init(&b->base, &impl, w, h);
	return b;
}

void pixbuf_drop(struct pixbuf *b) {
	if (b)
		wlr_buffer_drop(&b->base);
}

struct pixbuf *pixbuf_spare(struct pixbuf *pair[2], int *on, int w, int h) {
	int i = *on == 0 ? 1 : 0;
	struct pixbuf *b = pair[i];
	if (b && (b->base.width != w || b->base.height != h || b->base.n_locks)) {
		pixbuf_drop(b);
		b = pair[i] = NULL;
	}
	if (!b)
		b = pair[i] = pixbuf_new(w, h);
	if (b)
		*on = i;
	return b;
}
