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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

int
main (int argc, char * argv[])
{
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <oom file> <value>", argv[0]);
		return -1;
	}

	FILE * adj = fopen(argv[1], "w");
	int openerr = errno;

	if (adj == NULL) {
		if (openerr != ENOENT) {
			/* ENOENT happens a fair amount because of races, so it's not
			   worth printing a warning about */
			fprintf(stderr, "Unable to set OOM value on '%s' to '%s': %s", argv[1], argv[2], strerror(openerr));
			return openerr;
		} else {
			return 0;
		}
	}

	size_t writesize = fwrite(argv[2], 1, strlen(argv[2]), adj);
	int writeerr = errno;
	fclose(adj);

	if (writesize == strlen(argv[2]))
		return 0;
	
	if (writeerr != 0)
		fprintf(stderr, "Unable to set OOM value on '%s' to '%s': %s", argv[1], argv[2], strerror(writeerr));
	else
		/* No error, but yet, wrong size. Not sure, what could cause this. */
		fprintf(stderr, "Unable to set OOM value on '%s' to '%s': Wrote %d bytes", argv[1], argv[2], (int)writesize);

	return -1;
}
