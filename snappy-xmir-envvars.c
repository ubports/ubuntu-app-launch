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
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

void
copyenv (int fd, const char * envname)
{
	const char * envval = getenv(envname);
	if (envval == NULL) {
		fprintf(stderr, "Unable to get environment variable '%s'\n", envname);
		exit(EXIT_FAILURE);
	}

	write(fd, envname, strlen(envname) + 1);
	write(fd, envval, strlen(envval) + 1);
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
	char * socketstring = getenv("UBUNTU_APP_LAUNCH_SNAPPY_XMIR_ENVVARS");
	if (socketstring == NULL) {
		fprintf(stderr, "Unable to get socket environment variable\n");
		return(EXIT_FAILURE);
	}

	int socketnum = atoi(socketstring);
	if (!(socketnum > 0 && socketnum < 20)) {
		fprintf(stderr, "Passed socket ID not within a valid range: %d\n", socketnum);
		return(EXIT_FAILURE);
	}

	/* Dump envvars to socket */
	copyenv(socketnum, "DISPLAY");
	copyenv(socketnum, "DBUS_SESSION_BUS_ADDRESS");

	/* Close the socket */
	close(socketnum);

	/* Wait for sigterm */
	signal(SIGTERM, termhandler);

	wait(NULL);
	return EXIT_SUCCESS;
}
