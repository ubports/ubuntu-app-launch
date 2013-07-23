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
#include "helpers.h"

int
main (int argc, char * argv[])
{
	if (argc != 2 && argc != 3) {
		g_error("Should be called as: %s <app_id> [uri list]", argv[0]);
		return 1;
	}

	gchar * symlinkdir = g_build_filename(g_get_user_cache_dir(), "upstart-app-launch", "desktop", NULL);
	if (!g_file_test(symlinkdir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		g_warning("Application directory '%s' doesn't exist", symlinkdir);
		return 1;
	}

	gchar * desktopfile = manifest_to_desktop(symlinkdir, argv[1]);
	if (desktopfile != NULL) {
		return 1;
	}

	GKeyFile * keyfile = g_key_file_new();
	GError * error = NULL;

	g_key_file_load_from_file(keyfile, desktopfile, 0, &error);
	if (error != NULL) {
		g_warning("Unable to load desktop file '%s': %s", desktopfile, error->message);
		g_error_free(error);
		return 1;
	}

	gchar * exec = desktop_to_exec(keyfile, desktopfile);
	if (exec == NULL) {
		return 1;
	}

	set_upstart_variable("APP_EXEC", exec);

	g_free(exec);
	g_key_file_unref(keyfile);
	g_free(desktopfile);
	g_free(symlinkdir);

	return 0;
}
