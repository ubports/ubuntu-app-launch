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
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

void
copyenv (int fd, const char * envname, const char * envval)
{
	if (envval == NULL) {
		fprintf(stderr, "Unable to get environment variable '%s'\n", envname);
		exit(EXIT_FAILURE);
	}

	int writesize;

	writesize = write(fd, envname, strlen(envname) + 1);

	if (writesize <= 0) {
		fprintf(stderr, "Unable to write to socket '%s'\n", envname);
		exit(EXIT_FAILURE);
	}

	writesize = write(fd, envval, strlen(envval) + 1);

	if (writesize <= 0) {
		fprintf(stderr, "Unable to write to socket '%s'\n", envval);
		exit(EXIT_FAILURE);
	}

	if (getenv("G_MESSAGES_DEBUG") != NULL)
		printf("Wrote envvar '%s=%s'\n", envname, envval);
}

void
termhandler (int sig)
{
	exit(EXIT_SUCCESS);
}

int
main (int argc, char * argv[])
{
	/* Grab socket */
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <socket name>\n", argv[0]);
		return(EXIT_FAILURE);
	}

	char * socketname = argv[1];

	int socketfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (socketfd <= 0) {
		fprintf(stderr, "%s: Unable to create socket\n", argv[0]);
		return EXIT_FAILURE;
	}

	struct sockaddr_un socketaddr = {0};
	socketaddr.sun_family = AF_UNIX;
	strncpy(socketaddr.sun_path, socketname, sizeof(socketaddr.sun_path) - 1);
	socketaddr.sun_path[0] = 0;

	if (connect(socketfd, (const struct sockaddr *)&socketaddr, sizeof(struct sockaddr_un)) < 0) {
		fprintf(stderr, "Unable to connect socket\n");
		return EXIT_FAILURE;
	}

	/* Dump envvars to socket */
	copyenv(socketfd, "DISPLAY", getenv("DISPLAY"));
	copyenv(socketfd, "DBUS_SESSION_BUS_ADDRESS", getenv("DBUS_SESSION_BUS_ADDRESS"));

	char mypid[16];
	snprintf(mypid, 16, "%d", getpid());
	copyenv(socketfd, "UBUNTU_APP_LAUNCH_SNAPPY_XMIR_ENVVARS_PID", mypid);

	/* Close the socket */
	close(socketfd);

	/* Wait for sigterm */
	signal(SIGTERM, termhandler);

	for (;;) {
		sleep(24 * 60 * 60); // taking things one day at a time
	}

	return EXIT_FAILURE;
}
