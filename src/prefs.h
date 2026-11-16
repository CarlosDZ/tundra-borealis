#ifndef TUNDRA_PREFS_H
#define TUNDRA_PREFS_H

#include <stddef.h>
#include <tundra/conf.h>

#define PREFS_MAX 128
#define PREFS_ERR_MAX 160

/* Which programs prefer H or V, by app_id. The file has one line per program,
 * "h = app_id" or "v = app_id": an app_id can be longer than a key. */
struct prefs {
	struct {
		char app_id[TCONF_VALUE_MAX];
		char type;
	} items[PREFS_MAX];
	int n;
};

/* 0, or -1 with err set and p left empty. */
int prefs_parse(const char *text, size_t len, struct prefs *p, char *err);

/* 'H' or 'V'; H for a program not listed or without an app_id. */
char prefs_type(const struct prefs *p, const char *app_id);

#endif
