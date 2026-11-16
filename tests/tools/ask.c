#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

int main(int argc, char **argv) {
	const char *dir = getenv("XDG_RUNTIME_DIR");
	struct sockaddr_un a = {.sun_family = AF_UNIX};
	if (argc < 2 || !dir ||
	    snprintf(a.sun_path, sizeof(a.sun_path), "%s/borealis-test", dir) >=
	        (int)sizeof(a.sun_path)) {
		fprintf(stderr, "usage: ask command [args]\n");
		return 2;
	}
	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0 || connect(fd, (struct sockaddr *)&a, sizeof(a)) < 0) {
		perror("connect error");
		return 1;
	}
	struct timeval tv = {10, 0};
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	char line[512] = "";
	for (int i = 1; i < argc; i++) {
		if (i > 1)
			strncat(line, " ", sizeof(line) - strlen(line) - 1);
		strncat(line, argv[i], sizeof(line) - strlen(line) - 1);
	}
	strncat(line, "\n", sizeof(line) - strlen(line) - 1);
	if (write(fd, line, strlen(line)) < 0) {
		perror("write error");
		return 1;
	}

	char buf[4096], last[64] = "";
	ssize_t r;
	size_t at = 0;
	while ((r = read(fd, buf, sizeof(buf))) > 0) {
		fwrite(buf, 1, (size_t)r, stdout);
		for (ssize_t i = 0; i < r; i++) {
			if (buf[i] == '\n') {
				last[at] = '\0';
				at = 0;
			} else if (at < sizeof(last) - 1) {
				last[at++] = buf[i];
			}
		}
	}
	close(fd);
	return strcmp(last, "ok") == 0 ? 0 : 1;
}
