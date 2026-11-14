#include <stdio.h>

#include "cli.h"
#include "server.h"

#define EXIT_OK 0
#define EXIT_FAIL 1
#define EXIT_USAGE 2

int main(int argc, char *argv[]) {
	switch (cli_parse(argc, argv)) {
	case CLI_VERSION:
		printf("tundra-borealis %s\n", TUNDRA_BOREALIS_VERSION);
		return EXIT_OK;
	case CLI_HELP:
		cli_usage(stdout);
		return EXIT_OK;
	case CLI_RUN:
		return server_run() == 0 ? EXIT_OK : EXIT_FAIL;
	case CLI_USAGE:
		break;
	}
	cli_usage(stderr);
	return EXIT_USAGE;
}
