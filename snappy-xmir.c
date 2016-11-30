/*
 * Copyright © 2016 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *     Ted Gould <ted.gould@canonical.com>
 */

#define _POSIX_C_SOURCE 200212L

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <string.h>

void
sigchild_handler (int signal)
{
	fprintf(stderr, "XMir has closed unexpectedly\n");
	exit(EXIT_FAILURE);
}

struct sigaction sigchild_action = {
	.sa_handler = sigchild_handler,
	.sa_flags = SA_NOCLDWAIT
};

int
main (int argc, char * argv[])
{
	if (argc < 3) {
		fprintf(stderr, "%s: Usage: [appid] [command to execute...]\n", argv[0]);
		return EXIT_FAILURE;
	}

	char * appid = argv[1];

	/* Setup socket pair */
	int sockets[2];
	if (socketpair(AF_LOCAL, SOCK_STREAM, 0, sockets) != 0) {
		fprintf(stderr, "Unable to create socketpair for grabbing environment variables\n");
		return EXIT_FAILURE;
	}

	int ourend = sockets[0];
	int envend = sockets[1];

	/* Fork and exec the x11 setup under it's confiment */
	if (sigaction(SIGCHLD, &sigchild_action, NULL) != 0) {
		fprintf(stderr, "Unable to setup child signal handler\n");
		return EXIT_FAILURE;
	}

	if (fork() == 0) {
		/* XMir start here */
		/* GOAL: /snap/bin/unity8-session.xmir-helper $appid libertine-launch /snap/unity8-session/current/usr/bin/snappy-xmir-envvars */
		/* TODO: Make paths more dynamic */
		char socketbuf[16] = {0};
		snprintf(socketbuf, 16, "%d", envend);

		setenv("UBUNTU_APP_LAUNCH_SNAPPY_XMIR_ENVVARS", socketbuf, 1);

		char * xmirexec[5] = {
			"/snap/bin/unity8-session.xmir-helper",
			appid,
			"libertine-launch",
			"/snap/unity8-session/current/usr/bin/snappy-xmir-envvars",
			NULL
		};

		printf("Executing xmir-helper on PID: %d", getpid());

		return execv(xmirexec[0], xmirexec);
	}

	/* Read our socket until we get all of the environment */
	char readbuf[2048] = {0};
	int amountread = 0;
	while ((amountread = read(ourend, readbuf, 2048))) {
		if (amountread == 2048) {
			fprintf(stderr, "Environment is too large, abort!\n");
			exit(EXIT_FAILURE);
		}

		char * startvar = readbuf;

		/* TODO: Assumes one big read of both value and variable */
		do {
			char * startval = startvar + strlen(startvar) + 1;
			setenv(startvar, startval, 1);
			startvar = startval + strlen(startval) + 1;
		}
		while (startvar < readbuf + amountread);
	}

	close(ourend);

	/* Exec the application with the new environment under its confinement */
	return execv(argv[2], &(argv[2]));
}
