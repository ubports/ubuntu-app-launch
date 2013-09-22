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

#include <glib.h>
#include <glib/gstdio.h>

int
main (int argc, char * argv[])
{
	/* Make sure we have work to do */
	const gchar * app_exec = g_getenv("APP_EXEC");
	if (app_exec == NULL) {
		g_warning("No exec line given, nothing to do except fail");
		return 1;
	}

	/* URIs */
	const gchar * app_uris = g_getenv("APP_URIS");

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

	return 0;
}
