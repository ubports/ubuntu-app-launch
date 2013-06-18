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
#include <string.h>
#include <glib.h>

/* Try to find a desktop file in a particular data directory */
GKeyFile *
try_dir (const char * dir, const gchar * desktop)
{
	gchar * fullpath = g_build_filename(dir, "applications", desktop, NULL);
	GKeyFile * keyfile = g_key_file_new();

	/* NOTE: Leaving off the error here as we'll get a bunch of them,
	   so individuals aren't really useful */
	gboolean loaded = g_key_file_load_from_file(keyfile, fullpath, G_KEY_FILE_NONE, NULL);

	g_free(fullpath);

	if (loaded) {
		return keyfile;
	}

	g_key_file_free(keyfile);
	return NULL;
}

/* Check to make sure we have the sections and keys we want */
GKeyFile *
verify_keyfile (GKeyFile * inkeyfile, const gchar * desktop)
{
	if (inkeyfile == NULL) return NULL;

	gboolean passed = TRUE;

	if (passed && !g_key_file_has_group(inkeyfile, "Desktop Entry")) {
		passed = FALSE;
	}

	if (passed && !g_key_file_has_key(inkeyfile, "Desktop Entry", "Exec", NULL)) {
		passed = FALSE;
	}

	if (passed) {
		return inkeyfile;
	}

	g_debug("Desktop file '%s' is malformed", desktop);
	g_key_file_free(inkeyfile);
	return NULL;
}

int
main (int argc, char * argv[])
{
	if (argc != 2) {
		g_error("Should be called as: %s <app_id>", argv[0]);
		return 1;
	}

	gchar * desktop = g_strdup_printf("%s.desktop", argv[1]);

	const char * const * data_dirs = g_get_system_data_dirs();
	GKeyFile * keyfile = NULL;
	int i;

	keyfile = try_dir(g_get_user_data_dir(), desktop);
	keyfile = verify_keyfile(keyfile, desktop);

	for (i = 0; data_dirs[i] != NULL && keyfile == NULL; i++) {
		keyfile = try_dir(data_dirs[i], desktop);
		keyfile = verify_keyfile(keyfile, desktop);
	}

	if (keyfile == NULL) {
		g_error("Unable to find keyfile for application '%s'", argv[0]);
		return 1;
	}

	gchar * execline = g_key_file_get_string(keyfile, "Desktop Entry", "Exec", NULL);
	g_return_val_if_fail(execline != NULL, 1);

	/* TODO: keeping this simple for now */
	gchar * first = strstr(execline, " ");
	if (first != NULL) {
		first[0] = '\0';
	}

	g_print("%s\n", execline);

	g_key_file_free(keyfile);
	g_free(desktop);
	g_free(execline);

	return 0;
}
