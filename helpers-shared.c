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

#include "helpers.h"
#include <gio/gio.h>
#include <cgmanager/cgmanager.h>

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

/* Get the connection to the cgroup manager */
GDBusConnection *
cgroup_manager_connection (void)
{
	GError * error = NULL;

	GDBusConnection * cgmanager = NULL;

	if (g_getenv("UBUNTU_APP_LAUNCH_CG_MANAGER_SESSION_BUS")) {
		/* For working dbusmock */
		g_debug("Connecting to CG Manager on session bus");
		cgmanager = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
	} else {
		cgmanager = g_dbus_connection_new_for_address_sync(
			CGMANAGER_DBUS_PATH,
			G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
			NULL, /* Auth Observer */
			NULL, /* Cancellable */
			&error);
	}

	if (error != NULL) {
		g_warning("Unable to connect to cgroup manager: %s", error->message);
		g_error_free(error);
		return NULL;
	}

	return cgmanager;
}

/* Get the PIDs for a particular cgroup */
/* We're using the base cgroup 'freezer' in this code (and
   in the Upstart jobs). Really the actual group is meaningless
   we just need one that is in every kernel we need to support.
   We're just using the cgroup as a bag of PIDs, not for
   restricting any particular resource. */
GList *
pids_from_cgroup (GDBusConnection * cgmanager, const gchar * jobname, const gchar * instancename)
{
	GError * error = NULL;
	const gchar * name = g_getenv("UBUNTU_APP_LAUNCH_CG_MANAGER_NAME");
	gchar * groupname = g_strdup_printf("upstart/%s-%s", jobname, instancename);

	g_debug("Looking for cg manager '%s' group '%s'", name, groupname);

	GVariant * vtpids = g_dbus_connection_call_sync(cgmanager,
		name, /* bus name for direct connection is NULL */
		"/org/linuxcontainers/cgmanager",
		"org.linuxcontainers.cgmanager0_0",
		"GetTasks",
		g_variant_new("(ss)", "freezer", groupname),
		G_VARIANT_TYPE("(ai)"),
		G_DBUS_CALL_FLAGS_NONE,
		-1, /* default timeout */
		NULL, /* cancellable */
		&error);

	g_free(groupname);

	if (error != NULL) {
		g_warning("Unable to get PID list from cgroup manager: %s", error->message);
		g_error_free(error);
		return NULL;
	}

	GVariant * vpids = g_variant_get_child_value(vtpids, 0);
	GVariantIter iter;
	g_variant_iter_init(&iter, vpids);
	gint32 pid;
	GList * retval = NULL;

	while (g_variant_iter_loop(&iter, "i", &pid)) {
		retval = g_list_prepend(retval, GINT_TO_POINTER(pid));
	}

	g_variant_unref(vpids);
	g_variant_unref(vtpids);

	return retval;
}
