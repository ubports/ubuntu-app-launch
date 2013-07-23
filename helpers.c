
#include <json-glib/json-glib.h>
#include "helpers.h"

/* Take an app ID and validate it and then break it up
   and spit it out.  These are newly allocated strings */
gboolean
app_id_to_triplet (const gchar * app_id, gchar ** package, gchar ** application, gchar ** version)
{
	/* 'Parse' the App ID */
	gchar ** app_id_segments = g_strsplit(app_id, "_", 4);
	if (g_strv_length(app_id_segments) != 3) {
		g_warning("Unable to parse Application ID: %s", app_id);
		g_strfreev(app_id_segments);
		return FALSE;
	}

	if (package != NULL) {
		*package = app_id_segments[0];
	} else {
		g_free(app_id_segments[0]);
	}

	if (application != NULL) {
		*application = app_id_segments[1];
	} else {
		g_free(app_id_segments[1]);
	}

	if (version != NULL) {
		*version = app_id_segments[2];
	} else {
		g_free(app_id_segments[2]);
	}

	g_free(app_id_segments);
	return TRUE;
}

/* Take a manifest, parse it, find the application and
   and then return the path to the desktop file */
gchar *
manifest_to_desktop (const gchar * app_dir, const gchar * app_id)
{
	gchar * package = NULL;
	gchar * application = NULL;
	gchar * version = NULL;
	JsonParser * parser = NULL;
	GError * error = NULL;
	gchar * desktoppath = NULL;

	if (!app_id_to_triplet(app_id, &package, &application, &version)) {
		return NULL;
	}

	gchar * manifestfile = g_strdup_printf("%s.manifest", package);
	gchar * manifestpath = g_build_filename(app_dir, app_id, ".click", "info", manifestfile, NULL);
	g_free(manifestfile);

	if (!g_file_test(manifestpath, G_FILE_TEST_EXISTS)) {
		g_warning("Unable to find manifest file: %s", manifestpath);
		goto manifest_out;
	}

	parser = json_parser_new();

	json_parser_load_from_file(parser, manifestpath, &error);
	if (error != NULL) {
		g_warning("Unable to load manifest file '%s': %s", manifestpath, error->message);
		g_error_free(error);
		goto manifest_out;
	}

	JsonNode * root = json_parser_get_root(parser);
	if (json_node_get_node_type(root) != JSON_NODE_OBJECT) {
		g_warning("Manifest '%s' doesn't start with an object", manifestpath);
		goto manifest_out;
	}

	JsonObject * rootobj = json_node_get_object(root);
	if (!json_object_has_member(rootobj, "version")) {
		g_warning("Manifest '%s' doesn't have a version", manifestpath);
		goto manifest_out;
	}

	if (g_strcmp0(json_object_get_string_member(rootobj, "version"), version) != 0) {
		g_warning("Manifest '%s' version '%s' doesn't match AppID version '%s'", manifestpath, json_object_get_string_member(rootobj, "version"), version);
		goto manifest_out;
	}

	if (!json_object_has_member(rootobj, "applications")) {
		g_warning("Manifest '%s' doesn't have an applications section", manifestpath);
		goto manifest_out;
	}

	JsonObject * appsobj = json_object_get_object_member(rootobj, "applications");
	if (appsobj == NULL) {
		g_warning("Manifest '%s' has an applications section that is not a JSON object", manifestpath);
		goto manifest_out;
	}

	if (!json_object_has_member(appsobj, application)) {
		g_warning("Manifest '%s' doesn't have the application '%s' defined", manifestpath, application);
		goto manifest_out;
	}

	JsonObject * appobj = json_object_get_object_member(appsobj, application);
	if (appobj == NULL) {
		g_warning("Manifest '%s' has a definition for application '%s' that is not an object", manifestpath, application);
		goto manifest_out;
	}

	if (json_object_has_member(appobj, "type") && g_strcmp0(json_object_get_string_member(appobj, "type"), "desktop") != 0) {
		g_warning("Manifest '%s' has a definition for application '%s' who's type is not 'desktop'", manifestpath, application);
		goto manifest_out;
	}

	gchar * filename = NULL;
	if (json_object_has_member(appobj, "file")) {
		filename = g_strdup(json_object_get_string_member(appobj, "file"));
	} else {
		filename = g_strdup_printf("%s.desktop", application);
	}

	desktoppath = g_build_filename(app_dir, app_id, filename, NULL);
	g_free(filename);

	if (!g_file_test(desktoppath, G_FILE_TEST_EXISTS)) {
		g_warning("Application desktop file '%s' doesn't exist", desktoppath);
		g_free(desktoppath);
		desktoppath = NULL;
	}

manifest_out:
	g_clear_object(&parser);
	g_free(manifestpath);
	g_free(package);
	g_free(application);
	g_free(version);

	return desktoppath;
}

/* Take a desktop file, make sure that it makes sense and
   then return the exec line */
gchar *
desktop_to_exec (GKeyFile * desktop_file)
{

	return NULL;
}

