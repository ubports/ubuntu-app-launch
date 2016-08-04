/*
 * Copyright © 2015 Canonical Ltd.
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
#include <click.h>

#include "ubuntu-app-launch.h"
#include "app-info.h"

/* Try and get a manifest and do a couple sanity checks on it */
static JsonObject *
get_manifest (const gchar * pkg, gchar ** pkgpath)
{
	/* Get the directory from click */
	GError * error = NULL;

	ClickDB * db = click_db_new();
	/* If TEST_CLICK_DB is unset, this reads the system database. */
	click_db_read(db, g_getenv("TEST_CLICK_DB"), &error);
	if (error != NULL) {
		g_warning("Unable to read Click database: %s", error->message);
		g_error_free(error);
		g_object_unref(db);
		return NULL;
	}
	/* If TEST_CLICK_USER is unset, this uses the current user name. */
	ClickUser * user = click_user_new_for_user(db, g_getenv("TEST_CLICK_USER"), &error);
	if (error != NULL) {
		g_warning("Unable to read Click database: %s", error->message);
		g_error_free(error);
		g_object_unref(db);
		return NULL;
	}
	g_object_unref(db);
	JsonObject * manifest = click_user_get_manifest(user, pkg, &error);
	if (error != NULL) {
		g_warning("Unable to get manifest for '%s' package: %s", pkg, error->message);
		g_error_free(error);
		g_object_unref(user);
		return NULL;
	}

	if (pkgpath != NULL) {
		*pkgpath = click_user_get_path(user, pkg, &error);
		if (error != NULL) {
			g_warning("Unable to get the Click package directory for %s: %s", pkg, error->message);
			g_error_free(error);
			g_object_unref(user);
			return NULL;
		}
	}
	g_object_unref(user);

	if (!json_object_has_member(manifest, "version")) {
		g_warning("Manifest file for package '%s' does not have a version", pkg);
		json_object_unref(manifest);
		return NULL;
	}

	return manifest;
}

/* Types of search we can do for an app name */
enum _app_name_t {
	APP_NAME_ONLY,
	APP_NAME_FIRST,
	APP_NAME_LAST
};
typedef enum _app_name_t app_name_t;

/* Figure out the app name if it's one of the keywords */
static const gchar *
manifest_app_name (JsonObject ** manifest, const gchar * pkg, const gchar * original_app)
{
	app_name_t app_type = APP_NAME_FIRST;

	if (original_app == NULL) {
		/* first */
	} else if (g_strcmp0(original_app, "first-listed-app") == 0) {
		/* first */
	} else if (g_strcmp0(original_app, "last-listed-app") == 0) {
		app_type = APP_NAME_LAST;
	} else if (g_strcmp0(original_app, "only-listed-app") == 0) {
		app_type = APP_NAME_ONLY;
	} else {
		return original_app;
	}

	if (*manifest == NULL) {
		*manifest = get_manifest(pkg, NULL);
	}

	JsonObject * hooks = json_object_get_object_member(*manifest, "hooks");

	if (hooks == NULL) {
		return NULL;
	}

	GList * apps = json_object_get_members(hooks);
	if (apps == NULL) {
		return NULL;
	}

	const gchar * retapp = NULL;

	switch (app_type) {
	case APP_NAME_ONLY:
		if (g_list_length(apps) == 1) {
			retapp = (const gchar *)apps->data;
		}
		break;
	case APP_NAME_FIRST:
		retapp = (const gchar *)apps->data;
		break;
	case APP_NAME_LAST:
		retapp = (const gchar *)(g_list_last(apps)->data);
		break;
	default:
		break;
	}

	g_list_free(apps);

	return retapp;
}

/* Figure out the app version using the manifest */
static const gchar *
manifest_version (JsonObject ** manifest, const gchar * pkg, const gchar * original_ver)
{
	if (original_ver != NULL && g_strcmp0(original_ver, "current-user-version") != 0) {
		return original_ver;
	} else  {
		if (*manifest == NULL) {
			*manifest = get_manifest(pkg, NULL);
		}
		g_return_val_if_fail(*manifest != NULL, NULL);

		return g_strdup(json_object_get_string_member(*manifest, "version"));
	}

	return NULL;
}

/* A click triplet can require using the Click DB and getting a
   manifest. This code does that to look up the versions */
gchar *
click_triplet_to_app_id (const gchar * pkg, const gchar * app, const gchar * ver)
{
	const gchar * version = NULL;
	const gchar * application = NULL;
	JsonObject * manifest = NULL;

	version = manifest_version(&manifest, pkg, ver);
	g_return_val_if_fail(version != NULL, NULL);

	application = manifest_app_name(&manifest, pkg, app);
	g_return_val_if_fail(application != NULL, NULL);

	gchar * retval = g_strdup_printf("%s_%s_%s", pkg, application, version);

	/* The object may hold allocation for some of our strings used above */
	if (manifest)
		json_object_unref(manifest);

	return retval;
}

/* Build an appid how we think it should exist and then make sure
   we can find it. Then pull it together. */
gchar *
libertine_triplet_to_app_id (const gchar * pkg, const gchar * app, const gchar * ver)
{
	if (app == NULL) {
		return NULL;
	}

	gchar * synthappid = g_strdup_printf("%s_%s_0.0", pkg, app);
	if (app_info_libertine(synthappid, NULL, NULL)) {
		return synthappid;
	} else {
		g_free(synthappid);
		return NULL;
	}
}

/* Look to see if the app id results in a desktop file, if so, fill in the params */
static gboolean
evaluate_dir (const gchar * dir, const gchar * desktop, gchar ** appdir, gchar ** appdesktop)
{
	char * fulldir = g_build_filename(dir, "applications", desktop, NULL);
	gboolean found = FALSE;

	if (g_file_test(fulldir, G_FILE_TEST_EXISTS)) {
		if (appdir != NULL) {
			*appdir = g_strdup(dir);
		}

		if (appdesktop != NULL) {
			*appdesktop = g_strdup_printf("applications/%s", desktop);
		}

		found = TRUE;
	}

	g_free(fulldir);
	return found;
}

/* Handle the legacy case where we look through the data directories */
gboolean
app_info_legacy (const gchar * appid, gchar ** appdir, gchar ** appdesktop)
{
	gchar * desktop = g_strdup_printf("%s.desktop", appid);

	/* Special case the user's dir */
	if (evaluate_dir(g_get_user_data_dir(), desktop, appdir, appdesktop)) {
		g_free(desktop);
		return TRUE;
	}

	const char * const * data_dirs = g_get_system_data_dirs();
	int i;
	for (i = 0; data_dirs[i] != NULL; i++) {
		if (evaluate_dir(data_dirs[i], desktop, appdir, appdesktop)) {
			g_free(desktop);
			return TRUE;
		}
	}

	return FALSE;
}

/* Handle the libertine case where we look in the container */
gboolean
app_info_libertine (const gchar * appid, gchar ** appdir, gchar ** appdesktop)
{
	char * container = NULL;
	char * app = NULL;

	if (!ubuntu_app_launch_app_id_parse(appid, &container, &app, NULL)) {
		return FALSE;
	}

	gchar * desktopname = g_strdup_printf("%s.desktop", app);

	gchar * desktopdir = g_build_filename(g_get_user_cache_dir(), "libertine-container", container, "rootfs", "usr", "share", NULL);
	gchar * desktopfile = g_build_filename(desktopdir, "applications", desktopname, NULL);

	if (!g_file_test(desktopfile, G_FILE_TEST_EXISTS)) {
		g_free(desktopdir);
		g_free(desktopfile);

		desktopdir = g_build_filename(g_get_user_data_dir(), "libertine-container", "user-data", container, ".local", "share", NULL);
		desktopfile = g_build_filename(desktopdir, "applications", desktopname, NULL);

		if (!g_file_test(desktopfile, G_FILE_TEST_EXISTS)) {
			g_free(desktopdir);
			g_free(desktopfile);

			g_free(desktopname);
			g_free(container);
			g_free(app);

			return FALSE;
		}
	}

	if (appdir != NULL) {
		*appdir = desktopdir;
	} else {
		g_free(desktopdir);
	}

	if (appdesktop != NULL) {
		*appdesktop = g_build_filename("applications", desktopname, NULL);
	}

	g_free(desktopfile);
	g_free(desktopname);
	g_free(container);
	g_free(app);

	return TRUE;
}

/* Get the information on where the desktop file is from libclick */
gboolean
app_info_click (const gchar * appid, gchar ** appdir, gchar ** appdesktop)
{
	gchar * package = NULL;
	gchar * application = NULL;

	if (!ubuntu_app_launch_app_id_parse(appid, &package, &application, NULL)) {
		return FALSE;
	}

	JsonObject * manifest = get_manifest(package, appdir);
	if (manifest == NULL) {
		g_free(package);
		g_free(application);
		return FALSE;
	}

	g_free(package);

	if (appdesktop != NULL) {
		JsonObject * hooks = json_object_get_object_member(manifest, "hooks");
		if (hooks == NULL) {
			json_object_unref(manifest);
			g_free(application);
			return FALSE;
		}

		JsonObject * appobj = json_object_get_object_member(hooks, application);
		g_free(application);

		if (appobj == NULL) {
			json_object_unref(manifest);
			return FALSE;
		}

		const gchar * desktop = json_object_get_string_member(appobj, "desktop");
		if (desktop == NULL) {
			json_object_unref(manifest);
			return FALSE;
		}

		*appdesktop = g_strdup(desktop);
	} else {
		g_free(application);
	}

	json_object_unref(manifest);

	return TRUE;
}

/* Determine whether it's a click package by looking for the symlink
   that is created by the desktop hook */
static gboolean
is_click (const gchar * appid)
{
	gchar * appiddesktop = g_strdup_printf("%s.desktop", appid);
	gchar * click_link = NULL;
	const gchar * link_farm_dir = g_getenv("UBUNTU_APP_LAUNCH_LINK_FARM");
	if (G_LIKELY(link_farm_dir == NULL)) {
		click_link = g_build_filename(g_get_home_dir(), ".cache", "ubuntu-app-launch", "desktop", appiddesktop, NULL);
	} else {
		click_link = g_build_filename(link_farm_dir, appiddesktop, NULL);
	}
	g_free(appiddesktop);
	gboolean click = g_file_test(click_link, G_FILE_TEST_EXISTS);
	g_free(click_link);

	return click;
}

/* Determine whether an AppId is realated to a Libertine container by
   checking the container and program name. */
static gboolean
is_libertine (const gchar * appid)
{
	if (app_info_libertine(appid, NULL, NULL)) {
		g_debug("Libertine application detected: %s", appid);
		return TRUE;
	} else {
		return FALSE;
	}
}

gboolean
ubuntu_app_launch_application_info (const gchar * appid, gchar ** appdir, gchar ** appdesktop)
{
	if (is_click(appid)) {
		return app_info_click(appid, appdir, appdesktop);
	} else if (is_libertine(appid)) {
		return app_info_libertine(appid, appdir, appdesktop);
	} else {
		return app_info_legacy(appid, appdir, appdesktop);
	}
}

