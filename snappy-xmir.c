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
#include <sys/un.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>

#define ENVVAR_BUFFER_SIZE 4096
#define SOCKETNAME_BUFFER_SIZE 256

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

	/* Build Socket Name */
	srand(time(NULL));
	char socketname[SOCKETNAME_BUFFER_SIZE] = {0};
	snprintf(socketname, sizeof(socketname), "/ual-socket-%08X-%s", rand(), appid);

	/* Setup abstract socket */
	int socketfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (socketfd <= 0) {
		fprintf(stderr, "%s: Unable to create socket\n", argv[0]);
		return EXIT_FAILURE;
	}

	struct sockaddr_un socketaddr = {0};
	socketaddr.sun_family = AF_UNIX;
	strncpy(socketaddr.sun_path, socketname, sizeof(socketaddr.sun_path) - 1);
	socketaddr.sun_path[0] = 0;

	if (bind(socketfd, (const struct sockaddr *)&socketaddr, sizeof(struct sockaddr_un)) < 0) {
		fprintf(stderr, "%s: Unable to bind socket '%s'\n", argv[0], socketname);
		return EXIT_FAILURE;
	}

	/* Fork and exec the x11 setup under it's confiment */
	if (sigaction(SIGCHLD, &sigchild_action, NULL) != 0) {
		fprintf(stderr, "Unable to setup child signal handler\n");
		return EXIT_FAILURE;
	}

	if (fork() == 0) {
		/* XMir start here */
		/* GOAL: /snap/bin/unity8-session.xmir-helper $appid libertine-launch /snap/unity8-session/current/usr/bin/snappy-xmir-envvars socketname */

		char * snappyhelper = getenv("UBUNTU_APP_LAUNCH_SNAPPY_XMIR_HELPER");
		if (snappyhelper == NULL) {
			snappyhelper = "xmir-helper";
		}

		char * libertinelaunch = getenv("UBUNTU_APP_LAUNCH_LIBERTINE_LAUNCH");
		if (libertinelaunch == NULL) {
			libertinelaunch = "libertine-launch";
		}

		/* envvar is like us, but a little more */
		char envvars[256] = {0};
		snprintf(envvars, sizeof(envvars), "%s-envvars", argv[0]);

		char * xmirexec[6] = {
			snappyhelper,
			appid,
			libertinelaunch,
			envvars,
			socketname,
			NULL
		};

		printf("Executing xmir-helper on PID: %d\n", getpid());

		fflush(stdout);

		return execv(xmirexec[0], xmirexec);
	}

	listen(socketfd, 1); /* 1 is the number of people who can connect */
	int readsocket = accept(socketfd, NULL, NULL);

	if (getenv("G_MESSAGES_DEBUG") != NULL) {
		printf("Got a socket connection on: %s\n", socketname);
	}

	/* Read our socket until we get all of the environment */
	char readbuf[ENVVAR_BUFFER_SIZE] = {0};
	int amountread = 0;
	int thisread = 0;
	while ((thisread = read(readsocket, readbuf + amountread, ENVVAR_BUFFER_SIZE - amountread))) {
		amountread += thisread;

		if (amountread == ENVVAR_BUFFER_SIZE) {
			fprintf(stderr, "Environment is too large, abort!\n");
			exit(EXIT_FAILURE);
		}
	}

	close(readsocket);
	close(socketfd);

	/* Parse the environment into variables we can insert */
	if (amountread > 0) {
		char * startvar = readbuf;

		do {
			char * startval = startvar + strlen(startvar) + 1;
			setenv(startvar, startval, 1);

			if (getenv("G_MESSAGES_DEBUG") != NULL) {
				printf("Got env: %s=%s\n", startvar, startval);
			}

			startvar = startval + strlen(startval) + 1;
		}
		while (startvar < readbuf + amountread);
	}

	/* Clear MIR_* variables from the environment */
	/* Unfortunately calling unsetenv resets the environ array so
	   it can become invalid. So we need to start over, though this
	   is fast */
	int unset = 1;
	while (unset) {
		unset = 0;

		unsigned int i;
		for (i = 0; __environ[i] != NULL; i++) {
			const char * env = __environ[i];
			/* Not checking the length becasue imagining that block
			   size will always be larger than 4 bytes on 32-bit systems.
			   Compiler should fold this into one comparison. */
			if (env[0] == 'M' && env[1] == 'I' && env[2] == 'R' && env[3] == '_') {
				char envname[64] = {0};
				unset = 1;

				strncpy(envname, env, 64 - 1);
				unsigned int j;
				for (j = 0; j < 64 && envname[j] != '\0'; j++) {
					if (envname[j] == '=') {
						envname[j] = '\0';

						if (unsetenv(envname) != 0) {
							/* Shouldn't happen unless we're out of memory,
							   might as well bail now if that's the case. */
							fprintf(stderr, "Unable to unset '%s' environment variable\n", envname);
							exit(EXIT_FAILURE);
						}

						break;
					}
				}

				break;
			}
		}
	}

	fflush(stdout);

	/* Exec the application with the new environment under its confinement */
	return execv(argv[2], &(argv[2]));
}
