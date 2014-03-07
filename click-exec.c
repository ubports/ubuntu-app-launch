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
#include "click-exec-trace.h"

/*

INTRODUCTION:

This is the utility that executes a click package based on the Application ID.
Actually it just determines what needs to be executed, and asks Upstart to execute
it so that it can be tracked better.  This process runs OUTSIDE of the app armor
confinement for the application.  It also DOES NOT use any files that can be modified
by the user.  So things like the desktop file in ~/.local/share/applications are
all off limits.

For information on Click packages and the manifest look at the Click package documentation:

https://click.readthedocs.org/en/latest/

*/

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

	tracepoint(upstart_app_launch, click_start);

	/* Ensure we keep one connection open to the bus for the entire
	   script even though different people need it throughout */
	GError * error = NULL;
	GDBusConnection * bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
	if (error != NULL) {
		g_error("Unable to get session bus: %s", error->message);
		g_error_free(error);
		return 1;
	}

	handshake_t * handshake = starting_handshake_start(app_id);
	if (handshake == NULL) {
		g_warning("Unable to setup starting handshake");
	}

	tracepoint(upstart_app_launch, click_starting_sent);

	gchar * package = NULL;
	/* 'Parse' the App ID */
	if (!app_id_to_triplet(app_id, &package, NULL, NULL)) {
		g_warning("Unable to parse App ID: '%s'", app_id);
		return 1;
	}

	/* Check click to find out where the files are */
	gchar * cmdline = g_strdup_printf("click pkgdir \"%s\"", package);

	gchar * output = NULL;
	g_spawn_command_line_sync(cmdline, &output, NULL, NULL, &error);
	g_free(cmdline);

	tracepoint(upstart_app_launch, click_found_pkgdir);

	/* If we have an extra newline, we can delete it. */
	gchar * newline = g_strstr_len(output, -1, "\n");
	if (newline != NULL) {
		newline[0] = '\0';
	}

	if (error != NULL) {
		g_warning("Unable to get the package directory from click: %s", error->message);
		g_error_free(error);
		g_free(output); /* Probably not set, but just in case */
		g_free(package);
		return 1;
	}

	if (!g_file_test(output, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		g_warning("Application directory '%s' doesn't exist", output);
		g_free(output);
		g_free(package);
		return 1;
	}

	g_debug("Setting 'APP_DIR' to '%s'", output);
	set_upstart_variable("APP_DIR", output, FALSE);

	set_confined_envvars(package, output);

	tracepoint(upstart_app_launch, click_configured_env);

	gchar * desktopfile = manifest_to_desktop(output, app_id);

	g_free(output);
	g_free(package);

	if (desktopfile == NULL) {
		g_warning("Desktop file unable to be found");
		return 1;
	}

	tracepoint(upstart_app_launch, click_read_manifest);

	GKeyFile * keyfile = g_key_file_new();

	set_upstart_variable("APP_DESKTOP_FILE_PATH", desktopfile, FALSE);
	g_key_file_load_from_file(keyfile, desktopfile, 0, &error);
	if (error != NULL) {
		g_warning("Unable to load desktop file '%s': %s", desktopfile, error->message);
		g_error_free(error);
		g_key_file_free(keyfile);
		g_free(desktopfile);
		return 1;
	}

	/* This string is quoted using desktop file quoting:
	   http://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#exec-variables */
	gchar * exec = desktop_to_exec(keyfile, desktopfile);
	if (exec == NULL) {
		return 1;
	}

	tracepoint(upstart_app_launch, click_read_desktop);

	/* TODO: This is for Surface Flinger, when we drop support we can drop this */
	gchar * userdesktopfile = g_strdup_printf("%s.desktop", app_id);
	gchar * userdesktoppath = g_build_filename(g_get_home_dir(), ".local", "share", "applications", userdesktopfile, NULL);
	set_upstart_variable("APP_DESKTOP_FILE", userdesktoppath, FALSE);
	g_free(userdesktopfile);
	g_free(userdesktoppath);

	g_debug("Setting 'APP_EXEC' to '%s'", exec);
	/* NOTE: This should be the last upstart variable set as it is sync
	   so it will wait for a reply from Upstart implying that Upstart
	   has seen all the other variable requests we made */
	set_upstart_variable("APP_EXEC", exec, TRUE);

	g_free(exec);
	g_key_file_unref(keyfile);
	g_free(desktopfile);

	tracepoint(upstart_app_launch, click_handshake_wait);

	starting_handshake_wait(handshake);

	tracepoint(upstart_app_launch, click_handshake_complete);

	g_object_unref(bus);

	return 0;
}
