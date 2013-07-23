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
#include <string.h>
#include <json-glib/json-glib.h>

typedef struct _app_state_t app_state_t;
struct _app_state_t {
	gchar * app_id;
	gboolean has_click;
	gboolean has_desktop;
};

/* Find an entry in the app array */
app_state_t *
find_app_entry (const gchar * name, GArray * app_array)
{
	int i;
	for (i = 0; i < app_array->len; i++) {
		app_state_t * state = &g_array_index(app_array, app_state_t, i);

		if (g_strcmp0(state->app_id, name) == 0) {
			return state;
		}
	}

	app_state_t newstate;
	newstate.has_click = FALSE;
	newstate.has_desktop = FALSE;
	newstate.app_id = g_strdup(name);

	g_array_append_val(app_array, newstate);

	/* Note: The pointer needs to be the entry in the array, not the
	   one that we have on the stack.  Criticaly important. */
	app_state_t * statepntr = &g_array_index(app_array, app_state_t, app_array->len - 1);
	return statepntr;
}

/* Look at an click package entry */
void
add_click_package (const gchar * name, GArray * app_array)
{
	app_state_t * state = find_app_entry(name, app_array);
	state->has_click = TRUE;

	return;
}

/* Look at an desktop file entry */
void
add_desktop_file (const gchar * name, GArray * app_array)
{
	if (!g_str_has_suffix(name, ".desktop")) {
		return;
	}

	if (!g_str_has_prefix(name, "click-")) {
		return;
	}

	gchar * appid = g_strdup(name + strlen("click-"));
	g_strstr_len(appid, -1, ".desktop")[0] = '\0';

	app_state_t * state = find_app_entry(appid, app_array);
	state->has_desktop = TRUE;

	g_free(appid);
	return;
}

/* Open a directory and look at all the entries */
void
dir_for_each (const gchar * dirname, void(*func)(const gchar * name, GArray * app_array), GArray * app_array)
{
	GError * error = NULL;
	GDir * directory = g_dir_open(dirname, 0, &error);

	if (error != NULL) {
		g_warning("Unable to read directory '%s': %s", dirname, error->message);
		g_error_free(error);
		return;
	}

	const gchar * filename = NULL;
	while ((filename = g_dir_read_name(directory)) != NULL) {
		func(filename, app_array);
	}

	g_dir_close(directory);
	return;
}

/* Function to take the source Desktop file and build a new
   one with similar, but not the same data in it */
static void
copy_desktop_file (const gchar * from, const gchar * to, const gchar * appdir)
{

	return;
}

/* Parse the manifest file and start looking into it */
static void
parse_manifest_file (const gchar * manifestfile, const gchar * application_name, const gchar * version, const gchar * desktopfile, const gchar * application_dir)
{
	JsonParser * parser = json_parser_new();
	GError * error = NULL;

	json_parser_load_from_file(parser, manifestfile, &error);
	if (error != NULL) {
		g_warning("Unable to load manifest file '%s': %s", manifestfile, error->message);
		g_error_free(error);
		g_object_unref(parser);
		return;
	}

	JsonNode * root = json_parser_get_root(parser);
	if (json_node_get_node_type(root) != JSON_NODE_OBJECT) {
		g_warning("Manifest '%s' doesn't start with an object", manifestfile);
		g_object_unref(parser);
		return;
	}

	JsonObject * rootobj = json_node_get_object(root);
	if (!json_object_has_member(rootobj, "version")) {
		g_warning("Manifest '%s' doesn't have a version", manifestfile);
		g_object_unref(parser);
		return;
	}

	if (g_strcmp0(json_object_get_string_member(rootobj, "version"), version) != 0) {
		g_warning("Manifest '%s' version '%s' doesn't match AppID version '%s'", manifestfile, json_object_get_string_member(rootobj, "version"), version);
		g_object_unref(parser);
		return;
	}

	if (!json_object_has_member(rootobj, "applications")) {
		g_warning("Manifest '%s' doesn't have an applications section", manifestfile);
		g_object_unref(parser);
		return;
	}

	JsonObject * appsobj = json_object_get_object_member(rootobj, "applications");
	if (appsobj == NULL) {
		g_warning("Manifest '%s' has an applications section that is not a JSON object", manifestfile);
		g_object_unref(parser);
		return;
	}

	if (!json_object_has_member(appsobj, application_name)) {
		g_warning("Manifest '%s' doesn't have the application '%s' defined", manifestfile, application_name);
		g_object_unref(parser);
		return;
	}

	JsonObject * appobj = json_object_get_object_member(appsobj, application_name);
	if (appobj != NULL) {
		g_warning("Manifest '%s' has a definition for application '%s' that is not an object", manifestfile, application_name);
		g_object_unref(parser);
		return;
	}

	if (json_object_has_member(appobj, "type") && g_strcmp0(json_object_get_string_member(appobj, "type"), "desktop") != 0) {
		g_warning("Manifest '%s' has a definition for application '%s' who's type is not 'desktop'", manifestfile, application_name);
		g_object_unref(parser);
		return;
	}

	gchar * filename = NULL;
	if (json_object_has_member(appobj, "file")) {
		filename = g_strdup(json_object_get_string_member(appobj, "file"));
	} else {
		filename = g_strdup_printf("%s.desktop", application_name);
	}

	gchar * desktoppath = g_build_filename(application_dir, filename, NULL);
	g_free(filename);

	if (g_file_test(desktoppath, G_FILE_TEST_EXISTS)) {
		copy_desktop_file(desktoppath, desktopfile, application_dir);
	} else {
		g_warning("Application desktop file '%s' doesn't exist", desktoppath);
	}

	g_free(desktoppath);
	g_object_unref(parser);
	return;
}

/* Build a desktop file in the user's home directory */
static void
build_desktop_file (app_state_t * state, const gchar * symlinkdir, const gchar * desktopdir)
{
	/* 'Parse' the App ID */
	gchar ** app_id_segments = g_strsplit(state->app_id, "_", 4);
	if (g_strv_length(app_id_segments) != 3) {
		g_warning("Unable to parse Application ID: %s", state->app_id);
		g_strfreev(app_id_segments);
		return;
	}

	/* Break appart the parsed app id */
	const gchar * packageid = app_id_segments[0];
	const gchar * application = app_id_segments[1];
	const gchar * version = app_id_segments[2];

	/* Determine the manifest file name */
	gchar * manifestfile = g_strdup_printf("%s.manifest", packageid);
	gchar * manifestpath = g_build_filename(symlinkdir, ".click", "info", manifestfile, NULL);
	g_free(manifestfile);

	/* Determine the desktop file name */
	gchar * desktopfile = g_strdup_printf("click-%s.desktop", state->app_id);
	gchar * desktoppath = g_build_filename(desktopdir, desktopfile, NULL);
	g_free(desktopfile);

	/* Make sure the manifest exists */
	if (!g_file_test(manifestpath, G_FILE_TEST_EXISTS)) {
		g_warning("Unable to find manifest file: %s", manifestpath);
	} else {
		parse_manifest_file(manifestpath, application, version, desktoppath, symlinkdir);
	}

	g_free(desktoppath);
	g_free(manifestpath);
	g_strfreev(app_id_segments);
	return;
}

/* Remove the desktop file from the user's home directory */
static void
remove_desktop_file (app_state_t * state, const gchar * desktopdir)
{
	gchar * desktopfile = g_strdup_printf("click-%s.desktop", state->app_id);
	gchar * desktoppath = g_build_filename(desktopdir, desktopfile, NULL);
	g_free(desktopfile);

	if (g_unlink(desktoppath) != 0) {
		g_warning("Unable to delete desktop file: %s", desktoppath);
	}

	g_free(desktoppath);

	return;
}

/* The main function */
int
main (int argc, char * argv[])
{
	if (argc != 1) {
		g_error("Shouldn't have arguments");
		return 1;
	}

	GArray * apparray = g_array_new(FALSE, FALSE, sizeof(app_state_t));

	/* Find all the symlinks of apps */
	gchar * symlinkdir = g_build_filename(g_get_user_cache_dir(), "upstart-app-lauch", "desktop", NULL);
	if (!g_file_test(symlinkdir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		g_warning("No installed click packages");
	} else {
		dir_for_each(symlinkdir, add_click_package, apparray);
	}

	/* Find all the click desktop files */
	gchar * desktopdir = g_build_filename(g_get_user_data_dir(), "applications", NULL);
	if (!g_file_test(symlinkdir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		g_warning("No applications defined");
	} else {
		dir_for_each(desktopdir, add_desktop_file, apparray);
	}

	/* Process the merge */
	int i;
	for (i = 0; i < apparray->len; i++) {
		app_state_t * state = &g_array_index(apparray, app_state_t, i);
		g_debug("Processing App ID: %s", state->app_id);

		if (state->has_click && state->has_desktop) {
			g_debug("\tAlready synchronized");
		} else if (state->has_click) {
			g_debug("\tBuilding desktop file");
			build_desktop_file(state, symlinkdir, desktopdir);
		} else if (state->has_desktop) {
			g_debug("\tRemoving desktop file");
			remove_desktop_file(state, desktopdir);
		}

		g_free(state->app_id);
	}

	g_array_free(apparray, TRUE);
	g_free(desktopdir);
	g_free(symlinkdir);

	return 0;
}
