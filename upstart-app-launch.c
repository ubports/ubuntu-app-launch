/*
 * Copyright 2013 Canonical Ltd.
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

#include "libupstart-app-launch/upstart-app-launch.h"

int
main (int argc, gchar * argv[]) {

	if (argc < 2) {
		g_printerr("Usage: %s <app id> [uris]\n", argv[0]);
		return 1;
	}

	gchar ** uris = NULL;
	if (argc > 2) {
		int i;

		uris = g_new0(gchar *, argc - 1);

		for (i = 2; i < argc; i++) {
			uris[i - 2] = argv[i];
		}
	}

	upstart_app_launch_start_application(argv[1], (const gchar * const *)uris);

	g_free(uris);

	return 0;
}
