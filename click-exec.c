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

	GError * error = NULL;
	gchar * package = NULL;
	/* 'Parse' the App ID */
	if (!app_id_to_triplet(argv[1], &package, NULL, NULL)) {
		return 1;
	}

	/* Check click to find out where the files are */
	gchar * cmdline = g_strdup_printf("click pkgdir \"%s\"", package);
	g_free(package);

	gchar * output = NULL;
	g_spawn_command_line_sync(cmdline, &output, NULL, NULL, &error);
	g_free(cmdline);

	/* If we have an extra newline, we can delete it. */
	gchar * newline = g_strstr_len(output, -1, "\n");
	if (newline != NULL) {
		newline[0] = '\0';
	}

	if (error != NULL) {
		g_warning("Unable to get the package directory from click: %s", error->message);
		g_error_free(error);
		g_free(output); /* Probably not set, but just in case */
		return 1;
	}

	if (!g_file_test(output, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		g_warning("Application directory '%s' doesn't exist", output);
		g_free(output);
		return 1;
	}

	g_debug("Setting 'APP_DIR' to '%s'", output);
	set_upstart_variable("APP_DIR", output);

	gchar * desktopfile = manifest_to_desktop(output, argv[1]);
	g_free(output);
	if (desktopfile == NULL) {
		g_warning("Desktop file unable to be found");
		return 1;
	}

	GKeyFile * keyfile = g_key_file_new();

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

	gchar * parsedexec = desktop_exec_parse(exec, argc == 3 ? argv[2] : NULL);

	g_debug("Setting 'APP_EXEC' to '%s'", parsedexec);
	set_upstart_variable("APP_EXEC", parsedexec);

	g_free(parsedexec);
	g_free(exec);
	g_key_file_unref(keyfile);
	g_free(desktopfile);

	return 0;
}
