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

	if (argc != 2) {
		g_printerr("Usage: %s <app id>\n", argv[0]);
		return 1;
	}

	GPid pid = upstart_app_launch_check_app_running(argv[1]);

	if (pid == 0) {
		return 1;
	}

	g_print("%d\n", pid);
	return 0;
}
