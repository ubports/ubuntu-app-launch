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
#include <click.h>
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
	ClickDB * db = click_db_new();
	/* If TEST_CLICK_DB is unset, this reads the system database. */
	click_db_read(db, g_getenv("TEST_CLICK_DB"), &error);
	if (error != NULL) {
		g_warning("Unable to read Click database: %s", error->message);
		g_error_free(error);
		g_free(package);
		return 1;
	}
	/* If TEST_CLICK_USER is unset, this uses the current user name. */
	ClickUser * user = click_user_new_for_user(db, g_getenv("TEST_CLICK_USER"), &error);
	if (error != NULL) {
		g_warning("Unable to read Click database: %s", error->message);
		g_error_free(error);
		g_free(package);
		g_object_unref(db);
		return 1;
	}
	gchar * pkgdir = click_user_get_path(user, package, &error);
	if (error != NULL) {
		g_warning("Unable to get the Click package directory for %s: %s", package, error->message);
		g_error_free(error);
		g_free(package);
		g_object_unref(user);
		g_object_unref(db);
		return 1;
	}
	g_object_unref(user);
	g_object_unref(db);

	tracepoint(upstart_app_launch, click_found_pkgdir);

	if (!g_file_test(pkgdir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		g_warning("Application directory '%s' doesn't exist", pkgdir);
		g_free(pkgdir);
		g_free(package);
		return 1;
	}

	g_debug("Setting 'APP_DIR' to '%s'", pkgdir);
	set_upstart_variable("APP_DIR", pkgdir, FALSE);

	set_confined_envvars(package, pkgdir);

	tracepoint(upstart_app_launch, click_configured_env);

	gchar * desktopfile = manifest_to_desktop(pkgdir, app_id);

	g_free(pkgdir);
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
