#include "colors.c"
#include "test.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char dir[64], conf[96], tundra[112], theme[128];

static struct tdraw_theme *user_theme(const char *text) {
	struct tdraw_theme *t;
	const char *err;
	FILE *f = fopen(theme, "w");

	if (!f)
		return NULL;
	fputs(text, f);
	fclose(f);
	if (!(t = tdraw_theme_open()))
		return NULL;
	if (tdraw_theme_load(t, &err) < 0) {
		printf("# %s\n", err);
		tdraw_theme_free(t);
		return NULL;
	}
	return t;
}

int main(void) {
	const char *tmp = getenv("TMPDIR");
	struct tdraw_theme *t;
	struct colors c;
	float f[4];

	colors_load(&c, NULL);
	CHECK(c.background == 0xff141a1e && c.text == 0xffccd2d6 &&
	          c.alert == 0xffff1712 && c.fallen == 0xff4d2623,
	      "without a theme, the colors of Tundra");

	snprintf(dir, sizeof(dir), "%s/borealis-colors-XXXXXX", tmp ? tmp : "/tmp");
	if (!mkdtemp(dir)) {
		perror("mkdtemp error");
		return 1;
	}
	snprintf(conf, sizeof(conf), "%s/.config", dir);
	snprintf(tundra, sizeof(tundra), "%s/tundra", conf);
	snprintf(theme, sizeof(theme), "%s/theme", tundra);
	mkdir(conf, 0700);
	mkdir(tundra, 0700);
	setenv("HOME", dir, 1);
	t = user_theme("text = 112233\nborealis.text = 445566\nalert = 778899\n"
	               "borealis.fallen = 000000 128\n");
	colors_load(&c, t);
	CHECK(c.text == 0xff445566, "borealis.name goes before name");
	CHECK(c.alert == 0xff778899, "and name before the color of Tundra");
	CHECK(c.dim == 0xff889196, "a color the theme lacks is that of Tundra");
	CHECK(c.fallen == 0x80000000, "with the alpha the theme gives it");
	tdraw_theme_free(t);

	colors_rgba(0x80ff0000, f);
	CHECK(f[0] > 0.50f && f[0] < 0.51f && f[1] == 0 && f[2] == 0 &&
	          f[3] > 0.50f && f[3] < 0.51f,
	      "a color for the scene is premultiplied");
	unlink(theme);
	rmdir(tundra);
	rmdir(conf);
	rmdir(dir);
	return test_summary();
}
