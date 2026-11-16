#include "screens.c"
#include "test.h"

static struct screens_conf c;
static char err[SCREENS_ERR_MAX];

static int parse(const char *text) {
	err[0] = '\0';
	return screens_parse(text, strlen(text), &c, err);
}

int main(void) {
	CHECK(parse("") == 0 && !c.mirror && !c.second_right,
	      "extended, with the second on the left, by default");
	CHECK(parse("mode = mirror\n") == 0 && c.mirror && !c.second_right,
	      "mirror");
	CHECK(parse("second = right\nmode = extended\n") == 0 && !c.mirror &&
	          c.second_right,
	      "the second on the right");
	CHECK(parse("mode = side\n") == -1 && strstr(err, "line 1") && !c.mirror,
	      "an unknown mode is an error and leaves the defaults");
	CHECK(parse("second = up\n") == -1, "an unknown side");
	CHECK(parse("mode = mirror\nmode = mirror\n") == -1 && strstr(err, "twice"),
	      "a key twice");
	CHECK(parse("scale = 2\n") == -1 && strstr(err, "unknown key"),
	      "an unknown key");
	CHECK(parse("a {\n}\n") == -1, "a block");
	int m = 5;
	CHECK(screens_mode("mirror", &m) == 0 && m == 1, "the word mirror");
	CHECK(screens_mode("extended", &m) == 0 && m == 0, "the word extended");
	CHECK(screens_mode("Mirror", &m) == -1 && m == 0, "and nothing else");
	return test_summary();
}
