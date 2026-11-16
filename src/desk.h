#ifndef TUNDRA_DESK_H
#define TUNDRA_DESK_H

#include "fixed.h"
#include "grid.h"
#include "slots.h"

#define DESK_CANVASES (GRID_SIDE * GRID_SIDE)

struct desk {
	struct grid grid;
	struct slots canvas[DESK_CANVASES];
	unsigned int focus[DESK_CANVASES];      /* 0: the canvas focuses nothing */
	unsigned int fullscreen[DESK_CANVASES]; /* 0: none */
	const struct fixed_canvas *fixed[DESK_CANVASES]; /* NULL: a working one */
	unsigned int pinned[DESK_CANVASES][FIXED_MAX];   /* 0: no window */
	int w, h;
};

int desk_index(struct grid_pos p);
struct grid_pos desk_pos(int index);

void desk_init(struct desk *d, int w, int h);
void desk_resize(struct desk *d, int w, int h);

int desk_find(const struct desk *d, unsigned int id);
/* A fixed canvas takes no window but those of its programs, pinned to their
 * place; none of them moves, rotates or goes to another canvas. */
void desk_set_fixed(struct desk *d, int index, const struct fixed_canvas *c);
/* 0, or -1 if index is not fixed, the program is taken or id is on the desk. */
int desk_pin(struct desk *d, int index, int prog, unsigned int id);

/* The focused window of the focused canvas, or 0. */
unsigned int desk_focused(const struct desk *d);

/* Opens id on the focused canvas and focuses it. Its canvas, or -1. */
int desk_open(struct desk *d, unsigned int id, char type);
/* Removes id; the next window clockwise takes its focus. Its canvas, or -1. */
int desk_close(struct desk *d, unsigned int id);

/* The pointer went over id: it takes the focus of its canvas, and that canvas
 * the keyboard. 0, or -1 if id is not on the desk. */
int desk_focus_window(struct desk *d, unsigned int id);

/* Super + F, or the client asking: id covers its canvas or stops doing so. A
 * canvas has one at most; opening a window on it, closing id or sending it
 * away ends it. The canvas, or -1 if id is not on the desk or nothing changes.
 */
int desk_fullscreen(struct desk *d, unsigned int id, int on);

/* Super + Tab, Super + Shift + Tab and Super + R on the focused window, never
 * while its canvas has a window in fullscreen. The canvas they change, or -1
 * when there is nothing to do. */
int desk_focus_next(struct desk *d);
int desk_move(struct desk *d);
int desk_rotate(struct desk *d);

/* The view goes to another canvas. 0, or -1 at an edge or out of the grid. */
int desk_step(struct desk *d, enum grid_dir dir);
int desk_goto(struct desk *d, struct grid_pos p);

/* The focused window goes to another canvas and the view follows it. 0, or -1
 * with no window, at an edge, onto its own canvas or onto a full one. */
int desk_send_step(struct desk *d, enum grid_dir dir);
int desk_send(struct desk *d, struct grid_pos to);

int desk_place(const struct desk *d, int index, struct slots_place *out,
               struct layout *l);

#endif
