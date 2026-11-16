#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "spawn.h"

#define SPAWN_WAYLAND_FD 3

int spawn(const char *cmd, int wayland) {
	pid_t pid = fork();
	if (pid < 0) {
		perror("fork error");
		return -1;
	}
	if (pid == 0) {
		sigset_t none;
		sigemptyset(&none);
		sigprocmask(SIG_SETMASK, &none, NULL);
		for (int sig = 1; sig < NSIG; sig++)
			signal(sig, SIG_DFL);
		setsid();
		pid_t g = fork();
		if (g == 0) {
			int keep = 3;
			if (wayland >= 0) {
				if (dup2(wayland, SPAWN_WAYLAND_FD) < 0)
					_exit(127);
				setenv("WAYLAND_SOCKET", "3", 1);
				keep = SPAWN_WAYLAND_FD + 1;
			}
			close_range((unsigned int)keep, ~0U, 0);
			execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
			_exit(127);
		}
		_exit(g < 0 ? 1 : 0);
	}
	int status;
	if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != 0)
		return -1;
	return 0;
}
