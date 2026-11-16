#include <stdio.h>
#include <string.h>
#include <tundra/conf.h>

#include "screens.h"

static struct tconf parsed;

void screens_default(struct screens_conf *c) {
	c->mirror = 0;
	c->second_right = 0;
}

int screens_mode(const char *word, int *mirror) {
	if (strcmp(word, "extended") == 0)
		*mirror = 0;
	else if (strcmp(word, "mirror") == 0)
		*mirror = 1;
	else
		return -1;
	return 0;
}

static int fail(struct screens_conf *c, char *err, int line, const char *why) {
	snprintf(err, SCREENS_ERR_MAX, "line %d: %s", line, why);
	screens_default(c);
	return -1;
}

int screens_parse(const char *text, size_t len, struct screens_conf *c,
                  char *err) {
	screens_default(c);
	if (tconf_parse(text, len, &parsed) < 0)
		return fail(c, err, parsed.err_line, parsed.err);
	if (parsed.nblocks > 0)
		return fail(c, err, parsed.blocks[0].line, "no blocks here");
	int seen_mode = 0, seen_second = 0;
	for (int i = 0; i < parsed.n; i++) {
		const struct tconf_item *it = &parsed.items[i];
		if (strcmp(it->key, "mode") == 0) {
			if (seen_mode++)
				return fail(c, err, it->line, "mode twice");
			if (screens_mode(it->value, &c->mirror) < 0)
				return fail(c, err, it->line, "mode is extended or mirror");
		} else if (strcmp(it->key, "second") == 0) {
			if (seen_second++)
				return fail(c, err, it->line, "second twice");
			if (strcmp(it->value, "left") == 0)
				c->second_right = 0;
			else if (strcmp(it->value, "right") == 0)
				c->second_right = 1;
			else
				return fail(c, err, it->line, "second is left or right");
		} else {
			return fail(c, err, it->line, "unknown key");
		}
	}
	return 0;
}
