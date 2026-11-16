#include <string.h>

#include "cli.h"

enum cli_mode cli_parse(int argc, char **argv) {
	if (argc == 1)
		return CLI_RUN;
	if ((argc == 2 || argc == 3) && strcmp(argv[1], "screens") == 0)
		return CLI_SCREENS;
	if (argc != 2)
		return CLI_USAGE;
	if (strcmp(argv[1], "--version") == 0)
		return CLI_VERSION;
	if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)
		return CLI_HELP;
	return CLI_USAGE;
}

void cli_usage(FILE *f) {
	fprintf(f, "usage: tundra-borealis\n"
	           "       tundra-borealis screens [mirror | extended]\n"
	           "       tundra-borealis --version\n");
}
