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

#include "ual-tracepoint.h"
#include "libubuntu-app-launch/recoverable-problem.h"

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

/* Quick way to get the pid of cgmanager to report a bug on it */
static GPid
discover_cgmanager_pid (void)
{
	gchar * outbuf = NULL;
	GPid outpid = 0;
	
	if (g_spawn_command_line_sync("pidof cgmanager", &outbuf, NULL, NULL, NULL)) {
		outpid = g_ascii_strtoull(outbuf, NULL, 10);
	}

	g_free(outbuf);

	return outpid;
}

/* Structure to handle data for the cgmanager connection
   set of callbacks */
typedef struct {
	GMainLoop * loop;
	GCancellable * cancel;
	GDBusConnection * con;
} cgm_connection_t;

/* Function that gets executed when we timeout trying to connect. This
   is related to: LP #1377332 */
static gboolean
cgroup_manager_connection_timeout_cb (gpointer data)
{
	GPid cgmanager_pid = 0;
	cgm_connection_t * connection = (cgm_connection_t *)data;

	g_cancellable_cancel(connection->cancel);

	cgmanager_pid = discover_cgmanager_pid();
	if (cgmanager_pid != 0) {
		report_recoverable_problem("ubuntu-app-launch-cgmanager-connection-timeout", cgmanager_pid, FALSE, NULL);
	}

	return G_SOURCE_CONTINUE;
}

static void
cgroup_manager_connection_core_cb (GDBusConnection *(*finish_func)(GAsyncResult * res, GError ** error), GAsyncResult * res, cgm_connection_t * connection)
{
	GError * error = NULL;

	connection->con = finish_func(res, &error);

	if (error != NULL) {
		g_warning("Unable to get cgmanager connection: %s", error->message);
		g_error_free(error);
	}

	g_main_loop_quit(connection->loop);
}

static void
cgroup_manager_connection_bus_cb (GObject * obj, GAsyncResult * res, gpointer data)
{
	cgroup_manager_connection_core_cb(g_bus_get_finish, res, (cgm_connection_t *)data);
}

static void
cgroup_manager_connection_addr_cb (GObject * obj, GAsyncResult * res, gpointer data)
{
	cgroup_manager_connection_core_cb(g_dbus_connection_new_for_address_finish, res, (cgm_connection_t *)data);
}

/* Get the connection to the cgroup manager */
GDBusConnection *
cgroup_manager_connection (void)
{
	cgm_connection_t connection = {
		.loop = g_main_loop_new(NULL, FALSE),
		.con = NULL,
		.cancel = g_cancellable_new()
	};
	guint timeout = g_timeout_add_seconds(1, cgroup_manager_connection_timeout_cb, &connection);

	if (g_getenv("UBUNTU_APP_LAUNCH_CG_MANAGER_SESSION_BUS")) {
		/* For working dbusmock */
		g_debug("Connecting to CG Manager on session bus");
		g_bus_get(G_BUS_TYPE_SESSION,
			connection.cancel,
			cgroup_manager_connection_bus_cb,
			&connection);
	} else {
		g_dbus_connection_new_for_address(
			CGMANAGER_DBUS_PATH,
			G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
			NULL, /* Auth Observer */
			connection.cancel, /* Cancellable */
			cgroup_manager_connection_addr_cb,
			&connection);
	}

	g_main_loop_run(connection.loop);

	g_source_remove(timeout);
	g_main_loop_unref(connection.loop);
	g_object_unref(connection.cancel);

	return connection.con;
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
	gchar * groupname = NULL;
	if (jobname != NULL) {
		groupname = g_strdup_printf("upstart/%s-%s", jobname, instancename);
	}

	g_debug("Looking for cg manager '%s' group '%s'", name, groupname);

	GVariant * vtpids = g_dbus_connection_call_sync(cgmanager,
		name, /* bus name for direct connection is NULL */
		"/org/linuxcontainers/cgmanager",
		"org.linuxcontainers.cgmanager0_0",
		"GetTasks",
		g_variant_new("(ss)", "freezer", groupname ? groupname : ""),
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

/* Global markers for the ual_tracepoint macro */
int _ual_tracepoints_env_checked = 0;
int _ual_tracepoints_enabled = 0;

