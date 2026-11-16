#include "prefs.c"
#include "test.h"

static struct prefs p;
static char err[PREFS_ERR_MAX];

static int parse(const char *text) {
	err[0] = '\0';
	return prefs_parse(text, strlen(text), &p, err);
}

int main(void) {
	CHECK(parse("") == 0 && p.n == 0, "an empty file");
	CHECK(prefs_type(&p, "frost") == 'H', "a program not listed is H");
	CHECK(prefs_type(&p, NULL) == 'H', "a window without app_id is H");

	CHECK(parse("# comment\nv = org.example.tall\nh = frost\n") == 0 &&
	          p.n == 2,
	      "one program per line");
	CHECK(prefs_type(&p, "org.example.tall") == 'V', "a listed V");
	CHECK(prefs_type(&p, "frost") == 'H', "a listed H");
	CHECK(prefs_type(&p, "org.example") == 'H', "a prefix is not a match");

	CHECK(parse("v = \"org.example.an-app-id-much-longer-than-any-key\"\n") ==
	              0 &&
	          prefs_type(
	              &p, "org.example.an-app-id-much-longer-than-any-key") == 'V',
	      "an app_id longer than a key, quoted");

	CHECK(parse("x = frost\n") == -1 && strstr(err, "line 1") &&
	          strstr(err, "unknown key x"),
	      "an unknown key is an error with its line");
	CHECK(p.n == 0, "a failed parse leaves nothing");
	CHECK(parse("v = a\nh = a\n") == -1 && strstr(err, "line 2") &&
	          strstr(err, "twice"),
	      "the same program twice is an error");
	CHECK(parse("v = \"\"\n") == -1, "an empty app_id is an error");
	CHECK(parse("v = \"a\\x00b\"\n") == -1, "an app_id with a NUL is an error");
	CHECK(parse("apps {\nv = a\n}\n") == -1 && strstr(err, "no blocks"),
	      "a block is an error");
	CHECK(parse("v a\n") == -1 && strstr(err, "line 1"),
	      "a grammar error comes with its line");

	return test_summary();
}
