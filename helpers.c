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

#include <json-glib/json-glib.h>
#include <upstart.h>
#include "helpers.h"

/* Take an app ID and validate it and then break it up
   and spit it out.  These are newly allocated strings */
gboolean
app_id_to_triplet (const gchar * app_id, gchar ** package, gchar ** application, gchar ** version)
{
	/* 'Parse' the App ID */
	gchar ** app_id_segments = g_strsplit(app_id, "_", 4);
	if (g_strv_length(app_id_segments) != 3) {
		g_debug("Unable to parse Application ID: %s", app_id);
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
	gchar * output = NULL;
	gchar * application = NULL;
	gchar * version = NULL;
	JsonParser * parser = NULL;
	GError * error = NULL;
	gchar * desktoppath = NULL;

	if (!app_id_to_triplet(app_id, &package, &application, &version)) {
		g_warning("Unable to parse triplet: %s", app_id);
		return NULL;
	}

	gchar * cmdline = g_strdup_printf("click info \"%s\"", package);
	g_spawn_command_line_sync(cmdline, &output, NULL, NULL, &error);
	g_free(cmdline);

	if (error != NULL) {
		g_warning("Unable to get manifest for '%s': %s", package, error->message);
		g_error_free(error);
		goto manifest_out;
	}

	parser = json_parser_new();

	json_parser_load_from_data(parser, output, -1, &error);
	g_free(output);

	if (error != NULL) {
		g_warning("Unable to load manifest data '%s': %s", package, error->message);
		g_error_free(error);
		goto manifest_out;
	}

	JsonNode * root = json_parser_get_root(parser);
	if (json_node_get_node_type(root) != JSON_NODE_OBJECT) {
		g_warning("Manifest '%s' doesn't start with an object", package);
		goto manifest_out;
	}

	JsonObject * rootobj = json_node_get_object(root);
	if (!json_object_has_member(rootobj, "version")) {
		g_warning("Manifest '%s' doesn't have a version", package);
		goto manifest_out;
	}

	if (g_strcmp0(json_object_get_string_member(rootobj, "version"), version) != 0) {
		g_warning("Manifest '%s' version '%s' doesn't match AppID version '%s'", package, json_object_get_string_member(rootobj, "version"), version);
		goto manifest_out;
	}

	if (!json_object_has_member(rootobj, "hooks")) {
		g_warning("Manifest '%s' doesn't have an hooks section", package);
		goto manifest_out;
	}

	JsonObject * appsobj = json_object_get_object_member(rootobj, "hooks");
	if (appsobj == NULL) {
		g_warning("Manifest '%s' has an hooks section that is not a JSON object", package);
		goto manifest_out;
	}

	if (!json_object_has_member(appsobj, application)) {
		g_warning("Manifest '%s' doesn't have the application '%s' defined", package, application);
		goto manifest_out;
	}

	JsonObject * appobj = json_object_get_object_member(appsobj, application);
	if (appobj == NULL) {
		g_warning("Manifest '%s' has a definition for application '%s' that is not an object", package, application);
		goto manifest_out;
	}

	gchar * filename = NULL;
	if (json_object_has_member(appobj, "desktop")) {
		filename = g_strdup(json_object_get_string_member(appobj, "desktop"));
	} else {
		filename = g_strdup_printf("%s.desktop", application);
	}

	desktoppath = g_build_filename(app_dir, filename, NULL);
	g_free(filename);

	if (!g_file_test(desktoppath, G_FILE_TEST_EXISTS)) {
		g_warning("Application desktop file '%s' doesn't exist", desktoppath);
		g_free(desktoppath);
		desktoppath = NULL;
	}

manifest_out:
	g_clear_object(&parser);
	g_free(package);
	g_free(application);
	g_free(version);

	return desktoppath;
}

/* Take a desktop file, make sure that it makes sense and
   then return the exec line */
gchar *
desktop_to_exec (GKeyFile * desktop_file, const gchar * from)
{
	GError * error = NULL;

	if (!g_key_file_has_group(desktop_file, "Desktop Entry")) {
		g_warning("Desktop file '%s' does not have a 'Desktop Entry' group", from);
		return NULL;
	}

	gchar * type = g_key_file_get_string(desktop_file, "Desktop Entry", "Type", &error);
	if (error != NULL) {
		g_warning("Desktop file '%s' unable to get type: %s", from, error->message);
		g_error_free(error);
		g_free(type);
		return NULL;
	}

	if (g_strcmp0(type, "Application") != 0) {
		g_warning("Desktop file '%s' has a type of '%s' instead of 'Application'", from, type);
		g_free(type);
		return NULL;
	}
	g_free(type);

	if (g_key_file_has_key(desktop_file, "Desktop Entry", "NoDisplay", NULL)) {
		gboolean nodisplay = g_key_file_get_boolean(desktop_file, "Desktop Entry", "NoDisplay", NULL);
		if (nodisplay) {
			g_warning("Desktop file '%s' is set to not display, not copying", from);
			return NULL;
		}
	}

	if (g_key_file_has_key(desktop_file, "Desktop Entry", "Hidden", NULL)) {
		gboolean hidden = g_key_file_get_boolean(desktop_file, "Desktop Entry", "Hidden", NULL);
		if (hidden) {
			g_warning("Desktop file '%s' is set to be hidden, not copying", from);
			return NULL;
		}
	}

	if (g_key_file_has_key(desktop_file, "Desktop Entry", "Terminal", NULL)) {
		gboolean terminal = g_key_file_get_boolean(desktop_file, "Desktop Entry", "Terminal", NULL);
		if (terminal) {
			g_warning("Desktop file '%s' is set to run in a terminal, not copying", from);
			return NULL;
		}
	}

	if (!g_key_file_has_key(desktop_file, "Desktop Entry", "Exec", NULL)) {
		g_warning("Desktop file '%s' has no 'Exec' key", from);
		return NULL;
	}

	gchar * exec = g_key_file_get_string(desktop_file, "Desktop Entry", "Exec", NULL);
	return exec;
}

/* Sets an upstart variable, currently using initctl */
void
set_upstart_variable (const gchar * variable, const gchar * value)
{
	/* Check to see if we can get the job environment */
	const gchar * job_name = g_getenv("UPSTART_JOB");
	const gchar * instance_name = g_getenv("UPSTART_INSTANCE");
	g_return_if_fail(job_name != NULL);

	/* Get a bus, let's go! */
	GDBusConnection * bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	g_return_if_fail(bus != NULL);

	GVariantBuilder builder; /* Target: (assb) */
	g_variant_builder_init(&builder, G_VARIANT_TYPE_TUPLE);

	/* Setup the job properties */
	g_variant_builder_open(&builder, G_VARIANT_TYPE_ARRAY);
	g_variant_builder_add_value(&builder, g_variant_new_string(job_name));
	if (instance_name != NULL)
		g_variant_builder_add_value(&builder, g_variant_new_string(instance_name));
	g_variant_builder_close(&builder);

	/* The value itself */
	gchar * envstr = g_strdup_printf("%s=%s", variable, value);
	g_variant_builder_add_value(&builder, g_variant_new_take_string(envstr));

	/* Do we want to replace?  Yes, we do! */
	g_variant_builder_add_value(&builder, g_variant_new_boolean(TRUE));

	g_dbus_connection_call(bus,
		DBUS_SERVICE_UPSTART,
		DBUS_PATH_UPSTART,
		DBUS_INTERFACE_UPSTART,
		"SetEnv",
		g_variant_builder_end(&builder),
		NULL, /* reply */
		G_DBUS_CALL_FLAGS_NONE,
		-1, /* timeout */
		NULL, /* cancelable */
		NULL, NULL); /* callback */

	g_object_unref(bus);
}

/* Convert a URI into a file */
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

/* Put the list of files into the argument array */
static inline void
file_list_handling (GArray * outarray, gchar ** list, gchar * (*dup_func) (const gchar * in))
{
	/* No URLs, cool, this is a noop */
	if (list == NULL || list[0] == NULL) {
		return;
	}

	int i;
	for (i = 0; list[i] != NULL; i++) {
		gchar * entry = dup_func(list[i]);

		/* No NULLs */
		if (entry != NULL && entry[0] != '\0') {
			g_array_append_val(outarray, entry);
		} else {
			g_free(entry);
		}
	}
}

/* Parse a desktop exec line and return the next string */
static void
desktop_exec_segment_parse (GArray * finalarray, const gchar * execsegment, gchar ** uri_list)
{
	/* No NULL strings */
	if (execsegment == NULL || execsegment[0] == '\0')
		return;

	/* Handle %F and %U as an argument on their own as per the spec */
	if (g_strcmp0(execsegment, "%U") == 0) {
		return file_list_handling(finalarray, uri_list, g_strdup);
	}
	if (g_strcmp0(execsegment, "%F") == 0) {
		return file_list_handling(finalarray, uri_list, uri2file);
	}

	/* Start looking at individual codes */
	gchar ** execsplit = g_strsplit(execsegment, "%", 0);

	/* If we didn't have any codes, just exit here */
	if (execsplit[1] == NULL) {
		g_strfreev(execsplit);
		gchar * dup = g_strdup(execsegment);
		g_array_append_val(finalarray, dup);
		return;
	}

	int i;

	gboolean previous_percent = FALSE;
	GArray * outarray = g_array_new(TRUE, FALSE, sizeof(const gchar *));
	g_array_append_val(outarray, execsplit[0]);
	gchar * single_file = NULL;

	/* The variables allowed in an exec line from the Freedesktop.org Desktop
	   File specification: http://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#exec-variables */
	for (i = 1; execsplit[i] != NULL; i++) {
		const gchar * skipchar = &(execsplit[i][1]);

		/* Handle the case of %%F printing "%F" */
		if (previous_percent) {
			g_array_append_val(outarray, execsplit[i]);
			previous_percent = FALSE;
			continue;
		}

		switch (execsplit[i][0]) {
		case '\0': {
			const gchar * percent = "%";
			g_array_append_val(outarray, percent); /* %% is the literal */
			previous_percent = TRUE;
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
			if (uri_list != NULL && uri_list[0] != NULL) {
				if (single_file == NULL)
					single_file = uri2file(uri_list[0]);
				g_array_append_val(outarray, single_file);
			}

			g_array_append_val(outarray, skipchar);
			break;
		case 'F':
			g_warning("Exec line segment has a '%%F' that isn't its own argument '%s', ignoring.", execsegment);
			g_array_append_val(outarray, skipchar);
			break;
		case 'i':
		case 'c':
		case 'k':
			/* Perhaps?  Not sure anyone uses these */
			g_array_append_val(outarray, skipchar);
			break;
		case 'U':
			g_warning("Exec line segment has a '%%U' that isn't its own argument '%s', ignoring.", execsegment);
			g_array_append_val(outarray, skipchar);
			break;
		case 'u':
			if (uri_list != NULL && uri_list[0] != NULL) {
				g_array_append_val(outarray, uri_list[0]);
			}

			g_array_append_val(outarray, skipchar);
			break;
		default:
			g_warning("Desktop Exec line code '%%%c' unknown, skipping.", execsplit[i][0]);
			g_array_append_val(outarray, skipchar);
			break;
		}
	}

	gchar * output = g_strjoinv(NULL, (gchar **)outarray->data);
	g_array_free(outarray, TRUE);

	if (output != NULL && output[0] != '\0') {
		g_array_append_val(finalarray, output);
	} else {
		g_free(output);
	}

	g_free(single_file);
	g_strfreev(execsplit);
}

/* Take a full exec line, split it out, parse the segments and return
   it to the caller */
GArray *
desktop_exec_parse (const gchar * execline, const gchar * urilist)
{
	GError * error = NULL;
	gchar ** splitexec = NULL;
	gchar ** splituris = NULL;
	gint execitems = 0;

	/* This returns from desktop file style quoting to straight strings with
	   the appropriate characters split by the spaces that were meant for
	   splitting.  Trickier than it sounds.  But now we should be able to assume
	   that each string in the array is expected to be its own parameter. */
	g_shell_parse_argv(execline, &execitems, &splitexec, &error);

	if (error != NULL) {
		g_warning("Unable to parse exec line '%s': %s", execline, error->message);
		g_error_free(error);
		return NULL;
	}

	if (urilist != NULL && urilist[0] != '\0') {
		g_shell_parse_argv(urilist, NULL, &splituris, &error);

		if (error != NULL) {
			g_warning("Unable to parse URIs '%s': %s", urilist, error->message);
			g_error_free(error);
			/* Continuing without URIs */
			splituris = NULL;
		}
	}


	GArray * newargv = g_array_new(TRUE, FALSE, sizeof(gchar *));
	int i;
	for (i = 0; i < execitems; i++) {
		desktop_exec_segment_parse(newargv, splitexec[i], splituris);
	}
	g_strfreev(splitexec);

	if (splituris != NULL) {
		g_strfreev(splituris);
	}

	/* Each string here should be its own param */

	return newargv;
}

/* Check to make sure we have the sections and keys we want */
static gboolean
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

/* Try to find a desktop file in a particular data directory */
static GKeyFile *
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

/* Find the keyfile that we need for a particular AppID and return it.
   Or NULL if we can't find it. */
GKeyFile *
keyfile_for_appid (const gchar * appid, gchar ** desktopfile)
{
	gchar * desktop = g_strdup_printf("%s.desktop", appid);

	const char * const * data_dirs = g_get_system_data_dirs();
	GKeyFile * keyfile = NULL;
	int i;

	keyfile = try_dir(g_get_user_data_dir(), desktop);
	if (keyfile != NULL && desktopfile != NULL && *desktopfile == NULL) {
		*desktopfile = g_build_filename(g_get_user_data_dir(), "applications", desktop, NULL);
	}

	for (i = 0; data_dirs[i] != NULL && keyfile == NULL; i++) {
		keyfile = try_dir(data_dirs[i], desktop);

		if (keyfile != NULL && desktopfile != NULL && *desktopfile == NULL) {
			*desktopfile = g_build_filename(data_dirs[i], "applications", desktop, NULL);
		}
	}

	g_free(desktop);

	return keyfile;
}

/* Set environment various variables to make apps work under
 * confinement according to:
 * https://wiki.ubuntu.com/SecurityTeam/Specifications/ApplicationConfinement
 */
void
set_confined_envvars (const gchar * package, const gchar * app_dir)
{
	g_return_if_fail(package != NULL);
	g_return_if_fail(app_dir != NULL);

	g_debug("Setting 'UBUNTU_APPLICATION_ISOLATION' to '1'");
	set_upstart_variable("UBUNTU_APPLICATION_ISOLATION", "1");

	/* Make sure the XDG base dirs are set for the application using
	 * the user's current values/system defaults. We could set these to
	 * what is expected in the AppArmor profile, but that might be too
	 * brittle if someone uses different base dirs.
	 */
	g_debug("Setting 'XDG_CACHE_HOME' using g_get_user_cache_dir()");
	set_upstart_variable("XDG_CACHE_HOME", g_get_user_cache_dir());

	g_debug("Setting 'XDG_CONFIG_HOME' using g_get_user_config_dir()");
	set_upstart_variable("XDG_CONFIG_HOME", g_get_user_config_dir());

	g_debug("Setting 'XDG_DATA_HOME' using g_get_user_data_dir()");
	set_upstart_variable("XDG_DATA_HOME", g_get_user_data_dir());

	g_debug("Setting 'XDG_RUNTIME_DIR' using g_get_user_runtime_dir()");
	set_upstart_variable("XDG_RUNTIME_DIR", g_get_user_runtime_dir());

	/* Add the application's dir to the list of sources for data */
	gchar * datadirs = g_strjoin(":", app_dir, g_getenv("XDG_DATA_DIRS"), NULL);
	set_upstart_variable("XDG_DATA_DIRS", datadirs);
	g_free(datadirs);

	/* Set TMPDIR to something sane and application-specific */
	gchar * tmpdir = g_strdup_printf("%s/confined/%s", g_get_user_runtime_dir(), package);
	g_debug("Setting 'TMPDIR' to '%s'", tmpdir);
	set_upstart_variable("TMPDIR", tmpdir);
	g_debug("Creating '%s'", tmpdir);
	g_mkdir_with_parents(tmpdir, 0700);
	g_free(tmpdir);

	/* Do the same for nvidia */
	gchar * nv_shader_cachedir = g_strdup_printf("%s/%s", g_get_user_cache_dir(), package);
	g_debug("Setting '__GL_SHADER_DISK_CACHE_PATH' to '%s'", nv_shader_cachedir);
	set_upstart_variable("__GL_SHADER_DISK_CACHE_PATH", nv_shader_cachedir);
	g_free(nv_shader_cachedir);

	return;
}

static void
unity_signal_cb (GDBusConnection * con, const gchar * sender, const gchar * path, const gchar * interface, const gchar * signal, GVariant * params, gpointer user_data)
{
	GMainLoop * mainloop = (GMainLoop *)user_data;
	g_main_loop_quit(mainloop);
}

struct _handshake_t {
	GDBusConnection * con;
	GMainLoop * mainloop;
	guint signal_subscribe;
	guint timeout;
};

static gboolean
unity_too_slow_cb (gpointer user_data)
{
	handshake_t * handshake = (handshake_t *)user_data;
	g_main_loop_quit(handshake->mainloop);
	handshake->timeout = 0;
	return G_SOURCE_REMOVE;
}

handshake_t *
starting_handshake_start (const gchar *   app_id)
{
	GError * error = NULL;
	handshake_t * handshake = g_new0(handshake_t, 1);

	handshake->mainloop = g_main_loop_new(NULL, FALSE);
	handshake->con = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);

	if (error != NULL) {
		g_critical("Unable to connect to session bus: %s", error->message);
		g_error_free(error);
		g_free(handshake);
		return NULL;
	}

	/* Set up listening for the unfrozen signal from Unity */
	handshake->signal_subscribe = g_dbus_connection_signal_subscribe(handshake->con,
		NULL, /* sender */
		"com.canonical.UpstartAppLaunch", /* interface */
		"UnityStartingSignal", /* signal */
		"/", /* path */
		app_id, /* arg0 */
		G_DBUS_SIGNAL_FLAGS_NONE,
		unity_signal_cb, handshake->mainloop,
		NULL); /* user data destroy */

	/* Send unfreeze to to Unity */
	g_dbus_connection_emit_signal(handshake->con,
		NULL, /* destination */
		"/", /* path */
		"com.canonical.UpstartAppLaunch", /* interface */
		"UnityStartingBroadcast", /* signal */
		g_variant_new("(s)", app_id),
		&error);

	/* Really, Unity? */
	handshake->timeout = g_timeout_add_seconds(1, unity_too_slow_cb, handshake);

	return handshake;
}

void
starting_handshake_wait (handshake_t * handshake)
{
	if (handshake == NULL)
		return;

	g_main_loop_run(handshake->mainloop);

	if (handshake->timeout != 0)
		g_source_remove(handshake->timeout);
	g_main_loop_unref(handshake->mainloop);
	g_dbus_connection_signal_unsubscribe(handshake->con, handshake->signal_subscribe);
	g_object_unref(handshake->con);

	g_free(handshake);
}
