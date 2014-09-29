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

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

int
main (int argc, char * argv[])
{
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <pid> <value>", argv[0]);
		return -1;
	}

	/* Not we turn this into an integer and back so that we can ensure we don't
	   get used for nefarious tasks. */
	int pidval = atoi(argv[1]);
	if ((pidval < 1) || (pidval >= 32768)) {
		fprintf(stderr, "PID passed %s is invalid: %d", argv[1], pidval);
		return -1;
	}

	/* Open up the PID directory first, to ensure that it is actually one of
	   ours, so that we can't be used to set a OOM value on just anything */
	char pidpath[32];
	snprintf(pidpath, sizeof(pidpath), "/proc/%d", pidval);

	int piddir = open(pidpath, O_RDONLY | O_DIRECTORY);
	if (piddir < 0) {
		fprintf(stderr, "Unable open PID directory '%s' for '%s': %s", pidpath, argv[1], strerror(errno));
		return -1;
	}

	struct stat piddirstat = {0};
	if (fstat(piddir, &piddirstat) <= 0) {
		close(piddir);
		fprintf(stderr, "Unable stat PID directory '%s' for '%s': %s", pidpath, argv[1], strerror(errno));
		return -1;
	}

	if (getuid() != piddirstat.st_uid) {
		close(piddir);
		fprintf(stderr, "PID directory '%s' is not owned by %d but by %d", pidpath, getuid(), piddirstat.st_uid);
		return -1;
	}

	/* Looks good, let's try to get the actual oom_adj_score file to write
	   the value to it. */
	int adj = openat(piddir, "oom_score_adj", O_WRONLY);
	int openerr = errno;

	if (adj == 0) {
		close(piddir);

		if (openerr != ENOENT) {
			/* ENOENT happens a fair amount because of races, so it's not
			   worth printing a warning about */
			fprintf(stderr, "Unable to set OOM value on '%s' to '%s': %s", argv[1], argv[2], strerror(openerr));
			return openerr;
		} else {
			return 0;
		}
	}

	size_t writesize = write(adj, argv[2], strlen(argv[2]));
	int writeerr = errno;

	close(adj);
	close(piddir);

	if (writesize == strlen(argv[2]))
		return 0;
	
	if (writeerr != 0)
		fprintf(stderr, "Unable to set OOM value on '%s' to '%s': %s", argv[1], argv[2], strerror(writeerr));
	else
		/* No error, but yet, wrong size. Not sure, what could cause this. */
		fprintf(stderr, "Unable to set OOM value on '%s' to '%s': Wrote %d bytes", argv[1], argv[2], (int)writesize);

	return -1;
}
