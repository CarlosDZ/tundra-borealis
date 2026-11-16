#include <stdio.h>
#include <string.h>

#include "prefs.h"

/* Too big for the stack */
static struct tconf parsed;

static int fail(struct prefs *p, char *err, int line, const char *what,
                const char *arg) {
	snprintf(err, PREFS_ERR_MAX, "line %d: %s %s", line, what, arg);
	p->n = 0;
	return -1;
}

int prefs_parse(const char *text, size_t len, struct prefs *p, char *err) {
	p->n = 0;
	if (tconf_parse(text, len, &parsed) < 0) {
		snprintf(err, PREFS_ERR_MAX, "line %d: %s", parsed.err_line,
		         parsed.err);
		return -1;
	}
	if (parsed.nblocks > 0)
		return fail(p, err, parsed.blocks[0].line, "no blocks here, found",
		            parsed.blocks[0].name);

	for (int i = 0; i < parsed.n; i++) {
		const struct tconf_item *it = &parsed.items[i];
		char type;
		if (strcmp(it->key, "h") == 0)
			type = 'H';
		else if (strcmp(it->key, "v") == 0)
			type = 'V';
		else
			return fail(p, err, it->line, "unknown key", it->key);
		if (it->len == 0 || memchr(it->value, '\0', it->len))
			return fail(p, err, it->line, "not an app_id:", it->value);
		for (int j = 0; j < p->n; j++)
			if (strcmp(p->items[j].app_id, it->value) == 0)
				return fail(p, err, it->line, "listed twice:", it->value);
		if (p->n == PREFS_MAX)
			return fail(p, err, it->line, "more programs than", "128");
		memcpy(p->items[p->n].app_id, it->value, it->len + 1);
		p->items[p->n].type = type;
		p->n++;
	}
	return 0;
}

char prefs_type(const struct prefs *p, const char *app_id) {
	if (!app_id)
		return 'H';
	for (int i = 0; i < p->n; i++)
		if (strcmp(p->items[i].app_id, app_id) == 0)
			return p->items[i].type;
	return 'H';
}
