#ifndef TUNDRA_PICK_H
#define TUNDRA_PICK_H

struct pick_size {
	int w, h;
};

int pick_scale(int w, int h, int hw, int hh);

int pick(const struct pick_size *d, int n, int hw, int hh, unsigned int r,
         int *scale);

#endif
