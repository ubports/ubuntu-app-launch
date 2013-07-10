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

	return filelist;
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
			g_array_append_val(outarray, percent); /* %% is the litteral */
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
			if (uri_list != NULL) {
				if (single_file == NULL) {
					if (g_utf8_strchr(uri_list, -1, ' ') != NULL) {
						if (single_uri == NULL) {
							single_uri = g_strdup(uri_list);
							g_utf8_strchr(single_uri, -1, ' ')[0] = '\0';
						}
						single_file = uri2file(single_uri);
					} else {
						single_file = uri2file(uri_list);
					}
				}

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
			g_array_append_val(outarray, uri_list);
			g_array_append_val(outarray, skipchar);
			break;
		case 'u':
			if (uri_list != NULL) {
				if (g_utf8_strchr(uri_list, -1, ' ') == NULL) {
					/* There's only one, so we're good, just add it */
					g_array_append_val(outarray, uri_list);
					g_array_append_val(outarray, skipchar);
				} else {
					if (single_uri == NULL) {
						single_uri = g_strdup(uri_list);
						g_utf8_strchr(single_uri, -1, ' ')[0] = '\0';
					}

					g_array_append_val(outarray, single_uri);
				}
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

	return output;
}

/* Set an environment variable in Upstart */
static void
set_variable (const gchar * variable, const gchar * value)
{
	GError * error = NULL;
	gchar * command[4] = {
		"initctl",
		"set-env",
		NULL,
		NULL
	};

	gchar * variablestr = g_strdup_printf("%s=%s", variable, value);
	command[2] = variablestr;

	g_spawn_sync(NULL, /* working directory */
		command,
		NULL, /* environment */
		G_SPAWN_SEARCH_PATH,
		NULL, NULL, /* child setup */
		NULL, /* stdout */
		NULL, /* stderr */
		NULL, /* exit status */
		&error);

	if (error != NULL) {
		g_warning("Unable to set variable '%s' to '%s': %s", variable, value, error->message);
		g_error_free(error);
	}

	g_free(variablestr);
	return;
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

	gchar * codeexec = handle_codes(execline, argc == 3 ? argv[2] : NULL);
	if (codeexec != NULL) {
		g_free(execline);
		execline = codeexec;
	}

	gchar * apparmor = g_key_file_get_string(keyfile, "Desktop Entry", "XCanonicalAppArmorProfile", NULL);
	if (apparmor != NULL) {
		set_variable("APP_EXEC_POLICY", apparmor);
	}

	set_variable("APP_EXEC", execline);

	g_key_file_free(keyfile);
	g_free(desktop);
	g_free(execline);

	return 0;
}
