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
#include <glib/gstdio.h>

#include "exec-line-exec-trace.h"
#include "helpers.h"

int
main (int argc, char * argv[])
{
	/* Make sure we have work to do */
	/* This string is quoted using desktop file quoting:
	   http://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#exec-variables */
	const gchar * app_exec = g_getenv("APP_EXEC");
	if (app_exec == NULL) {
		/* There should be no reason for this, a g_error() so that it gets
		   picked up by Apport and we can track it */
		g_error("No exec line given, nothing to do except fail");
		return 1;
	}

	g_setenv("LTTNG_UST_REGISTER_TIMEOUT", "0", FALSE); /* Set to zero if not set */
	tracepoint(upstart_app_launch, exec_start);

	/* URIs */
	const gchar * app_uris = g_getenv("APP_URIS");
	const gchar * app_desktop = g_getenv("APP_DESKTOP_FILE");

	/* Look to see if we have a directory defined that we
	   should be using for everything.  If so, change to it
	   and add it to the path */
	const gchar * appdir = g_getenv("APP_DIR");

	if (appdir != NULL) {
		if (g_chdir(appdir) != 0) {
			g_warning("Unable to change directory to '%s'", appdir);
		}
	}

	/* Protect against app directories that have ':' in them */
	if (appdir != NULL && strchr(appdir, ':') == NULL) {
		const gchar * path_path = g_getenv("PATH");
		gchar * path_libpath = NULL;
		const gchar * path_joinable[4] = { 0 };

		const gchar * import_path = g_getenv("QML2_IMPORT_PATH");
		gchar * import_libpath = NULL;
		const gchar * import_joinable[4] = { 0 };

		/* If we've got an architecture set insert that into the
		   path before everything else */
		const gchar * archdir = g_getenv("UPSTART_APP_LAUNCH_ARCH");
		if (archdir != NULL && strchr(archdir, ':') == NULL) {
			path_libpath = g_build_filename(appdir, "lib", archdir, "bin", NULL);
			import_libpath = g_build_filename(appdir, "lib", archdir, NULL);

			path_joinable[0] = path_libpath;
			path_joinable[1] = appdir;
			path_joinable[2] = path_path;

			/* Need to check whether the original is NULL because we're
			   appending instead of prepending */
			if (import_path == NULL) {
				import_joinable[0] = import_libpath;
			} else {
				import_joinable[0] = import_path;
				import_joinable[1] = import_libpath;
			}
		} else {
			path_joinable[0] = appdir;
			path_joinable[1] = path_path;

			import_joinable[0] = import_path;
		}

		gchar * newpath = g_strjoinv(":", (gchar**)path_joinable);
		g_setenv("PATH", newpath, TRUE);
		g_free(path_libpath);
		g_free(newpath);

		if (import_joinable[0] != NULL) {
			gchar * newimport = g_strjoinv(":", (gchar**)import_joinable);
			g_setenv("QML2_IMPORT_PATH", newimport, TRUE);
			g_free(newimport);
		}
		g_free(import_libpath);
	}

	/* Parse the execiness of it all */
	GArray * newargv = desktop_exec_parse(app_exec, app_uris);
	if (newargv == NULL) {
		g_warning("Unable to parse exec line '%s'", app_exec);
		return 1;
	}

	tracepoint(upstart_app_launch, exec_parse_complete);

	/* Surface flinger check */
	if (g_getenv("USING_SURFACE_FLINGER") != NULL && app_desktop != NULL) {
		gchar * sf = g_strdup_printf("--desktop_file_hint=%s", app_desktop);
		g_array_append_val(newargv, sf);
	}

	if (g_getenv("MIR_SOCKET") != NULL && g_strcmp0(g_getenv("APP_XMIR_ENABLE"), "1") == 0) {
		g_array_prepend_val(newargv, "xinit");
		/* Original command goes here */
		g_array_append_val(newargv, "--");
		g_array_append_val(newargv, "-mir");

		const gchar * appid = g_getenv("APP_ID");
		g_array_append_val(newargv, appid);
	}

	/* Now exec */
	gchar ** nargv = (gchar**)g_array_free(newargv, FALSE);

	tracepoint(upstart_app_launch, exec_pre_exec);

	int execret = execvp(nargv[0], nargv);

	if (execret != 0) {
		gchar * execprint = g_strjoinv(" ", nargv);
		g_warning("Unable to exec '%s' in '%s': %s", execprint, appdir, strerror(errno));
		g_free(execprint);
	}

	return execret;
}
