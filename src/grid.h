#ifndef TUNDRA_GRID_H
#define TUNDRA_GRID_H

/* Horizontals and depths: 1 to GRID_SIDE on each axis. */
#define GRID_SIDE 4

/* With two extended screens the odd horizontals go to the second one. */
enum grid_screens { GRID_ONE, GRID_MIRROR, GRID_EXTENDED };

enum grid_dir { GRID_LEFT, GRID_RIGHT, GRID_DEEPER, GRID_SHALLOWER };

#define GRID_MAIN 0
#define GRID_SECOND 1

struct grid_pos {
	int h, d;
};

struct grid {
	struct grid_pos focus;
	enum grid_screens screens;
};

void grid_init(struct grid *g);
void grid_set_screens(struct grid *g, enum grid_screens screens);

int grid_valid(struct grid_pos p);
int grid_same(struct grid_pos a, struct grid_pos b);

/* Where a step from the focused canvas lands. 0 and *to, or -1 at an edge:
 * neither axis wraps around. With extended screens left and right jump from
 * pair to pair and keep the screen. */
int grid_step(const struct grid *g, enum grid_dir dir, struct grid_pos *to);

int grid_goto(struct grid *g, struct grid_pos p);

/* Extended screens: the focus goes to the other canvas of the pair. 0, or -1
 * with a single or mirrored screen. */
int grid_other_screen(struct grid *g);

struct grid_pos grid_shown(const struct grid *g, int screen);
int grid_focused_screen(const struct grid *g);
int grid_is_shown(const struct grid *g, struct grid_pos p);

#endif
