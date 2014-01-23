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
	if (argc != 2) {
		g_printerr("Usage: %s <helper type>\n", argv[0]);
		return 1;
	}

	gchar ** appids = upstart_app_launch_list_helpers(argv[1]);
	if (appids == NULL) {
		g_warning("Error getting App IDs for helper type '%s'", argv[1]);
		return -1;
	}

	int i;
	for (i = 0; appids[i] != NULL; i++) {
		g_print("%s\n", appids[i]);
	}

	g_strfreev(appids);

	return 0;
}
