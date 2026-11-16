#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "cli.h"
#include "control.h"
#include "screens.h"
#include "server.h"

#define EXIT_OK 0
#define EXIT_FAIL 1
#define EXIT_USAGE 2

static int screens(const char *mode) {
	int mirror;
	if (mode && screens_mode(mode, &mirror) < 0) {
		cli_usage(stderr);
		return EXIT_USAGE;
	}
	const char *display = getenv("WAYLAND_DISPLAY");
	struct sockaddr_un a = {.sun_family = AF_UNIX};
	if (control_path(display && *display ? display : "wayland-0", a.sun_path,
	                 sizeof(a.sun_path)) < 0) {
		fprintf(stderr, "no XDG_RUNTIME_DIR to find borealis in\n");
		return EXIT_FAIL;
	}
	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0 || connect(fd, (struct sockaddr *)&a, sizeof(a)) < 0) {
		fprintf(stderr, "borealis is not running here\n");
		return EXIT_FAIL;
	}
	char line[64];
	int n = snprintf(line, sizeof(line), "screens%s%s\n", mode ? " " : "",
	                 mode ? mode : "");
	if (write(fd, line, (size_t)n) != n) {
		perror("write error");
		return EXIT_FAIL;
	}
	char buf[256];
	size_t len = 0;
	ssize_t r;
	while (len < sizeof(buf) - 1 &&
	       (r = read(fd, buf + len, sizeof(buf) - 1 - len)) > 0)
		len += (size_t)r;
	close(fd);
	buf[len] = '\0';
	if (strncmp(buf, "screens ", 8) != 0 || !strstr(buf, "\nok\n")) {
		fprintf(stderr, "%s", len ? buf : "no answer from borealis\n");
		return EXIT_FAIL;
	}
	*strchr(buf, '\n') = '\0';
	printf("%s\n", buf + 8);
	return EXIT_OK;
}

int main(int argc, char *argv[]) {
	switch (cli_parse(argc, argv)) {
	case CLI_VERSION:
		printf("tundra-borealis %s\n", TUNDRA_BOREALIS_VERSION);
		return EXIT_OK;
	case CLI_HELP:
		cli_usage(stdout);
		return EXIT_OK;
	case CLI_SCREENS:
		return screens(argc == 3 ? argv[2] : NULL);
	case CLI_RUN:
		return server_run() == 0 ? EXIT_OK : EXIT_FAIL;
	case CLI_USAGE:
		break;
	}
	cli_usage(stderr);
	return EXIT_USAGE;
}
