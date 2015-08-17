/*
 * Copyright © 2014 Canonical Ltd.
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

void
sigchild_handler (int signal)
{
	fprintf(stderr, "XMir has closed unexpectedly\n");
	exit(1);
}

struct sigaction sigchild_action = {
	.sa_handler = sigchild_handler,
	.sa_flags = SA_NOCLDWAIT
};

int
main (int argc, char * argv[])
{
	if (argc < 3) {
		fprintf(stderr, "xmir-helper needs more arguments: xmir-helper $(appid) $(thing to exec) ... \n");
		return 1;
	}

	/* Make nice variables for the things we need */
	char * appid = argv[1];
	char * xmir = getenv("UBUNTU_APP_LAUNCH_XMIR_PATH");
	if (xmir == NULL) {
		xmir = "/usr/bin/Xmir";
	}

	/* Build a socket pair to get the connection back from XMir */
	int sockets[2];
	if (socketpair(AF_LOCAL, SOCK_STREAM, 0, sockets) != 0) {
		fprintf(stderr, "Unable to create socketpair for communicating with XMir\n");
		return 1;
	}

	/* Give them nice names, the compiler will optimize out */
	int xmirsocket = sockets[0];
	int helpersocket = sockets[1];

	/* Watch for the child dying */
	if (sigaction(SIGCHLD, &sigchild_action, NULL) != 0) {
		fprintf(stderr, "Unable to setup child signal handler\n");
		return 1;
	}

	/* Start XMir */
	if (fork() == 0) {
		/* XMir start here */
		/* GOAL: XMir -displayfd ${xmirsocket} -mir ${appid} */
		char socketbuf[16] = {0};
		snprintf(socketbuf, 16, "%d", xmirsocket);

		char * xmirexec[6] = {
			xmir,
			"-displayfd",
			socketbuf,
			"-mir",
			appid,
			NULL
		};

		return execv(xmir, xmirexec);
	}

	/* Wait to get the display number from XMir */
	char readbuf[16] = {0};
	if (read(helpersocket, readbuf, 16) == 0) {
		fprintf(stderr, "Not reading anything from XMir\n");
		return 1;
	}

	int i;
	for (i = 0; i < sizeof(readbuf); i++) {
		if (readbuf[i] == '\n') {
			readbuf[i] = '\0';
			break;
		}
	}

	char displaynumber[16] = {0};
	snprintf(displaynumber, 16, ":%s", readbuf);

	/* Set up the display variable */
	setenv("DISPLAY", displaynumber, 1);

	/* Now that we have everything setup, we can execute */
	char ** nargv = &argv[2];
	int execret = execvp(nargv[0], nargv);
	return execret;
}
