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
#include "helpers.h"

/*

INTRODUCTION:

This is the utility that executes a click package based on the Application ID.
Actually it just determines what needs to be executed, and asks Upstart to execute
it so that it can be tracked better.  This process runs OUTSIDE of the app armor
confinement for the application.  It also DOES NOT use any files that can be modified
by the user.  So things like the desktop file in ~/.local/share/applications are
all off limits.

For information on Click packages and the manifest look at the Click package documentation:

https://click-package.readthedocs.org/en/latest/

*/

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
	if (argc != 1 && argc != 3) {
		g_error("Should be called as: %s", argv[0]);
		return 1;
	}

	const gchar * app_id = g_getenv("APP_ID");

	if (app_id == NULL) {
		g_error("No APP ID defined");
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

	gchar * package = NULL;
	/* 'Parse' the App ID */
	if (!app_id_to_triplet(app_id, &package, NULL, NULL)) {
		g_warning("Unable to parse App ID: '%s'", app_id);
		return 1;
	}

	/* Check click to find out where the files are */
	gchar * cmdline = g_strdup_printf("click pkgdir \"%s\"", package);
	g_free(package);

	gchar * output = NULL;
	g_spawn_command_line_sync(cmdline, &output, NULL, NULL, &error);
	g_free(cmdline);

	/* If we have an extra newline, we can delete it. */
	gchar * newline = g_strstr_len(output, -1, "\n");
	if (newline != NULL) {
		newline[0] = '\0';
	}

	if (error != NULL) {
		g_warning("Unable to get the package directory from click: %s", error->message);
		g_error_free(error);
		g_free(output); /* Probably not set, but just in case */
		return 1;
	}

	if (!g_file_test(output, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		g_warning("Application directory '%s' doesn't exist", output);
		g_free(output);
		return 1;
	}

	g_debug("Setting 'APP_DIR' to '%s'", output);
	set_upstart_variable("APP_DIR", output);

	set_confined_envvars(package, output);

	gchar * desktopfile = manifest_to_desktop(output, app_id);
	g_free(output);
	if (desktopfile == NULL) {
		g_warning("Desktop file unable to be found");
		return 1;
	}

	GKeyFile * keyfile = g_key_file_new();

	g_key_file_load_from_file(keyfile, desktopfile, 0, &error);
	if (error != NULL) {
		g_warning("Unable to load desktop file '%s': %s", desktopfile, error->message);
		g_error_free(error);
		return 1;
	}

	/* This string is quoted using desktop file quoting:
	   http://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#exec-variables */
	gchar * exec = desktop_to_exec(keyfile, desktopfile);
	if (exec == NULL) {
		return 1;
	}

	g_debug("Setting 'APP_EXEC' to '%s'", exec);
	set_upstart_variable("APP_EXEC", exec);

	g_free(exec);
	g_key_file_unref(keyfile);
	g_free(desktopfile);

	/* TODO: This is for Surface Flinger, when we drop support we can drop this */
	gchar * userdesktopfile = g_strdup_printf("%s.desktop", app_id);
	gchar * userdesktoppath = g_build_filename(g_get_home_dir(), ".local", "share", "applications", userdesktopfile, NULL);
	set_upstart_variable("APP_DESKTOP_FILE", userdesktoppath);
	g_free(userdesktopfile);
	g_free(userdesktoppath);

	g_main_loop_run(mainloop);

	g_main_loop_unref(mainloop);
	g_object_unref(con);

	return 0;
}
