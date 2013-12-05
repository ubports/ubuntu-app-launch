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

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <glib.h>
#include <gio/gio.h>

#include "helpers.h"

static void
unity_signal_cb (GDBusConnection * con, const gchar * sender, const gchar * path, const gchar * interface, const gchar * signal, GVariant * params, gpointer user_data)
{
	GMainLoop * mainloop = (GMainLoop *)user_data;
	g_main_loop_quit(mainloop);
}

static gboolean
unity_too_slow_cb (gpointer user_data)
{
	GMainLoop * mainloop = (GMainLoop *)user_data;
	g_main_loop_quit(mainloop);
	return G_SOURCE_REMOVE;
}

int
main (int argc, char * argv[])
{
	if (argc != 1) {
		g_error("Should be called as: %s", argv[0]);
		return 1;
	}

	const gchar * app_id = g_getenv("APP_ID");

	if (app_id == NULL) {
		g_error("No APP_ID environment variable defined");
		return 1;
	}

	GMainLoop * mainloop = g_main_loop_new(NULL, FALSE);

	GError * error = NULL;
	GDBusConnection * con = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
	if (error != NULL) {
		g_critical("Unable to connect to session bus: %s", error->message);
		g_error_free(error);
		return 1;
	}

	/* Set up listening for the unfrozen signal from Unity */
	g_dbus_connection_signal_subscribe(con,
		NULL, /* sender */
		"com.canonical.UpstartAppLaunch", /* interface */
		"UnityStartingSignal", /* signal */
		"/", /* path */
		app_id, /* arg0 */
		G_DBUS_SIGNAL_FLAGS_NONE,
		unity_signal_cb, mainloop,
		NULL); /* user data destroy */

	/* Send unfreeze to to Unity */
	g_dbus_connection_emit_signal(con,
		NULL, /* destination */
		"/", /* path */
		"com.canonical.UpstartAppLaunch", /* interface */
		"UnityStartingBroadcast", /* signal */
		g_variant_new("(s)", app_id),
		&error);

	/* Really, Unity? */
	g_timeout_add_seconds(1, unity_too_slow_cb, mainloop);

	gchar * desktopfilename = NULL;
	GKeyFile * keyfile = keyfile_for_appid(app_id, &desktopfilename);

	if (keyfile == NULL) {
		g_error("Unable to find keyfile for application '%s'", app_id);
		return 1;
	}

	/* This string is quoted using desktop file quoting:
	   http://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#exec-variables */
	gchar * execline = desktop_to_exec(keyfile, app_id);
	g_return_val_if_fail(execline != NULL, 1);
	set_upstart_variable("APP_EXEC", execline);
	g_free(execline);

	if (g_key_file_has_key(keyfile, "Desktop Entry", "Path", NULL)) {
		gchar * path = g_key_file_get_string(keyfile, "Desktop Entry", "Path", NULL);
		set_upstart_variable("APP_DIR", path);
		g_free(path);
	}

	gchar * apparmor = g_key_file_get_string(keyfile, "Desktop Entry", "X-Ubuntu-AppArmor-Profile", NULL);
	if (apparmor != NULL) {
		set_upstart_variable("APP_EXEC_POLICY", apparmor);
		set_confined_envvars(app_id, "/usr/share");
		g_free(apparmor);
	} else {
		set_upstart_variable("APP_EXEC_POLICY", "unconfined");
	}

	g_key_file_free(keyfile);

	/* TODO: This is for Surface Flinger.  When we drop support, we can drop this code */
	if (desktopfilename != NULL) {
		set_upstart_variable("APP_DESKTOP_FILE", desktopfilename);
		g_free(desktopfilename);
	}

	g_main_loop_run(mainloop);

	g_main_loop_unref(mainloop);
	g_object_unref(con);

	return 0;
}
