#include "cli.c"
#include "test.h"

static enum cli_mode parse(int argc, char *a1, char *a2) {
	char *argv[] = {"tundra-borealis", a1, a2, NULL};
	return cli_parse(argc, argv);
}

int main(void) {
	CHECK(parse(1, NULL, NULL) == CLI_RUN, "no arguments runs the compositor");
	CHECK(parse(2, "--version", NULL) == CLI_VERSION, "--version");
	CHECK(parse(2, "--help", NULL) == CLI_HELP, "--help");
	CHECK(parse(2, "-h", NULL) == CLI_HELP, "-h");
	CHECK(parse(2, "run", NULL) == CLI_USAGE,
	      "an unknown argument is a usage error");
	CHECK(parse(3, "--version", "x") == CLI_USAGE,
	      "--version takes nothing after it");
	return test_summary();
}
