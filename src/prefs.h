#ifndef TUNDRA_PREFS_H
#define TUNDRA_PREFS_H

#include <stddef.h>
#include <tundra/conf.h>

#define PREFS_MAX 128
#define PREFS_ERR_MAX 160

struct prefs {
	struct {
		char app_id[TCONF_VALUE_MAX];
		char type;
	} items[PREFS_MAX];
	int n;
};

int prefs_parse(const char *text, size_t len, struct prefs *p, char *err);

char prefs_type(const struct prefs *p, const char *app_id);

#endif
