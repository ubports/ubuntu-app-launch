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
#include "ubuntu-app-launch-trace.h"
#include "ual-tracepoint.h"

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

gboolean
click_task_setup (const gchar * app_id, EnvHandle * handle)
{
	if (app_id == NULL) {
		g_error("No APP ID defined");
		return FALSE;
	}

	ual_tracepoint(click_start, app_id);

	/* Ensure we keep one connection open to the bus for the entire
	   script even though different people need it throughout */
	GError * error = NULL;
	GDBusConnection * bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
	if (error != NULL) {
		g_error("Unable to get session bus: %s", error->message);
		g_error_free(error);
		return FALSE;
	}

	handshake_t * handshake = starting_handshake_start(app_id);
	if (handshake == NULL) {
		g_warning("Unable to setup starting handshake");
	}

	ual_tracepoint(click_starting_sent, app_id);

	gchar * package = NULL;
	/* 'Parse' the App ID */
	if (!app_id_to_triplet(app_id, &package, NULL, NULL)) {
		g_warning("Unable to parse App ID: '%s'", app_id);
		return FALSE;
	}

	/* Check click to find out where the files are */
	ClickDB * db = click_db_new();
	/* If TEST_CLICK_DB is unset, this reads the system database. */
	click_db_read(db, g_getenv("TEST_CLICK_DB"), &error);
	if (error != NULL) {
		g_warning("Unable to read Click database: %s", error->message);
		g_error_free(error);
		g_free(package);
		return FALSE;
	}
	/* If TEST_CLICK_USER is unset, this uses the current user name. */
	ClickUser * user = click_user_new_for_user(db, g_getenv("TEST_CLICK_USER"), &error);
	if (error != NULL) {
		g_warning("Unable to read Click database: %s", error->message);
		g_error_free(error);
		g_free(package);
		g_object_unref(db);
		return FALSE;
	}
	gchar * pkgdir = click_user_get_path(user, package, &error);
	if (error != NULL) {
		g_warning("Unable to get the Click package directory for %s: %s", package, error->message);
		g_error_free(error);
		g_free(package);
		g_object_unref(user);
		g_object_unref(db);
		return FALSE;
	}
	g_object_unref(user);
	g_object_unref(db);

	ual_tracepoint(click_found_pkgdir, app_id);

	if (!g_file_test(pkgdir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		g_warning("Application directory '%s' doesn't exist", pkgdir);
		g_free(pkgdir);
		g_free(package);
		return FALSE;
	}

	g_debug("Setting 'APP_DIR' to '%s'", pkgdir);
	env_handle_add(handle, "APP_DIR", pkgdir);

	set_confined_envvars(handle, package, pkgdir);

	ual_tracepoint(click_configured_env, app_id);

	gchar * desktopfile = manifest_to_desktop(pkgdir, app_id);

	g_free(pkgdir);
	g_free(package);

	if (desktopfile == NULL) {
		g_warning("Desktop file unable to be found");
		return FALSE;
	}

	ual_tracepoint(click_read_manifest, app_id);

	GKeyFile * keyfile = g_key_file_new();

	env_handle_add(handle, "APP_DESKTOP_FILE_PATH", desktopfile);
	g_key_file_load_from_file(keyfile, desktopfile, 0, &error);
	if (error != NULL) {
		g_warning("Unable to load desktop file '%s': %s", desktopfile, error->message);
		g_error_free(error);
		g_key_file_free(keyfile);
		g_free(desktopfile);
		return FALSE;
	}

	/* This string is quoted using desktop file quoting:
	   http://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#exec-variables */
	gchar * exec = desktop_to_exec(keyfile, desktopfile);
	if (exec == NULL) {
		return FALSE;
	}

	ual_tracepoint(click_read_desktop, app_id);

	g_debug("Setting 'APP_EXEC' to '%s'", exec);
	env_handle_add(handle, "APP_EXEC", exec);

	g_free(exec);
	g_key_file_unref(keyfile);
	g_free(desktopfile);

	ual_tracepoint(handshake_wait, app_id);

	starting_handshake_wait(handshake);

	ual_tracepoint(handshake_complete, app_id);

	g_object_unref(bus);

	return TRUE;
}
