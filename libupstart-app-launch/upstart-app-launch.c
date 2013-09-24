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

#include "upstart-app-launch.h"
#include <upstart.h>
#include <nih/alloc.h>
#include <gio/gio.h>
#include <string.h>

static void apps_for_job (NihDBusProxy * upstart, const gchar * name, GArray * apps, gboolean truncate_legacy);

static NihDBusProxy *
nih_proxy_create (void)
{
	NihDBusProxy *   upstart;
	DBusConnection * conn;
	DBusError        error;
	const gchar *    upstart_session;

	upstart_session = g_getenv("UPSTART_SESSION");
	if (upstart_session == NULL) {
		g_warning("Not running under Upstart User Session");
		return NULL;
	}

	dbus_error_init(&error);
	conn = dbus_connection_open(upstart_session, &error);

	if (conn == NULL) {
		g_warning("Unable to connect to the Upstart Session: %s", error.message);
		dbus_error_free(&error);
		return NULL;
	}

	dbus_error_free(&error);

	upstart = nih_dbus_proxy_new(NULL, conn,
		NULL,
		DBUS_PATH_UPSTART,
		NULL, NULL);

	if (upstart == NULL) {
		g_warning("Unable to build proxy to Upstart");
		dbus_connection_close(conn);
		dbus_connection_unref(conn);
		return NULL;
	}

	dbus_connection_unref(conn);

	upstart->auto_start = FALSE;

	return upstart;
}

gboolean
upstart_app_launch_start_application (const gchar * appid, const gchar * const * uris)
{
	NihDBusProxy * proxy = NULL;

	proxy = nih_proxy_create();
	if (proxy == NULL) {
		return FALSE;
	}

	gchar * env_appid = g_strdup_printf("APP_ID=%s", appid);
	gchar * env_uris = NULL;

	/* TODO: Joining only with space could cause issues with breaking them
	   back out.  We don't have any cases of more than one today.  But, this
	   isn't good.
	   https://bugs.launchpad.net/upstart-app-launch/+bug/1229354
	   */
	if (uris != NULL) {
		gchar * urisjoin = g_strjoinv(" ", (gchar **)uris);
		env_uris = g_strdup_printf("APP_URIS=%s", urisjoin);
		g_free(urisjoin);
	}

	gchar * env[3];
	env[0] = env_appid;
	env[1] = env_uris;
	env[2] = NULL;

	gboolean retval = TRUE;
	if (upstart_emit_event_sync(NULL, proxy, "application-start", env, 0) != 0) {
		g_warning("Unable to emit signal 'application-start'");
		retval = FALSE;
	}

	g_free(env_appid);
	g_free(env_uris);
	nih_unref(proxy, NULL);

	return retval;
}

static void
stop_job (NihDBusProxy * upstart, const gchar * jobname, const gchar * appname, const gchar * instanceid)
{
	g_debug("Stopping job %s app_id %s instance_id %s", jobname, appname, instanceid);
	nih_local char * job_path = NULL;
	if (upstart_get_job_by_name_sync(NULL, upstart, jobname, &job_path) != 0) {
		g_warning("Unable to find job '%s'", jobname);
		return;
	}

	NihDBusProxy * job_proxy = nih_dbus_proxy_new(NULL, upstart->connection,
		NULL,
		job_path,
		NULL, NULL);

	if (job_proxy == NULL) {
		g_warning("Unable to build proxy to Job '%s'", jobname);
		return;
	}

	gchar * app = g_strdup_printf("APP_ID=%s", appname);
	gchar * inst = NULL;
	
	if (instanceid != NULL) {
		inst = g_strdup_printf("INSTANCE_ID=%s", instanceid);
	}

	gchar * env[3] = {
		app,
		inst,
		NULL
	};

	if (job_class_stop_sync(NULL, job_proxy, env, 0) != 0) {
		g_warning("Unable to stop job %s app %s instance %s", jobname, appname, instanceid);
	}

	g_free(app);
	g_free(inst);
	nih_unref(job_proxy, NULL);

	return;
}

static void
free_helper (gpointer value)
{
	gchar ** strp = (gchar **)value;
	g_free(*strp);
}

gboolean
upstart_app_launch_stop_application (const gchar * appid)
{
	gboolean found = FALSE;
	int i;
	NihDBusProxy * proxy = NULL;

	proxy = nih_proxy_create();
	if (proxy == NULL) {
		return FALSE;
	}

	GArray * apps = g_array_new(TRUE, TRUE, sizeof(gchar *));
	g_array_set_clear_func(apps, free_helper);

	/* Look through the click jobs and see if any match.  There can
	   only be one instance for each ID in the click world */
	apps_for_job(proxy, "application-click", apps, FALSE);
	for (i = 0; i < apps->len; i++) {
		const gchar * array_id = g_array_index(apps, const gchar *, i);
		if (g_strcmp0(array_id, appid) == 0) {
			stop_job(proxy, "application-click", appid, NULL);
			found = TRUE;
			break; /* There can be only one with click */
		}
	}

	if (apps->len > 0)
		g_array_remove_range(apps, 0, apps->len);

	/* Look through the legacy apps.  Trickier because we know that there
	   can be many instances of the legacy jobs out there, so we might
	   have to kill more than one of them. */
	apps_for_job(proxy, "application-legacy", apps, FALSE);
	gchar * appiddash = g_strdup_printf("%s-", appid); /* Probably could go RegEx here, but let's start with just a prefix lookup */
	for (i = 0; i < apps->len; i++) {
		const gchar * array_id = g_array_index(apps, const gchar *, i);
		if (g_str_has_prefix(array_id, appiddash)) {
			gchar * instanceid = g_strrstr(array_id, "-");
			stop_job(proxy, "application-legacy", appid, &(instanceid[1]));
			found = TRUE;
		}
	}
	g_free(appiddash);

	g_array_free(apps, TRUE);

	return found;
}

static GDBusConnection *
gdbus_upstart_ref (void) {
	static GDBusConnection * gdbus_upstart = NULL;

	if (gdbus_upstart != NULL) {
		return g_object_ref(gdbus_upstart);
	}

	const gchar * upstart_addr = g_getenv("UPSTART_SESSION");
	if (upstart_addr == NULL) {
		g_print("Doesn't appear to be an upstart user session\n");
		return NULL;
	}

	GError * error = NULL;
	gdbus_upstart = g_dbus_connection_new_for_address_sync(upstart_addr,
	                                                       G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
	                                                       NULL, /* auth */
	                                                       NULL, /* cancel */
	                                                       &error);

	if (error != NULL) {
		g_warning("Unable to connect to Upstart bus: %s", error->message);
		g_error_free(error);
		return NULL;
	}

	g_object_add_weak_pointer(G_OBJECT(gdbus_upstart), (gpointer)&gdbus_upstart);

	return gdbus_upstart;
}

/* The data we keep for each observer */
typedef struct _observer_t observer_t;
struct _observer_t {
	GDBusConnection * conn;
	guint sighandle;
	upstart_app_launch_app_observer_t func;
	gpointer user_data;
};

/* The Arrays of Observers */
static GArray * start_array = NULL;
static GArray * stop_array = NULL;

static void
observer_cb (GDBusConnection * conn, const gchar * sender, const gchar * object, const gchar * interface, const gchar * signal, GVariant * params, gpointer user_data)
{
	observer_t * observer = (observer_t *)user_data;

	gchar * env = NULL;
	GVariant * envs = g_variant_get_child_value(params, 1);
	GVariantIter iter;
	g_variant_iter_init(&iter, envs);

	gboolean job_found = FALSE;
	gboolean job_legacy = FALSE;
	gchar * instance = NULL;

	while (g_variant_iter_loop(&iter, "s", &env)) {
		if (g_strcmp0(env, "JOB=application-click") == 0) {
			job_found = TRUE;
		} else if (g_strcmp0(env, "JOB=application-legacy") == 0) {
			job_found = TRUE;
			job_legacy = TRUE;
		} else if (g_str_has_prefix(env, "INSTANCE=")) {
			instance = g_strdup(env + strlen("INSTANCE="));
		}
	}

	g_variant_unref(envs);

	if (job_legacy && instance != NULL) {
		gchar * dash = g_strrstr(instance, "-");
		if (dash != NULL) {
			dash[0] = '\0';
		}
	}

	if (job_found && instance != NULL) {
		observer->func(instance, observer->user_data);
	}

	g_free(instance);

	return;
}

/* Creates the observer structure and registers for the signal with
   GDBus so that we can get a callback */
static gboolean
add_app_generic (upstart_app_launch_app_observer_t observer, gpointer user_data, const gchar * signal, GArray ** array)
{
	observer_t observert;
	observert.conn = gdbus_upstart_ref();

	if (observert.conn == NULL) {
		return FALSE;
	}

	observert.func = observer;
	observert.user_data = user_data;

	if (*array == NULL) {
		*array = g_array_new(FALSE, FALSE, sizeof(observer_t));
	}
	g_array_append_val(*array, observert);
	observer_t * pobserver = &g_array_index(*array, observer_t, (*array)->len - 1);

	pobserver->sighandle = g_dbus_connection_signal_subscribe(observert.conn,
		NULL, /* sender */
		DBUS_INTERFACE_UPSTART, /* interface */
		"EventEmitted", /* signal */
		DBUS_PATH_UPSTART, /* path */
		signal, /* arg0 */
		G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
		observer_cb,
		pobserver,
		NULL); /* user data destroy */

	return TRUE;
}

gboolean
upstart_app_launch_observer_add_app_start (upstart_app_launch_app_observer_t observer, gpointer user_data)
{
	return add_app_generic(observer, user_data, "starting", &start_array);
}

gboolean
upstart_app_launch_observer_add_app_stop (upstart_app_launch_app_observer_t observer, gpointer user_data)
{
	return add_app_generic(observer, user_data, "stopped", &stop_array);
}

gboolean
upstart_app_launch_observer_add_app_focus (upstart_app_launch_app_observer_t observer, gpointer user_data)
{
	return FALSE;
}

gboolean
upstart_app_launch_observer_add_app_resume (upstart_app_launch_app_observer_t observer, gpointer user_data)
{
	return FALSE;
}

gboolean
upstart_app_launch_observer_add_app_failed (upstart_app_launch_app_failed_observer_t observer, gpointer user_data)
{
	return FALSE;
}

static gboolean
delete_app_generic (upstart_app_launch_app_observer_t observer, gpointer user_data, GArray ** array)
{
	int i;
	observer_t * observert = NULL;
	for (i = 0; i < (*array)->len; i++) {
		observert = &g_array_index(*array, observer_t, i);

		if (observert->func == observer && observert->user_data == user_data) {
			break;
		}
	}

	if (i == (*array)->len) {
		return FALSE;
	}

	g_dbus_connection_signal_unsubscribe(observert->conn, observert->sighandle);
	g_object_unref(observert->conn);
	g_array_remove_index_fast(*array, i);

	if ((*array)->len == 0) {
		g_array_free(*array, TRUE);
		*array = NULL;
	}

	return TRUE;
}

gboolean
upstart_app_launch_observer_delete_app_start (upstart_app_launch_app_observer_t observer, gpointer user_data)
{
	return delete_app_generic(observer, user_data, &start_array);
}

gboolean
upstart_app_launch_observer_delete_app_stop (upstart_app_launch_app_observer_t observer, gpointer user_data)
{
	return delete_app_generic(observer, user_data, &stop_array);
}

gboolean
upstart_app_launch_observer_delete_app_resume (upstart_app_launch_app_observer_t observer, gpointer user_data)
{
	return FALSE;
}

gboolean
upstart_app_launch_observer_delete_app_focus (upstart_app_launch_app_observer_t observer, gpointer user_data)
{
	return FALSE;
}

gboolean
upstart_app_launch_observer_delete_app_failed (upstart_app_launch_app_failed_observer_t observer, gpointer user_data)
{
	return FALSE;
}

/* Get all the instances for a given job name */
static void
apps_for_job (NihDBusProxy * upstart, const gchar * name, GArray * apps, gboolean truncate_legacy)
{
	nih_local char * job_path = NULL;
	if (upstart_get_job_by_name_sync(NULL, upstart, name, &job_path) != 0) {
		g_warning("Unable to find job '%s'", name);
		return;
	}

	NihDBusProxy * job_proxy = nih_dbus_proxy_new(NULL, upstart->connection,
		NULL,
		job_path,
		NULL, NULL);

	if (job_proxy == NULL) {
		g_warning("Unable to build proxy to Job '%s'", name);
		return;
	}

	nih_local char ** instances;
	if (job_class_get_all_instances_sync(NULL, job_proxy, &instances) != 0) {
		g_warning("Unable to get instances for job '%s'", name);
		nih_unref(job_proxy, NULL);
		return;
	}

	int jobnum;
	for (jobnum = 0; instances[jobnum] != NULL; jobnum++) {
		NihDBusProxy * instance_proxy = nih_dbus_proxy_new(NULL, upstart->connection,
			NULL,
			instances[jobnum],
			NULL, NULL);

		nih_local char * instance_name = NULL;
		if (job_get_name_sync(NULL, instance_proxy, &instance_name) == 0) {
			gchar * dup = g_strdup(instance_name);

			if (truncate_legacy && g_strcmp0(name, "application-legacy") == 0) {
				gchar * last_dash = g_strrstr(dup, "-");
				if (last_dash != NULL) {
					last_dash[0] = '\0';
				}
			}

			g_array_append_val(apps, dup);
		} else {
			g_warning("Unable to get name for instance '%s' of job '%s'", instances[jobnum], name);
		}

		nih_unref(instance_proxy, NULL);
	}

	nih_unref(job_proxy, NULL);

	return;
}

gchar **
upstart_app_launch_list_running_apps (void)
{
	NihDBusProxy * proxy = NULL;

	proxy = nih_proxy_create();
	if (proxy == NULL) {
		return g_new0(gchar *, 1);
	}

	GArray * apps = g_array_new(TRUE, TRUE, sizeof(gchar *));

	apps_for_job(proxy, "application-legacy", apps, TRUE);
	apps_for_job(proxy, "application-click", apps, FALSE);

	nih_unref(proxy, NULL);

	return (gchar **)g_array_free(apps, FALSE);
}

/* Look for the app for a job */
static GPid
pid_for_job (NihDBusProxy * upstart, const gchar * job, const gchar * appid)
{
	nih_local char * job_path = NULL;
	if (upstart_get_job_by_name_sync(NULL, upstart, job, &job_path) != 0) {
		g_warning("Unable to find job '%s'", job);
		return 0;
	}

	NihDBusProxy * job_proxy = nih_dbus_proxy_new(NULL, upstart->connection,
		NULL,
		job_path,
		NULL, NULL);

	if (job_proxy == NULL) {
		g_warning("Unable to build proxy to Job '%s'", job);
		return 0;
	}

	nih_local char ** instances;
	if (job_class_get_all_instances_sync(NULL, job_proxy, &instances) != 0) {
		g_warning("Unable to get instances for job '%s'", job);
		nih_unref(job_proxy, NULL);
		return 0;
	}

	GPid pid = 0;
	int jobnum;
	for (jobnum = 0; instances[jobnum] != NULL && pid == 0; jobnum++) {
		NihDBusProxy * instance_proxy = nih_dbus_proxy_new(NULL, upstart->connection,
			NULL,
			instances[jobnum],
			NULL, NULL);

		nih_local char * instance_name = NULL;
		if (job_get_name_sync(NULL, instance_proxy, &instance_name) == 0) {
			if (g_strcmp0(job, "application-legacy") == 0) {
				gchar * last_dash = g_strrstr(instance_name, "-");
				if (last_dash != NULL) {
					last_dash[0] = '\0';
				}
			}
		} else {
			g_warning("Unable to get name for instance '%s' of job '%s'", instances[jobnum], job);
		}

		if (g_strcmp0(instance_name, appid) == 0) {
			nih_local JobProcessesElement ** elements;
			if (job_get_processes_sync(NULL, instance_proxy, &elements) == 0) {
				pid = elements[0]->item1;
			}
		}

		nih_unref(instance_proxy, NULL);
	}

	nih_unref(job_proxy, NULL);

	return pid;
}

GPid
upstart_app_launch_get_primary_pid (const gchar * appid)
{
	NihDBusProxy * proxy = NULL;

	proxy = nih_proxy_create();
	if (proxy == NULL) {
		return 0;
	}

	GPid pid = 0;

	if (pid == 0) {
		pid = pid_for_job(proxy, "application-legacy", appid);
	}

	if (pid == 0) {
		pid = pid_for_job(proxy, "application-click", appid);
	}

	nih_unref(proxy, NULL);

	return pid;
}

gboolean
upstart_app_launch_pid_in_app_id (GPid pid, const gchar * appid)
{
	if (pid == 0) {
		return FALSE;
	}

	GPid primary = upstart_app_launch_get_primary_pid(appid);

	return primary == pid;
}
