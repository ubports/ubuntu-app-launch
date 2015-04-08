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

/*

INTRODUCTION:

This is a hook for Click packages.  You can find information on Click package hooks in
the click documentation:

https://click.readthedocs.org/en/latest/

Probably the biggest thing to understand for how this code works is that you need to
understand that this hook is run after one, or many packages are installed.  A set of
symbolic links are made to the desktop files per-application (not per-package) in the
directory specified in ubuntu-app-launcher-desktop.click-hook.in.  Those desktop files
give us the App ID of the packages that are installed and have applications needing
desktop files in them.  We then operate on each of them ensuring that they are synchronized
with the desktop files in ~/.local/share/applications/.

The desktop files that we're creating there ARE NOT used for execution by the
ubuntu-app-launch Upstart jobs.  They are there so that Unity can know which applications
are installed for this user and they provide an Exec line to allow compatibility with
desktop environments that are not using ubuntu-app-launch for launching applications.
You should not modify them and expect any executing under Unity to change.

*/

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <click.h>
#include <string.h>
#include <errno.h>

#include "helpers.h"

typedef struct _app_state_t app_state_t;
struct _app_state_t {
	gchar * app_id;
	gboolean has_click;
	gboolean has_desktop;
	guint64 click_modified;
	guint64 desktop_modified;
};

/* Desktop Group */
#define DESKTOP_GROUP      "Desktop Entry"
/* Desktop Keys */
#define APP_ID_KEY         "X-Ubuntu-Application-ID"
#define PATH_KEY           "Path"
#define EXEC_KEY           "Exec"
#define ICON_KEY           "Icon"
#define SYMBOLIC_ICON_KEY  "X-Ubuntu-SymbolicIcon"
#define SOURCE_FILE_KEY    "X-Ubuntu-UAL-Source-Desktop"
/* Other */
#define OLD_KEY_PREFIX     "X-Ubuntu-Old-"

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
	newstate.click_modified = 0;
	newstate.desktop_modified = 0;
	newstate.app_id = g_strdup(name);

	g_array_append_val(app_array, newstate);

	/* Note: The pointer needs to be the entry in the array, not the
	   one that we have on the stack.  Criticaly important. */
	app_state_t * statepntr = &g_array_index(app_array, app_state_t, app_array->len - 1);
	return statepntr;
}

/* Looks up the file creation time, which seems harder with GLib
   than it should be */
guint64
modified_time (const gchar * dir, const gchar * filename)
{
	gchar * path = g_build_filename(dir, filename, NULL);
	GFile * file = g_file_new_for_path(path);
	GFileInfo * info = g_file_query_info(file, G_FILE_ATTRIBUTE_TIME_MODIFIED, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);

	guint64 time = g_file_info_get_attribute_uint64(info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

	g_object_unref(info);
	g_object_unref(file);
	g_free(path);

	return time;
}

/* Look at an click package entry */
void
add_click_package (const gchar * dir, const gchar * name, GArray * app_array)
{
	if (!g_str_has_suffix(name, ".desktop")) {
		return;
	}

	gchar * appid = g_strdup(name);
	g_strstr_len(appid, -1, ".desktop")[0] = '\0';

	app_state_t * state = find_app_entry(appid, app_array);
	state->has_click = TRUE;
	state->click_modified = modified_time(dir, name);

	g_free(appid);

	return;
}

/* Look at the desktop file and ensure that it was built by us, and if it
   was that its source still exists */
gboolean
desktop_source_exists (const gchar * dir, const gchar * name)
{
	gchar * desktopfile = g_build_filename(dir, name, NULL);

	GKeyFile * keyfile = g_key_file_new();
	g_key_file_load_from_file(keyfile,
		desktopfile,
		G_KEY_FILE_NONE,
		NULL); /* No error */

	if (!g_key_file_has_key(keyfile, DESKTOP_GROUP, SOURCE_FILE_KEY, NULL)) {
		g_free(desktopfile);
		g_key_file_free(keyfile);
		return FALSE;
	}

	/* At this point we know the key exists, so if we can't find the source
	   file we want to delete the file as well. We need to replace it. */
	gchar * originalfile = g_key_file_get_string(keyfile, DESKTOP_GROUP, SOURCE_FILE_KEY, NULL);
	g_key_file_free(keyfile);
	gboolean found = TRUE;

	if (!g_file_test(originalfile, G_FILE_TEST_EXISTS)) {
		g_remove(desktopfile);
		found = FALSE;
	}

	g_free(originalfile);
	g_free(desktopfile);

	return found;
}

/* Look at an desktop file entry */
void
add_desktop_file (const gchar * dir, const gchar * name, GArray * app_array)
{
	if (!g_str_has_suffix(name, ".desktop")) {
		return;
	}

	if (!desktop_source_exists(dir, name)) {
		return;
	}

	gchar * appid = g_strdup(name);
	g_strstr_len(appid, -1, ".desktop")[0] = '\0';

	/* We only want valid APP IDs as desktop files */
	if (!app_id_to_triplet(appid, NULL, NULL, NULL)) {
		g_free(appid);
		return;
	}

	app_state_t * state = find_app_entry(appid, app_array);
	state->has_desktop = TRUE;
	state->desktop_modified = modified_time(dir, name);

	g_free(appid);
	return;
}

/* Open a directory and look at all the entries */
void
dir_for_each (const gchar * dirname, void(*func)(const gchar * dir, const gchar * name, GArray * app_array), GArray * app_array)
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
		func(dirname, filename, app_array);
	}

	g_dir_close(directory);
	return;
}

/* Helpers to ensure we write nicely */
static void 
write_string (int          fd,
              const gchar *string)
{
	int res; 
	do
		res = write (fd, string, strlen (string));
	while (G_UNLIKELY (res == -1 && errno == EINTR));
}

/* Make NULLs fast and fun! */
static void 
write_null (int fd)
{
	int res; 
	do
		res = write (fd, "", 1);
	while (G_UNLIKELY (res == -1 && errno == EINTR));
}

/* Child watcher */
static gboolean
apport_child_watch (GPid pid, gint status, gpointer user_data)
{
	g_main_loop_quit((GMainLoop *)user_data);
	return FALSE;
}

static gboolean
apport_child_timeout (gpointer user_data)
{
	g_warning("Recoverable Error Reporter Timeout");
	g_main_loop_quit((GMainLoop *)user_data);
	return FALSE;
}


/* Code to report an error, so we can start tracking how important this is */
static void
report_recoverable_error (const gchar * app_id, const gchar * iconfield, const gchar * originalicon, const gchar * iconpath)
{
	GError * error = NULL;
	gint error_stdin = 0;
	GPid pid = 0;
	gchar * argv[2] = {
		"/usr/share/apport/recoverable_problem",
		NULL
	};

	g_spawn_async_with_pipes(NULL, /* cwd */
		argv,
		NULL, /* envp */
		G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL | G_SPAWN_DO_NOT_REAP_CHILD,
		NULL, NULL, /* child setup func */
		&pid,
		&error_stdin,
		NULL, /* stdout */
		NULL, /* stderr */
		&error);

	if (error != NULL) {
		g_warning("Unable to report a recoverable error: %s", error->message);
		g_error_free(error);
	}

	if (error_stdin != 0) {
		write_string(error_stdin, "IconValue");
		write_null(error_stdin);
		write_string(error_stdin, originalicon);
		write_null(error_stdin);

		write_string(error_stdin, "AppID");
		write_null(error_stdin);
		write_string(error_stdin, app_id);
		write_null(error_stdin);

		write_string(error_stdin, "IconPath");
		write_null(error_stdin);
		write_string(error_stdin, iconpath);
		write_null(error_stdin);

		write_string(error_stdin, "IconField");
		write_null(error_stdin);
		write_string(error_stdin, iconfield);
		write_null(error_stdin);

		write_string(error_stdin, "DuplicateSignature");
		write_null(error_stdin);
		write_string(error_stdin, "icon-path-unhandled");
		/* write_null(error_stdin); -- No final NULL */

		close(error_stdin);
	}

	if (pid != 0) {
		GSource * child_source, * timeout_source;
		GMainContext * context = g_main_context_new();
		GMainLoop * loop = g_main_loop_new(context, FALSE);

		child_source = g_child_watch_source_new(pid);
		g_source_attach(child_source, context);
		g_source_set_callback(child_source, (GSourceFunc)apport_child_watch, loop, NULL);

		timeout_source = g_timeout_source_new_seconds(5);
		g_source_attach(timeout_source, context);
		g_source_set_callback(timeout_source, apport_child_timeout, loop, NULL);

		g_main_loop_run(loop);

		g_source_destroy(timeout_source);
		g_source_destroy(child_source);
		g_main_loop_unref(loop);
		g_main_context_unref(context);

		g_spawn_close_pid(pid);
	}

	return;
}

/* Function to take the source Desktop file and build a new
   one with similar, but not the same data in it */
static void
copy_desktop_file (const gchar * from, const gchar * to, const gchar * appdir, const gchar * app_id)
{
	GError * error = NULL;
	GKeyFile * keyfile = g_key_file_new();
	g_key_file_load_from_file(keyfile,
		from,
		G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS,
		&error);

	if (error != NULL) {
		g_warning("Unable to read the desktop file '%s' in the application directory: %s", from, error->message);
		g_error_free(error);
		g_key_file_unref(keyfile);
		return;
	}

	/* Path Hanlding */
	if (g_key_file_has_key(keyfile, DESKTOP_GROUP, PATH_KEY, NULL)) {
		gchar * oldpath = g_key_file_get_string(keyfile, DESKTOP_GROUP, PATH_KEY, NULL);
		g_debug("Desktop file '%s' has a Path set to '%s'.  Setting as " OLD_KEY_PREFIX PATH_KEY ".", from, oldpath);

		g_key_file_set_string(keyfile, DESKTOP_GROUP, OLD_KEY_PREFIX PATH_KEY, oldpath);

		g_free(oldpath);
	}

	g_key_file_set_string(keyfile, DESKTOP_GROUP, PATH_KEY, appdir);

	/* Icon Handling */
	if (g_key_file_has_key(keyfile, DESKTOP_GROUP, ICON_KEY, NULL)) {
		gchar * originalicon = g_key_file_get_string(keyfile, DESKTOP_GROUP, ICON_KEY, NULL);
		gchar * iconpath = g_build_filename(appdir, originalicon, NULL);

		/* If the icon in the path exists, let's use that */
		if (g_file_test(iconpath, G_FILE_TEST_EXISTS)) {
			g_key_file_set_string(keyfile, DESKTOP_GROUP, ICON_KEY, iconpath);
			/* Save the old value, because, debugging */
			g_key_file_set_string(keyfile, DESKTOP_GROUP, OLD_KEY_PREFIX ICON_KEY, originalicon);
		} else {
			/* So here we are, realizing all is lost.  Let's file a bug. */
			/* The goal here is to realize how often this case is, so we know how to prioritize fixing it */

			report_recoverable_error(app_id, ICON_KEY, originalicon, iconpath);
		}

		g_free(iconpath);
		g_free(originalicon);
	}

	/* SymbolicIcon Handling */
	if (g_key_file_has_key(keyfile, DESKTOP_GROUP, SYMBOLIC_ICON_KEY, NULL)) {
		gchar * originalicon = g_key_file_get_string(keyfile, DESKTOP_GROUP, SYMBOLIC_ICON_KEY, NULL);
		gchar * iconpath = g_build_filename(appdir, originalicon, NULL);

		/* If the icon in the path exists, let's use that */
		if (g_file_test(iconpath, G_FILE_TEST_EXISTS)) {
			g_key_file_set_string(keyfile, DESKTOP_GROUP, SYMBOLIC_ICON_KEY, iconpath);
			/* Save the old value, because, debugging */
			g_key_file_set_string(keyfile, DESKTOP_GROUP, OLD_KEY_PREFIX SYMBOLIC_ICON_KEY, originalicon);
		} else {
			/* So here we are, realizing all is lost.  Let's file a bug. */
			/* The goal here is to realize how often this case is, so we know how to prioritize fixing it */

			report_recoverable_error(app_id, SYMBOLIC_ICON_KEY, originalicon, iconpath);
		}

		g_free(iconpath);
		g_free(originalicon);
	}

	/* Exec Handling */
	gchar * oldexec = desktop_to_exec(keyfile, from);
	if (oldexec == NULL) {
		g_key_file_unref(keyfile);
		return;
	}

	gchar * newexec = g_strdup_printf("aa-exec-click -p %s -- %s", app_id, oldexec);
	g_key_file_set_string(keyfile, DESKTOP_GROUP, EXEC_KEY, newexec);
	g_free(newexec);
	g_free(oldexec);

	/* Adding an Application ID */
	g_key_file_set_string(keyfile, DESKTOP_GROUP, APP_ID_KEY, app_id);

	/* Adding the source file path */
	g_key_file_set_string(keyfile, DESKTOP_GROUP, SOURCE_FILE_KEY, from);

	/* Output */
	gsize datalen = 0;
	gchar * data = g_key_file_to_data(keyfile, &datalen, &error);
	g_key_file_unref(keyfile);

	if (error != NULL) {
		g_warning("Unable serialize keyfile built from '%s': %s", from, error->message);
		g_error_free(error);
		return;
	}

	g_file_set_contents(to, data, datalen, &error);
	g_free(data);

	if (error != NULL) {
		g_warning("Unable to write out desktop file to '%s': %s", to, error->message);
		g_error_free(error);
		return;
	}

	return;
}

/* Build a desktop file in the user's home directory */
static void
build_desktop_file (app_state_t * state, const gchar * symlinkdir, const gchar * desktopdir)
{
	GError * error = NULL;
	gchar * package = NULL;
	/* 'Parse' the App ID */
	if (!app_id_to_triplet(state->app_id, &package, NULL, NULL)) {
		return;
	}

	/* Read in the database */
	ClickDB * db = click_db_new();
	click_db_read(db, g_getenv("TEST_CLICK_DB"), &error);
	if (error != NULL) {
		g_warning("Unable to read Click database: %s", error->message);
		g_error_free(error);
		g_free(package);
		g_object_unref(db);
		return;
	}

	/* Check click to find out where the files are */
	ClickUser * user = click_user_new_for_user(db, g_getenv("TEST_CLICK_USER"), &error);
	if (error != NULL) {
		g_warning("Unable to read Click database: %s", error->message);
		g_error_free(error);
		g_free(package);
		g_object_unref(db);
		return;
	}
	g_object_unref(db);

	gchar * pkgdir = click_user_get_path(user, package, &error);
	if (error != NULL) {
		g_warning("Unable to get the Click package directory for %s: %s", package, error->message);
		g_error_free(error);
		g_free(package);
		return;
	}
	g_object_unref(user);
	g_free(package);

	if (!g_file_test(pkgdir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		g_warning("Directory returned by click '%s' couldn't be found", pkgdir);
		g_free(pkgdir);
		return;
	}

	gchar * indesktop = manifest_to_desktop(pkgdir, state->app_id);
	if (indesktop == NULL) {
		g_free(pkgdir);
		return;
	}

	/* Determine the desktop file name */
	gchar * desktopfile = g_strdup_printf("%s.desktop", state->app_id);
	gchar * desktoppath = g_build_filename(desktopdir, desktopfile, NULL);
	g_free(desktopfile);

	copy_desktop_file(indesktop, desktoppath, pkgdir, state->app_id);

	g_free(desktoppath);
	g_free(indesktop);
	g_free(pkgdir);

	return;
}

/* Remove the desktop file from the user's home directory */
static gboolean
remove_desktop_file (app_state_t * state, const gchar * desktopdir)
{
	gchar * desktopfile = g_strdup_printf("%s.desktop", state->app_id);
	gchar * desktoppath = g_build_filename(desktopdir, desktopfile, NULL);
	g_free(desktopfile);

	GKeyFile * keyfile = g_key_file_new();
	g_key_file_load_from_file(keyfile,
		desktoppath,
		G_KEY_FILE_NONE,
		NULL);

	if (!g_key_file_has_key(keyfile, DESKTOP_GROUP, APP_ID_KEY, NULL)) {
		g_debug("Desktop file '%s' is not one created by us.", desktoppath);
		g_key_file_unref(keyfile);
		g_free(desktoppath);
		return FALSE;
	}
	g_key_file_unref(keyfile);

	if (g_unlink(desktoppath) != 0) {
		g_warning("Unable to delete desktop file: %s", desktoppath);
	}

	g_free(desktoppath);

	return TRUE;
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

	/* Find all the symlinks of desktop files */
	gchar * symlinkdir = g_build_filename(g_get_user_cache_dir(), "ubuntu-app-launch", "desktop", NULL);
	if (!g_file_test(symlinkdir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		g_debug("No installed click packages");
	} else {
		dir_for_each(symlinkdir, add_click_package, apparray);
	}

	/* Find all the click desktop files */
	gchar * desktopdir = g_build_filename(g_get_user_data_dir(), "applications", NULL);
	gboolean desktopdirexists = FALSE;
	if (!g_file_test(desktopdir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		g_debug("No applications defined");
	} else {
		dir_for_each(desktopdir, add_desktop_file, apparray);
		desktopdirexists = TRUE;
	}

	/* Process the merge */
	int i;
	for (i = 0; i < apparray->len; i++) {
		app_state_t * state = &g_array_index(apparray, app_state_t, i);
		g_debug("Processing App ID: %s", state->app_id);

		if (state->has_click && state->has_desktop) {
			if (state->click_modified > state->desktop_modified) {
				g_debug("\tClick updated more recently");
				g_debug("\tRemoving desktop file");
				if (remove_desktop_file(state, desktopdir)) {
					g_debug("\tBuilding desktop file");
					build_desktop_file(state, symlinkdir, desktopdir);
				}
			} else {
				g_debug("\tAlready synchronized");
			}
		} else if (state->has_click) {
			if (!desktopdirexists) {
				if (g_mkdir_with_parents(desktopdir, 0755) == 0) {
					g_debug("\tCreated applications directory");
					desktopdirexists = TRUE;
				} else {
					g_warning("\tUnable to create applications directory");
				}
			}
			if (desktopdirexists) {
				g_debug("\tBuilding desktop file");
				build_desktop_file(state, symlinkdir, desktopdir);
			}
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
