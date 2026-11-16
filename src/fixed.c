#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "fixed.h"
#include "grid.h"

static struct tconf parsed;

static const struct {
	const char *name;
	unsigned int bit;
} privs[] = {
    {"overlay", FIXED_OVERLAY},
    {"capture", FIXED_CAPTURE},
    {"virtual-input", FIXED_VIRTUAL_INPUT},
    {"window-control", FIXED_WINDOW_CONTROL},
    {"lock", FIXED_LOCK},
};

static int fail(struct fixed_canvas *c, char *err, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(err, FIXED_ERR_MAX, fmt, ap);
	va_end(ap);
	c->n = 0;
	return -1;
}

int fixed_file_canvas(const char *name) {
	if (strncmp(name, "canvas-", 7) != 0 || strlen(name) != 9)
		return -1;
	int h = name[7] - '0', d = name[8] - '0';
	if (h < 1 || h > GRID_SIDE || d < 1 || d > GRID_SIDE)
		return -1;
	return (h - 1) * GRID_SIDE + d - 1;
}

static int number(const char **s, int *out) {
	const char *p = *s;
	while (*p == ' ')
		p++;
	if (*p < '0' || *p > '9')
		return -1;
	int v = 0;
	while (*p >= '0' && *p <= '9') {
		v = v * 10 + (*p - '0');
		if (v > 1000)
			return -1;
		p++;
	}
	*out = v;
	*s = p;
	return 0;
}

static int pair(const char *value, int *a, int *b) {
	const char *p = value;
	if (number(&p, a) < 0 || *p != ' ' || number(&p, b) < 0)
		return -1;
	while (*p == ' ')
		p++;
	return *p ? -1 : 0;
}

static int privileges(char *value, unsigned int *out) {
	*out = 0;
	if (strcmp(value, "none") == 0)
		return 0;
	for (char *it = strtok(value, ","); it; it = strtok(NULL, ",")) {
		while (*it == ' ')
			it++;
		size_t n = strlen(it);
		while (n && it[n - 1] == ' ')
			it[--n] = '\0';
		size_t i = 0;
		while (i < sizeof(privs) / sizeof(privs[0]) &&
		       strcmp(privs[i].name, it) != 0)
			i++;
		if (i == sizeof(privs) / sizeof(privs[0]) || (*out & privs[i].bit))
			return -1;
		*out |= privs[i].bit;
	}
	return *out ? 0 : -1;
}

static int overlap(const struct fixed_prog *a, const struct fixed_prog *b) {
	return a->x < b->x + b->w && b->x < a->x + a->w && a->y < b->y + b->h &&
	       b->y < a->y + a->h;
}

#define SEEN_COMMAND 1
#define SEEN_PLACE 2
#define SEEN_SIZE 4
#define SEEN_LEVEL 8
#define SEEN_PRIVILEGES 16
#define SEEN_NEEDED (SEEN_COMMAND | SEEN_PLACE | SEEN_SIZE | SEEN_PRIVILEGES)

int fixed_parse(const char *text, size_t len, struct fixed_canvas *c,
                char *err) {
	int seen[FIXED_MAX] = {0};
	c->n = 0;
	if (tconf_parse(text, len, &parsed) < 0)
		return fail(c, err, "line %d: %s", parsed.err_line, parsed.err);
	if (parsed.nblocks == 0)
		return fail(c, err, "no programs");
	if (parsed.nblocks > FIXED_MAX)
		return fail(c, err, "more than %d programs", FIXED_MAX);

	for (int b = 0; b < parsed.nblocks; b++) {
		for (int o = 0; o < b; o++)
			if (strcmp(parsed.blocks[o].name, parsed.blocks[b].name) == 0)
				return fail(c, err, "line %d: %s twice", parsed.blocks[b].line,
				            parsed.blocks[b].name);
		struct fixed_prog *p = &c->p[b];
		memset(p, 0, sizeof(*p));
		memcpy(p->name, parsed.blocks[b].name, sizeof(p->name));
	}
	c->n = parsed.nblocks;

	for (int i = 0; i < parsed.n; i++) {
		struct tconf_item *it = &parsed.items[i];
		if (it->block < 0)
			return fail(c, err, "line %d: %s outside a program", it->line,
			            it->key);
		struct fixed_prog *p = &c->p[it->block];
		int *s = &seen[it->block];
		int bit;
		if (strcmp(it->key, "command") == 0)
			bit = SEEN_COMMAND;
		else if (strcmp(it->key, "place") == 0)
			bit = SEEN_PLACE;
		else if (strcmp(it->key, "size") == 0)
			bit = SEEN_SIZE;
		else if (strcmp(it->key, "level") == 0)
			bit = SEEN_LEVEL;
		else if (strcmp(it->key, "privileges") == 0)
			bit = SEEN_PRIVILEGES;
		else
			return fail(c, err, "line %d: unknown key %s", it->line, it->key);
		if (*s & bit)
			return fail(c, err, "line %d: %s twice", it->line, it->key);
		*s |= bit;
		if (memchr(it->value, '\0', it->len))
			return fail(c, err, "line %d: %s holds a NUL", it->line, it->key);

		int a, b2;
		switch (bit) {
		case SEEN_COMMAND:
			if (it->len == 0)
				return fail(c, err, "line %d: empty command", it->line);
			memcpy(p->command, it->value, it->len + 1);
			break;
		case SEEN_PLACE:
			if (pair(it->value, &a, &b2) < 0 || a > 99 || b2 > 99)
				return fail(c, err, "line %d: place is x y, 0 to 99", it->line);
			p->x = a;
			p->y = b2;
			break;
		case SEEN_SIZE:
			if (pair(it->value, &a, &b2) < 0 || a < 1 || a > 100 || b2 < 1 ||
			    b2 > 100)
				return fail(c, err, "line %d: size is width height, 1 to 100",
				            it->line);
			p->w = a;
			p->h = b2;
			break;
		case SEEN_LEVEL: {
			const char *v = it->value;
			if (number(&v, &a) < 0 || *v || a >= FIXED_LEVELS)
				return fail(c, err, "line %d: level is 0 to %d", it->line,
				            FIXED_LEVELS - 1);
			p->level = a;
			break;
		}
		case SEEN_PRIVILEGES:
			if (privileges(it->value, &p->privileges) < 0)
				return fail(c, err, "line %d: unknown or repeated privilege",
				            it->line);
			break;
		}
	}

	for (int b = 0; b < c->n; b++) {
		struct fixed_prog *p = &c->p[b];
		if ((seen[b] & SEEN_NEEDED) != SEEN_NEEDED)
			return fail(c, err, "%s needs command, place, size and privileges",
			            p->name);
		if (p->x + p->w > 100 || p->y + p->h > 100)
			return fail(c, err, "%s goes past the canvas", p->name);
		for (int o = 0; o < b; o++)
			if (c->p[o].level == p->level && overlap(&c->p[o], p))
				return fail(c, err, "%s and %s overlap on level %d",
				            c->p[o].name, p->name, p->level);
	}
	for (int i = 1; i < c->n; i++) {
		struct fixed_prog t = c->p[i];
		int j = i - 1;
		while (j >= 0 && c->p[j].level > t.level) {
			c->p[j + 1] = c->p[j];
			j--;
		}
		c->p[j + 1] = t;
	}
	return 0;
}

struct layout_rect fixed_rect(const struct fixed_prog *p, int w, int h) {
	struct layout_rect r;
	r.x = w * p->x / 100;
	r.y = h * p->y / 100;
	r.w = w * (p->x + p->w) / 100 - r.x;
	r.h = h * (p->y + p->h) / 100 - r.y;
	return r;
}

int fixed_died(struct fixed_deaths *d, uint64_t now) {
	if (d->n == FIXED_DEATHS) {
		memmove(&d->at[0], &d->at[1], (FIXED_DEATHS - 1) * sizeof(d->at[0]));
		d->n--;
	}
	d->at[d->n++] = now;
	return !(d->n == FIXED_DEATHS && now - d->at[0] <= FIXED_DEATH_SPAN_MS);
}
