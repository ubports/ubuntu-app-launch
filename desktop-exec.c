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
#include "desktop-exec-trace.h"
#include "recoverable-problem.h"

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

	g_setenv("LTTNG_UST_REGISTER_TIMEOUT", "0", FALSE); /* Set to zero if not set */
	tracepoint(upstart_app_launch, desktop_start);

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

	tracepoint(upstart_app_launch, desktop_starting_sent);

	gchar * desktopfilename = NULL;
	GKeyFile * keyfile = keyfile_for_appid(app_id, &desktopfilename);

	if (keyfile == NULL) {
		g_warning("Unable to find keyfile for application '%s'", app_id);

		const gchar * props[3] = {
			"AppId", NULL,
			NULL
		};
		props[1] = app_id;

		GPid pid = 0;
		const gchar * launcher_pid = g_getenv("APP_LAUNCHER_PID");
		if (launcher_pid != NULL) {
			pid = atoi(launcher_pid);
		}

		/* Checking to see if we're using the command line tool to create
		   the appid. Chances are in that case it's a user error, and we
		   don't need to automatically record it, the user mistyped. */
		gboolean debugtool = FALSE;
		if (pid != 0) {
			gchar * cmdpath = g_strdup_printf("/proc/%d/cmdline", pid);
			gchar * cmdline = NULL;

			if (g_file_get_contents(cmdpath, &cmdline, NULL, NULL)) {
				if (g_strstr_len(cmdline, -1, "ubuntu-app-launch") != NULL) {
					debugtool = TRUE;
				}

				g_free(cmdline);
			} else {
				/* The caller has already exited, probably a debug tool */
				debugtool = TRUE;
			}

			g_free(cmdpath);
		}

		if (!debugtool) {
			report_recoverable_problem("ubuntu-app-launch-invalid-appid", pid, TRUE, props);
		} else {
			g_debug("Suppressing appid recoverable error for debug tool");
		}
		return 1;
	}

	tracepoint(upstart_app_launch, desktop_found);

	EnvHandle * handle = env_handle_start();

	/* Desktop file name so that libs can get other info from it */
	if (desktopfilename != NULL) {
		env_handle_add(handle, "APP_DESKTOP_FILE_PATH", desktopfilename);
		g_free(desktopfilename);
	}

	if (g_key_file_has_key(keyfile, "Desktop Entry", "Path", NULL)) {
		gchar * path = g_key_file_get_string(keyfile, "Desktop Entry", "Path", NULL);
		env_handle_add(handle, "APP_DIR", path);
		g_free(path);
	}

	gchar * apparmor = g_key_file_get_string(keyfile, "Desktop Entry", "X-Ubuntu-AppArmor-Profile", NULL);
	if (apparmor != NULL) {
		env_handle_add(handle, "APP_EXEC_POLICY", apparmor);
		set_confined_envvars(handle, app_id, "/usr/share");
		g_free(apparmor);
	} else {
		env_handle_add(handle, "APP_EXEC_POLICY", "unconfined");
	}

	/* This string is quoted using desktop file quoting:
	   http://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#exec-variables */
	gchar * execline = desktop_to_exec(keyfile, app_id);
	g_return_val_if_fail(execline != NULL, 1);
	env_handle_add(handle, "APP_EXEC", execline);
	g_free(execline);

	g_key_file_free(keyfile);

	tracepoint(upstart_app_launch, desktop_send_env_vars);

	/* Sync the env vars with Upstart */
	env_handle_finish(handle);
	handle = NULL; /* make errors not love */

	tracepoint(upstart_app_launch, desktop_handshake_wait);

	starting_handshake_wait(handshake);

	tracepoint(upstart_app_launch, desktop_handshake_complete);

	g_object_unref(bus);

	return 0;
}
