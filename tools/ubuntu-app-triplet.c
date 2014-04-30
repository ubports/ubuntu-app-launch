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

#include "libupstart-app-launch/upstart-app-launch.h"

int
main (int argc, gchar * argv[]) {

	if (argc > 4 || argc == 1) {
		g_printerr("Usage: %s <package> [application] [version]\n", argv[0]);
		return 1;
	}

	gchar * pkg = argv[1];
	gchar * app = NULL;
	gchar * ver = NULL;

	if (argc > 2) {
		app = argv[2];
	}

	if (argc > 3) {
		app = argv[3];
	}

	gchar * appid = upstart_app_launch_triplet_to_app_id(pkg, app, ver);
	if (appid == NULL) {
		return -1;
	}

	g_print("%s\n", appid);
	g_free(appid);

	return 0;
}
