#include "pick.h"

int pick_scale(int w, int h, int hw, int hh) {
	int sx, sy;
	if (w < 1 || h < 1 || hw < 1 || hh < 1)
		return 0;
	sx = hw / w;
	sy = hh / h;
	return sx < sy ? sx : sy;
}

static long long covered(const struct pick_size *d, int s) {
	return (long long)d->w * s * d->h * s;
}

int pick(const struct pick_size *d, int n, int hw, int hh, unsigned int r,
         int *scale) {
	long long best = 0;
	int fit = 0;
	for (int i = 0; i < n; i++) {
		int s = pick_scale(d[i].w, d[i].h, hw, hh);
		if (s && covered(&d[i], s) > best)
			best = covered(&d[i], s);
	}
	if (!best)
		return -1;
	for (int i = 0; i < n; i++) {
		int s = pick_scale(d[i].w, d[i].h, hw, hh);
		fit += s && covered(&d[i], s) * 5 >= best * 4;
	}
	r %= (unsigned int)fit;
	for (int i = 0; i < n; i++) {
		int s = pick_scale(d[i].w, d[i].h, hw, hh);
		if (s && covered(&d[i], s) * 5 >= best * 4 && !r--) {
			*scale = s;
			return i;
		}
	}
	return -1;
}
