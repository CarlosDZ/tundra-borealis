#ifndef TUNDRA_CLI_H
#define TUNDRA_CLI_H

#include <stdio.h>

enum cli_mode { CLI_RUN, CLI_VERSION, CLI_HELP, CLI_SCREENS, CLI_USAGE };

enum cli_mode cli_parse(int argc, char **argv);
void cli_usage(FILE *f);

#endif
