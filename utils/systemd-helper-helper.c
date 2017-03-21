/*
 * Copyright © 2017 Canonical Ltd.
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
#define _XOPEN_SOURCE 700

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>

#define PARAMS_SIZE 4096
#define PARAMS_COUNT 32
#define SOCKETNAME_SIZE 256
#define ENVNAME_SIZE 64

void
sigchild_handler (int signal, siginfo_t * sigdata, void * data)
{
	if (signal != SIGCHLD) {
		return;
	}

	if (sigdata->si_code == CLD_KILLED) {
		fprintf(stderr, "Helper exec tool killed\n");
		exit(EXIT_FAILURE);
	}

	if (sigdata->si_code == CLD_DUMPED) {
		fprintf(stderr, "Helper exec tool dumped\n");
		exit(EXIT_FAILURE);
	}

	if (sigdata->si_code == CLD_EXITED && sigdata->si_status != 0) {
		fprintf(stderr, "Helper exec tool has closed unexpectedly: %d\n", sigdata->si_status);
		exit(EXIT_FAILURE);
	}
}

struct sigaction sigchild_data = {0};

struct sigaction sigchild_action = {
	.sa_sigaction = sigchild_handler,
	.sa_flags = SA_NOCLDWAIT
};

int
get_params (char * readbuf, char ** exectool)
{
	int amountread = 0;

	/* Build Socket Name */
	srand(time(NULL));
	char socketname[SOCKETNAME_SIZE] = {0};
	snprintf(socketname, sizeof(socketname), "/ual-helper-%08X", rand());

	/* Setup abstract socket */
	int socketfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (socketfd <= 0) {
		fprintf(stderr, "Unable to create socket\n");
		exit(EXIT_FAILURE);
	}

	struct sockaddr_un socketaddr = {0};
	socketaddr.sun_family = AF_UNIX;
	strncpy(socketaddr.sun_path, socketname, sizeof(socketaddr.sun_path) - 1);
	socketaddr.sun_path[0] = 0;

	if (bind(socketfd, (const struct sockaddr *)&socketaddr, sizeof(struct sockaddr_un)) < 0) {
		fprintf(stderr, "Unable to bind socket '%s'\n", socketname);
		exit(EXIT_FAILURE);
	}

	/* Fork and exec the exec-tool under it's confinement */
	if (sigaction(SIGCHLD, &sigchild_action, &sigchild_data) != 0) {
		fprintf(stderr, "Unable to setup child signal handler\n");
		exit(EXIT_FAILURE);
	}

	pid_t childpid;
	if ((childpid = fork()) == 0) {
		/* Exec tool start here */

		/* NOTE: We might need a different environment when the
		 * exec tool is existing in the unity8-session snap, but
		 * we'll need to get integration hooks setup and start
		 * playing with them to see what makes sense there. Probably
		 * something like legacy-exec in the unity8-session snap. */

		/* GOAL: ${argv[2]} */
		setenv("UBUNTU_APP_LAUNCH_HELPER_EXECTOOL_SETEXEC_SOCKET", socketname, 1);

		printf("Executing exec-tool on PID: %d\n", getpid());

		fflush(stdout);

		return execv(exectool[0], exectool);
	}

	listen(socketfd, 1); /* 1 is the number of people who can connect */
	int readsocket = accept(socketfd, NULL, NULL);

	if (getenv("G_MESSAGES_DEBUG") != NULL) {
		printf("Got a socket connection on: %s\n", socketname);
	}

	/* Read our socket until we get all of the environment */
	int thisread = 0;
	while ((thisread = read(readsocket, readbuf + amountread, PARAMS_SIZE - amountread)) > 0) {
		amountread += thisread;

		if (amountread == PARAMS_SIZE) {
			fprintf(stderr, "Params are too large, abort!\n");
			exit(EXIT_FAILURE);
		}
	}

	close(readsocket);
	close(socketfd);

	siginfo_t waitiddata = {0};
	if (waitid(P_PID, childpid, &waitiddata, WEXITED) != 0) {
		if (errno != ECHILD) {
			perror("waitid on child failed");
			exit(EXIT_FAILURE);
		}
	}

	/* Use the same handler of errors */
	sigchild_handler(SIGCHLD, &waitiddata, NULL);

	return amountread;
}

int
main (int argc, char * argv[])
{
	if (argc == 1) {
		fprintf(stderr, "%s: Usage: <exec-tool to execute...> <app exec> <urls>\n", argv[0]);
		return EXIT_FAILURE;
	}

	int debug = (getenv("G_MESSAGES_DEBUG") != NULL);

	if (debug) {
		printf("Getting parameters from exec-tool: %s\n", argv[1]);
	}

	char readbuf[PARAMS_SIZE] = {0};
	int amountread = get_params(readbuf, &argv[1]);

	char * apparray[PARAMS_COUNT] = {0};
	int currentparam = 0;
	int currentargc = 2;

	if (getenv("UBUNTU_APP_LAUNCH_DEMANGLE_PATH") != NULL &&
			getenv("UBUNTU_APP_LAUNCH_DEMANGLE_NAME") != NULL) {
		apparray[currentparam] = getenv("UBUNTU_APP_LAUNCH_DEMANGLER");
		if (apparray[currentparam] == NULL) {
			apparray[currentparam] = DEMANGLER_PATH;
		}
		if (debug) {
			printf("Using demangler: %s\n", apparray[currentparam]);
		}
		currentparam++;
	}

	/* Copy in app exec */
	for (; currentargc < argc && /* Don't overrun argv */
			currentparam < PARAMS_COUNT; /* Don't overrun our static array */
			currentargc++, currentparam++) {
		if (argv[currentargc][0] == '-' && argv[currentargc][1] == '-') { /* Cheap strcmp "--" */
			currentargc++;
			break;
		}
		apparray[currentparam] = argv[currentargc];
	}

	/* Parse the scoket data into params we can insert */
	if (amountread > 0) {
		char * startvar = readbuf;

		do {
			/* Removing the %u and %U from legacy stuff */
			if (!(startvar[0] == '%' && startvar[1] == 'u') &&
					!(startvar[0] == '%' && startvar[1] == 'U')) {
				if (debug) printf("Socket value: %s\n", startvar); 
				apparray[currentparam] = startvar;
				currentparam++;
			} else {
				if (debug) printf("Ignore value: %s\n", startvar); 
			}

			startvar = startvar + strlen(startvar) + 1;
		}
		while (startvar < readbuf + amountread && currentparam < PARAMS_COUNT - 1);

	}

	/* Copy in URLs */
	for (; currentargc < argc && /* Don't overrun argv */
			currentparam < PARAMS_COUNT; /* Don't overrun our static array */
			currentargc++, currentparam++) {
		apparray[currentparam] = argv[currentargc];
	}

	if (debug) {
		printf("Exec:");
		int i;
		for (i = 0; i < currentparam; i++) {
			printf(" %s", apparray[i]);
		}
		printf("\n");
	}

	fflush(stdout);

	/* Exec the application with the new environment under its confinement */
	return execv(apparray[0], apparray);
}
