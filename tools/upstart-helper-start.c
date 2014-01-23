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
#include <gio/gio.h>

int
main (int argc, gchar * argv[]) {
	if (argc != 3) {
		g_printerr("Usage: %s <helper type> <app id>\n", argv[0]);
		return 1;
	}

	GDBusConnection * con = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	g_return_val_if_fail(con != NULL, -1);

	int retval = -1;

	if (upstart_app_launch_start_helper(argv[1], argv[2])) {
		retval = 0;
	} else {
		g_debug("Unable to start app id '%s' of type '%s'", argv[2], argv[1]);
	}

	g_dbus_connection_flush_sync(con, NULL, NULL);
	g_object_unref(con);

	return retval; 
}
