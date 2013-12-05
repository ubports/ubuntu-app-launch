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
		g_warning("No exec line given, nothing to do except fail");
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

		const gchar * path = g_getenv("PATH");
		gchar * newpath = g_strdup_printf("%s:%s", appdir, path);
		g_setenv("PATH", newpath, TRUE);
		g_free(newpath);
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

	/* Now exec */
	gchar ** nargv = (gchar**)g_array_free(newargv, FALSE);

	tracepoint(upstart_app_launch, exec_pre_exec);

	int execret = execvp(nargv[0], nargv);

	if (execret != 0) {
		g_warning("Unable to exec: %s", strerror(errno));
	}

	return execret;
}
