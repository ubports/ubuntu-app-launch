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
#include "app-info.h"

JsonObject * get_manifest (const gchar * pkg, gchar ** pkgpath);

/* Look to see if the app id results in a desktop file, if so, fill in the params */
static gboolean
evaluate_dir (const gchar * dir, const gchar * desktop, gchar ** appdir, gchar ** appdesktop)
{
	char * fulldir = g_build_filename(dir, "applications", desktop, NULL);

	if (g_file_test(fulldir, G_FILE_TEST_EXISTS)) {
		if (appdir != NULL) {
			*appdir = g_strdup(dir);
		}

		if (appdesktop != NULL) {
			*appdesktop = g_strdup_printf("applications/%s", desktop);
		}
	}

	g_free(fulldir);
	return FALSE;
}

/* Handle the legacy case where we look through the data directories */
gboolean
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

/* Get the information on where the desktop file is from libclick */
gboolean
app_info_click (const gchar * appid, gchar ** appdir, gchar ** appdesktop)
{
	gchar * package = NULL;
	gchar * application = NULL;

	if (!ubuntu_app_launch_app_id_parse(appid, &package, &application, NULL)) {
		return FALSE;
	}

	JsonObject * manifest = get_manifest(package, appdir);
	if (manifest == NULL) {
		g_free(package);
		g_free(application);
		return FALSE;
	}

	g_free(package);

	if (appdesktop != NULL) {
		JsonObject * hooks = json_object_get_object_member(manifest, "hooks");
		if (hooks == NULL) {
			json_object_unref(manifest);
			g_free(application);
			return FALSE;
		}

		JsonObject * appobj = json_object_get_object_member(hooks, application);
		g_free(application);

		if (appobj == NULL) {
			json_object_unref(manifest);
			return FALSE;
		}

		const gchar * desktop = json_object_get_string_member(appobj, "desktop");
		if (desktop == NULL) {
			json_object_unref(manifest);
			return FALSE;
		}

		*appdesktop = g_strdup(desktop);
	} else {
		g_free(application);
	}

	json_object_unref(manifest);

	return TRUE;
}

