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

	return 0;
}
