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

#include <gio/gio.h>
#include "libupstart-app-launch/upstart-app-launch.h"

int retval = 0;
const gchar * global_appid;

static void
app_started (const gchar * appid, gpointer user_data)
{
	if (g_strcmp0(appid, global_appid) != 0)
		return;
	g_debug("Application Started: %s", appid);
	g_main_loop_quit((GMainLoop *)user_data);
}

static void
app_focus (const gchar * appid, gpointer user_data)
{
	if (g_strcmp0(appid, global_appid) != 0)
		return;
	g_debug("Application Focused");
	g_main_loop_quit((GMainLoop *)user_data);
}

static void
app_failed (const gchar * appid, upstart_app_launch_app_failed_t failure_type, gpointer user_data)
{
	if (g_strcmp0(appid, global_appid) != 0)
		return;
	g_warning("Application Startup Failed");
	retval = 1;
	g_main_loop_quit((GMainLoop *)user_data);
}

int
main (int argc, char * argv[])
{
	GDBusConnection * con = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	g_return_val_if_fail(con != NULL, 1);

	global_appid = g_getenv("APP_ID");
	g_return_val_if_fail(global_appid != NULL, 1);

	const gchar * uris_str = g_getenv("APP_URIS");
	gchar ** uris = NULL;
	if (uris_str != NULL) {
		GError * error = NULL;
		gint uri_count = 0;
		g_shell_parse_argv(uris_str, &uri_count, &uris, &error);

		if (error != NULL) {
			g_warning("Unable to parse uris '%s': %s", uris_str, error->message);
			g_error_free(error);
		} else {
			g_debug("Got %d URIs", uri_count);
		}
	}

	GMainLoop * mainloop = g_main_loop_new(NULL, FALSE);

	upstart_app_launch_observer_add_app_started(app_started, mainloop);
	upstart_app_launch_observer_add_app_focus(app_focus, mainloop);
	upstart_app_launch_observer_add_app_failed(app_failed, mainloop);

	g_debug("Start Application: %s", global_appid);
	upstart_app_launch_start_application(global_appid, (const gchar * const *)uris);
	g_strfreev(uris);

	g_debug("Wait for results");
	g_main_loop_run(mainloop);
	g_main_loop_unref(mainloop);

	g_object_unref(con);

	return retval;
}
