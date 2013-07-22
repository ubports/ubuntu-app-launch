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

typedef struct _app_state_t app_state_t;
struct _app_state_t {
	gchar * app_id;
	gboolean has_click;
	gboolean has_app;
};

/* Look at an click package entry */
void
add_click_package (const gchar * name, GArray * app_array)
{


	return;
}

/* Look at an desktop file entry */
void
add_desktop_file (const gchar * name, GArray * app_array)
{


	return;
}

/* Open a directory and look at all the entries */
void
dir_for_each (const gchar * dirname, void(*func)(const gchar * name, GArray * app_array), GArray * app_array)
{


	return;
}

int
main (int argc, char * argv[])
{
	if (argc != 1) {
		g_error("Shouldn't have arguments");
		return 1;
	}

	GArray * apparray = g_array_new(FALSE, FALSE, sizeof(app_state_t));

	gchar * symlinkdir = g_build_filename(g_get_user_cache_dir(), "upstart-app-lauch", "desktop", NULL);
	if (!g_file_test(symlinkdir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		g_warning("No installed click packages");
	} else {
		dir_for_each(symlinkdir, add_click_package, apparray);
	}

	gchar * desktopdir = g_build_filename(g_get_user_data_dir(), "applications", NULL);
	if (!g_file_test(symlinkdir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		g_warning("No applications defined");
	} else {
		dir_for_each(desktopdir, add_desktop_file, apparray);
	}


	return 0;
}
