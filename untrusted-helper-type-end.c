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

#include "libubuntu-app-launch/ubuntu-app-launch.h"
#include <gio/gio.h>

int
main (int argc, gchar * argv[]) {
	const gchar * type = g_getenv("HELPER_TYPE");
	g_return_val_if_fail(type != NULL, -1);

	GDBusConnection * con = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	g_return_val_if_fail(con != NULL, -1);

	gchar ** appids = ubuntu_app_launch_list_helpers(type);
	if (appids == NULL) {
		g_warning("Error getting App IDs for helper type '%s'", argv[1]);
		return -1;
	}

	int i;
	for (i = 0; appids[i] != NULL; i++) {
		gchar ** instances = ubuntu_app_launch_list_helper_instances(type, appids[i]);
		guint instance_cnt = g_strv_length(instances);

		if (instance_cnt == 0) {
			g_debug("Stopping %s", appids[i]);
			if (!ubuntu_app_launch_stop_helper(type, appids[i])) {
				g_warning("Unable to stop '%s'", appids[i]);
			}
		} else {
			int j;
			for (j = 0; j < instance_cnt; j++) {
				g_debug("Stopping %s (%s)", appids[i], instances[j]);
				if (!ubuntu_app_launch_stop_multiple_helper(type, appids[i], instances[j])) {
					g_warning("Unable to stop '%s' instance '%s'", appids[i], instances[j]);
				}
			}
		}

		g_strfreev(instances);
	}

	g_strfreev(appids);

	g_dbus_connection_flush_sync(con, NULL, NULL);
	g_object_unref(con);

	return 0;
}
