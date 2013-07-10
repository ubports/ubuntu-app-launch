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
#include <gio/gio.h>

gboolean verify_keyfile (GKeyFile * inkeyfile, const gchar * desktop);

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

	if (!loaded) {
		g_key_file_free(keyfile);
		return NULL;
	}

	if (!verify_keyfile(keyfile, desktop)) {
		g_key_file_free(keyfile);
		return NULL;
	}

	return keyfile;
}

/* Check to make sure we have the sections and keys we want */
gboolean
verify_keyfile (GKeyFile * inkeyfile, const gchar * desktop)
{
	if (inkeyfile == NULL) return FALSE;

	if (!g_key_file_has_group(inkeyfile, "Desktop Entry")) {
		g_warning("Desktop file '%s' is missing the 'Desktop Entry' group", desktop);
		return FALSE;
	}

	if (!g_key_file_has_key(inkeyfile, "Desktop Entry", "Exec", NULL)) {
		g_warning("Desktop file '%s' is missing the 'Exec' key", desktop);
		return FALSE;
	}

	return TRUE;
}

static gchar *
uri2file (const gchar * uri)
{
	GError * error = NULL;
	gchar * retval = g_filename_from_uri(uri, NULL, &error);

	if (error != NULL) {
		g_warning("Unable to resolve '%s' to a filename: %s", uri, error->message);
		g_error_free(error);
	}

	if (retval == NULL) {
		retval = g_strdup("");
	}

	g_debug("Converting URI '%s' to file '%s'", uri, retval);
	return retval;
}

static void
free_string (gpointer value)
{
	gchar ** str = (gchar **)value;
	g_free(*str);
	return;
}

static gchar *
build_file_list (const gchar * uri_list)
{
	gchar ** uri_split = g_strsplit(uri_list, " ", 0);

	GArray * outarray = g_array_new(TRUE, FALSE, sizeof(gchar *));
	g_array_set_clear_func(outarray, free_string);

	int i;
	for (i = 0; uri_split[i] != NULL; i++) {
		gchar * path = uri2file(uri_split[i]);
		g_array_append_val(outarray, path);
	}

	gchar * filelist = g_strjoinv(" ", (gchar **)outarray->data);
	g_array_free(outarray, TRUE);

	g_strfreev(uri_split);

	return filelist;
}

/* Make sure we have the single URI variable */
static inline void
ensure_singleuri (gchar ** single_uri, const gchar * uri_list)
{
	if (uri_list == NULL) {
		return;
	}

	if (*single_uri != NULL) {
		return;
	}

	*single_uri = g_strdup(uri_list);
	g_utf8_strchr(*single_uri, -1, ' ')[0] = '\0';

	return;
}

/* Make sure we have a single file variable */
static inline void
ensure_singlefile (gchar ** single_file, gchar ** single_uri, const gchar * uri_list)
{
	if (uri_list == NULL) {
		return;
	}

	if (*single_file != NULL) {
		return;
	}

	ensure_singleuri(single_uri, uri_list);

	if (single_uri != NULL) {
		*single_file = uri2file(*single_uri);
	}

	return;
}


static gchar *
handle_codes (const gchar * execline, const gchar * uri_list)
{
	gchar ** execsplit = g_strsplit(execline, "%", 0);

	/* If we didn't have any codes, just exit here */
	if (execsplit[1] == NULL) {
		g_strfreev(execsplit);
		return g_strdup(execline);
	}

	int i;
	gchar * single_uri = NULL;
	gchar * single_file = NULL;
	gchar * file_list = NULL;
	GArray * outarray = g_array_new(TRUE, FALSE, sizeof(const gchar *));
	g_array_append_val(outarray, execsplit[0]);

	for (i = 1; execsplit[i] != NULL; i++) {
		const gchar * skipchar = &(execsplit[i][1]);

		switch (execsplit[i][0]) {
		case '\0': {
			const gchar * percent = "%";
			g_array_append_val(outarray, percent); /* %% is the literal */
			break;
		}
		case 'd':
		case 'D':
		case 'n':
		case 'N':
		case 'v':
		case 'm':
			/* Deprecated */
			g_array_append_val(outarray, skipchar);
			break;
		case 'f':
			ensure_singlefile(&single_file, &single_uri, uri_list);

			if (single_file != NULL) {
				g_array_append_val(outarray, single_file);
			}

			g_array_append_val(outarray, skipchar);
			break;
		case 'F':
			if (uri_list != NULL) {
				if (file_list == NULL) {
					file_list = build_file_list(uri_list);
				}
				g_array_append_val(outarray, file_list);
			}

			g_array_append_val(outarray, skipchar);
			break;
		case 'i':
		case 'c':
		case 'k':
			/* Perhaps?  Not sure anyone uses these */
			g_array_append_val(outarray, skipchar);
			break;
		case 'U':
			if (uri_list != NULL) {
				g_array_append_val(outarray, uri_list);
			}
			g_array_append_val(outarray, skipchar);
			break;
		case 'u':
			ensure_singleuri(&single_uri, uri_list);

			if (single_uri != NULL) {
				g_array_append_val(outarray, single_uri);
			}

			g_array_append_val(outarray, skipchar);
			break;
		default:
			g_warning("Desktop Exec line code '%%%c' unknown, skipping.", execsplit[i][0]);
			g_array_append_val(outarray, skipchar);
			break;
		}
	}

	gchar * output = g_strjoinv(" ", (gchar **)outarray->data);
	g_array_free(outarray, TRUE);

	g_free(single_uri);
	g_free(single_file);
	g_free(file_list);
	g_strfreev(execsplit);

	return output;
}

int
main (int argc, char * argv[])
{
	if (argc != 2 && argc != 3) {
		g_error("Should be called as: %s <app_id> [uri list]", argv[0]);
		return 1;
	}

	gchar * desktop = g_strdup_printf("%s.desktop", argv[1]);

	const char * const * data_dirs = g_get_system_data_dirs();
	GKeyFile * keyfile = NULL;
	int i;

	keyfile = try_dir(g_get_user_data_dir(), desktop);

	for (i = 0; data_dirs[i] != NULL && keyfile == NULL; i++) {
		keyfile = try_dir(data_dirs[i], desktop);
	}

	if (keyfile == NULL) {
		g_error("Unable to find keyfile for application '%s'", argv[0]);
		return 1;
	}

	gchar * execline = g_key_file_get_string(keyfile, "Desktop Entry", "Exec", NULL);
	g_return_val_if_fail(execline != NULL, 1);

	gchar * codeexec = handle_codes(execline, argc == 3 ? argv[2] : NULL);
	if (codeexec != NULL) {
		g_free(execline);
		execline = codeexec;
	}

	gchar * apparmor = g_key_file_get_string(keyfile, "Desktop Entry", "XCanonicalAppArmorProfile", NULL);
	if (apparmor != NULL) {
		gchar * execnew = g_strdup_printf("aa-exec -p \"%s\" -- %s", apparmor, execline);
		g_free(execline);
		execline = execnew;
	}

	g_print("%s\n", execline);

	g_key_file_free(keyfile);
	g_free(desktop);
	g_free(execline);

	return 0;
}
