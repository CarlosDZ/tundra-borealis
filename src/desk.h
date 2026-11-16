#ifndef TUNDRA_DESK_H
#define TUNDRA_DESK_H

#include "fixed.h"
#include "grid.h"
#include "slots.h"

#define DESK_CANVASES (GRID_SIDE * GRID_SIDE)

struct desk {
	struct grid grid;
	struct slots canvas[DESK_CANVASES];
	unsigned int focus[DESK_CANVASES];
	unsigned int fullscreen[DESK_CANVASES];
	const struct fixed_canvas *fixed[DESK_CANVASES];
	unsigned int pinned[DESK_CANVASES][FIXED_MAX];
	int w, h;
};

int desk_index(struct grid_pos p);
struct grid_pos desk_pos(int index);

void desk_init(struct desk *d, int w, int h);
void desk_resize(struct desk *d, int w, int h);

int desk_find(const struct desk *d, unsigned int id);
void desk_set_fixed(struct desk *d, int index, const struct fixed_canvas *c);
int desk_pin(struct desk *d, int index, int prog, unsigned int id);

unsigned int desk_focused(const struct desk *d);

int desk_open(struct desk *d, unsigned int id, char type);
int desk_close(struct desk *d, unsigned int id);

int desk_focus_window(struct desk *d, unsigned int id);

int desk_fullscreen(struct desk *d, unsigned int id, int on);

int desk_focus_next(struct desk *d);
int desk_move(struct desk *d);
int desk_rotate(struct desk *d);

int desk_step(struct desk *d, enum grid_dir dir);
int desk_goto(struct desk *d, struct grid_pos p);

int desk_send_step(struct desk *d, enum grid_dir dir);
int desk_send(struct desk *d, struct grid_pos to);

int desk_place(const struct desk *d, int index, struct slots_place *out,
               struct layout *l);

#endif
