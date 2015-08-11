/*
 * Copyright © 2015 Canonical Ltd.
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

#include "ubuntu-app-launch.h"
#include "app-info.h"

/* Handle the libertine case where we look in the container */
gboolean
app_info_libertine (const gchar * appid, gchar ** appdir, gchar ** appdesktop)
{
	char * container = NULL;
	char * app = NULL;

	if (!ubuntu_app_launch_app_id_parse(appid, &container, &app, NULL)) {
		return FALSE;
	}

	gchar * desktopname = g_strdup_printf("%s.desktop", app);

	gchar * desktopdir = g_build_filename(g_get_user_cache_dir(), "libertine-container", container, "rootfs", "usr", "share", NULL);
	gchar * desktopfile = g_build_filename(desktopdir, "applications", desktopname, NULL);

	if (!g_file_test(desktopfile, G_FILE_TEST_EXISTS)) {
		g_free(desktopdir);
		g_free(desktopfile);

		desktopdir = g_build_filename(g_get_user_data_dir(), "libertine-container", "user-data", container, ".local", "share", NULL);
		desktopfile = g_build_filename(desktopdir, "applications", desktopname, NULL);

		if (!g_file_test(desktopfile, G_FILE_TEST_EXISTS)) {
			g_free(desktopdir);
			g_free(desktopfile);

			g_free(desktopname);
			g_free(container);
			g_free(app);

			return FALSE;
		}
	}

	g_free(desktopname);
	g_free(container);
	g_free(app);

	if (appdir != NULL) {
		*appdir = desktopdir;
	} else {
		g_free(desktopdir);
	}

	if (appdesktop != NULL) {
		*appdesktop = desktopfile;
	} else {
		g_free(desktopfile);
	}

	return TRUE;
}
