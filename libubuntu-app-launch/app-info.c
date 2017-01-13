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

#include <json-glib/json-glib.h>

#include "ubuntu-app-launch.h"

/* Prototypes */
static gboolean app_info_libertine (const gchar * appid, gchar ** appdir, gchar ** appdesktop);

/* Look to see if the app id results in a desktop file, if so, fill in the params */
static gboolean
evaluate_dir (const gchar * dir, const gchar * desktop, gchar ** appdir, gchar ** appdesktop)
{
	char * fulldir = g_build_filename(dir, "applications", desktop, NULL);
	gboolean found = FALSE;

	if (g_file_test(fulldir, G_FILE_TEST_EXISTS)) {
		if (appdir != NULL) {
			*appdir = g_strdup(dir);
		}

		if (appdesktop != NULL) {
			*appdesktop = g_strdup_printf("applications/%s", desktop);
		}

		found = TRUE;
	}

	g_free(fulldir);
	return found;
}

/* Handle the legacy case where we look through the data directories */
static gboolean
app_info_legacy (const gchar * appid, gchar ** appdir, gchar ** appdesktop)
{
	gchar * desktop = g_strdup_printf("%s.desktop", appid);

	/* Special case the user's dir */
	if (evaluate_dir(g_get_user_data_dir(), desktop, appdir, appdesktop)) {
		g_free(desktop);
		return TRUE;
	}

	const char * const * data_dirs = g_get_system_data_dirs();
	int i;
	for (i = 0; data_dirs[i] != NULL; i++) {
		if (evaluate_dir(data_dirs[i], desktop, appdir, appdesktop)) {
			g_free(desktop);
			return TRUE;
		}
	}

	return FALSE;
}

/* Handle the libertine case where we look in the container */
static gboolean
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

	if (appdir != NULL) {
		*appdir = desktopdir;
	} else {
		g_free(desktopdir);
	}

	if (appdesktop != NULL) {
		*appdesktop = g_build_filename("applications", desktopname, NULL);
	}

	g_free(desktopfile);
	g_free(desktopname);
	g_free(container);
	g_free(app);

	return TRUE;
}

/* Determine whether an AppId is realated to a Libertine container by
   checking the container and program name. */
static gboolean
is_libertine (const gchar * appid)
{
	if (app_info_libertine(appid, NULL, NULL)) {
		g_debug("Libertine application detected: %s", appid);
		return TRUE;
	} else {
		return FALSE;
	}
}

gboolean
ubuntu_app_launch_application_info (const gchar * appid, gchar ** appdir, gchar ** appdesktop)
{
	if (is_libertine(appid)) {
		return app_info_libertine(appid, appdir, appdesktop);
	} else {
		return app_info_legacy(appid, appdir, appdesktop);
	}
}

