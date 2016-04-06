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
#include <libwhoopsie/recoverable-problem.h>

#include "helpers.h"
#include "ubuntu-app-launch-trace.h"
#include "ual-tracepoint.h"
#include "ubuntu-app-launch.h"
#include "app-info.h"

/* Reports an error on the caller of UAL so that we can track
   who is trying to launch bad AppIDs, and then fix their bug
   so that we get better reporting upstream. */
void
report_error_on_caller (const gchar * app_id) {
	g_warning("Unable to find keyfile for application '%s'", app_id);

	const gchar * props[3] = {
		"AppId", NULL,
		NULL
	};
	props[1] = app_id;

	GPid pid = getpid();

	/* Checking to see if we're using the command line tool to create
	   the appid. Chances are in that case it's a user error, and we
	   don't need to automatically record it, the user mistyped. */
	gboolean debugtool = FALSE;
	if (pid != 0) {
		const gchar * cmdpath = "/proc/self/cmdline";
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
	}

	if (!debugtool) {
		whoopsie_report_recoverable_problem("ubuntu-app-launch-invalid-appid", pid, TRUE, props);
	} else {
		g_debug("Suppressing appid recoverable error for debug tool");
	}
}

/* Get the keyfile object for a libertine container based application. Look into
   the container's filesystem on disk and find it in /usr/share/applications in there.
   Those are currently the only apps that we look at today. We're not ensuring anything
   about the file other than it has basic sanity. */
GKeyFile *
keyfile_for_libertine (const gchar * appid, gchar ** outcontainer)
{
	gchar * desktopfile = NULL;
	gchar * desktopdir = NULL;

	if (!app_info_libertine(appid, &desktopdir, &desktopfile)) {
		return NULL;
	}

	gchar * desktopfull = g_build_filename(desktopdir, desktopfile, NULL);
	g_debug("Desktop full: %s", desktopfull);
	g_free(desktopdir);
	g_free(desktopfile);

	/* We now think we have a valid 'desktopfile' path */
	GKeyFile * keyfile = g_key_file_new();
	gboolean loaded = g_key_file_load_from_file(keyfile, desktopfull, G_KEY_FILE_NONE, NULL);

	if (!loaded) {
		g_free(desktopfull);
		g_key_file_free(keyfile);
		return NULL;
	}

	if (!verify_keyfile(keyfile, desktopfull)) {
		g_free(desktopfull);
		g_key_file_free(keyfile);
		return NULL;
	}

	g_free(desktopfull);

	if (outcontainer != NULL) {
		ubuntu_app_launch_app_id_parse(appid, outcontainer, NULL, NULL);
	}

	return keyfile;
}

gboolean
desktop_task_setup (GDBusConnection * bus, const gchar * app_id, EnvHandle * handle, gboolean is_libertine)
{
	if (app_id == NULL) {
		g_error("No APP_ID environment variable defined");
		return FALSE;
	}

	ual_tracepoint(desktop_start, app_id);

	handshake_t * handshake = starting_handshake_start(app_id);
	if (handshake == NULL) {
		g_warning("Unable to setup starting handshake");
	}

	ual_tracepoint(desktop_starting_sent, app_id);

	gchar * desktopfilename = NULL;
	GKeyFile * keyfile = NULL;
	gchar * libertinecontainer = NULL;
	if (is_libertine) {
		/* desktopfilename not set, not useful in this context */
		keyfile = keyfile_for_libertine(app_id, &libertinecontainer);
	} else {
		keyfile = keyfile_for_appid(app_id, &desktopfilename);
	}

	if (keyfile == NULL) {
		report_error_on_caller(app_id);
		g_free(libertinecontainer);
		return FALSE;
	}

	ual_tracepoint(desktop_found, app_id);

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

	if (g_key_file_has_key(keyfile, "Desktop Entry", "X-Ubuntu-XMir-Enable", NULL)) {
		if (g_key_file_get_boolean(keyfile, "Desktop Entry", "X-Ubuntu-XMir-Enable", NULL)) {
			env_handle_add(handle, "APP_XMIR_ENABLE", "1");
		} else {
			env_handle_add(handle, "APP_XMIR_ENABLE", "0");
		}
	} else if (is_libertine) {
		/* Default to X for libertine stuff */
		env_handle_add(handle, "APP_XMIR_ENABLE", "1");
	}

	/* This string is quoted using desktop file quoting:
	   http://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#exec-variables */
	gchar * execline = desktop_to_exec(keyfile, app_id);
	g_return_val_if_fail(execline != NULL, 1);

	if (is_libertine) {
		static const gchar * libertine_launch = NULL;
		if (G_UNLIKELY(libertine_launch == NULL)) {
			libertine_launch = g_getenv("UBUNTU_APP_LAUNCH_LIBERTINE_LAUNCH");
			if (libertine_launch == NULL) {
				libertine_launch = LIBERTINE_LAUNCH;
			}
		}

		gchar * libexec = g_strdup_printf("%s \"%s\" %s", libertine_launch, libertinecontainer, execline);
		g_free(execline);
		execline = libexec;
	}
	g_free(libertinecontainer); /* Handles NULL, let's be sure it goes away */

	env_handle_add(handle, "APP_EXEC", execline);
	g_free(execline);

	g_key_file_free(keyfile);

	ual_tracepoint(handshake_wait, app_id);

	starting_handshake_wait(handshake);

	ual_tracepoint(handshake_complete, app_id);

	return TRUE;
}
