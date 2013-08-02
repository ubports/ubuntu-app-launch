
#include "upstart-app-launch.h"
#include <upstart.h>
#include <nih/alloc.h>
#include <gio/gio.h>
#include <string.h>

static NihDBusProxy *
nih_proxy_create (void)
{
	DBusError        error;
	DBusConnection * conn;
	NihDBusProxy *   upstart;
	const gchar *    upstart_session;

	upstart_session = g_getenv("UPSTART_SESSION");
	if (upstart_session == NULL) {
		g_warning("Not running under Upstart User Session");
		return NULL;
	}

	dbus_error_init(&error);
	conn = dbus_connection_open_private(upstart_session, &error);

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
	NihDBusProxy * proxy = nih_proxy_create();
	if (proxy == NULL) {
		return FALSE;
	}

	gchar * env_appid = g_strdup_printf("APP_ID=%s", appid);
	gchar * env_uris = NULL;

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

GDBusConnection *
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
		observer->func(instance, 0, observer->user_data);
	}

	g_free(instance);

	return;
}


gboolean
upstart_app_launch_observer_add_app_start (upstart_app_launch_app_observer_t observer, gpointer user_data)
{
	observer_t observert;
	observert.conn = gdbus_upstart_ref();

	if (observert.conn == NULL) {
		return FALSE;
	}

	observert.func = observer;
	observert.user_data = user_data;

	if (start_array == NULL) {
		start_array = g_array_new(FALSE, FALSE, sizeof(observer_t));
	}
	g_array_append_val(start_array, observert);
	observer_t * pobserver = &g_array_index(start_array, observer_t, start_array->len - 1);

	pobserver->sighandle = g_dbus_connection_signal_subscribe(observert.conn,
		NULL, /* sender */
		DBUS_INTERFACE_UPSTART, /* interface */
		"EventEmitted", /* signal */
		DBUS_PATH_UPSTART, /* path */
		"starting", /* arg0 */
		G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
		observer_cb,
		pobserver,
		NULL); /* user data destroy */

	return TRUE;
}

gboolean
upstart_app_launch_observer_add_app_stop (upstart_app_launch_app_observer_t observer, gpointer user_data)
{
	observer_t observert;
	observert.conn = gdbus_upstart_ref();

	if (observert.conn == NULL) {
		return FALSE;
	}

	observert.func = observer;
	observert.user_data = user_data;

	if (stop_array == NULL) {
		stop_array = g_array_new(FALSE, FALSE, sizeof(observer_t));
	}
	g_array_append_val(stop_array, observert);
	observer_t * pobserver = &g_array_index(stop_array, observer_t, stop_array->len - 1);

	pobserver->sighandle = g_dbus_connection_signal_subscribe(observert.conn,
		NULL, /* sender */
		DBUS_INTERFACE_UPSTART, /* interface */
		"EventEmitted", /* signal */
		DBUS_PATH_UPSTART, /* path */
		"stopped", /* arg0 */
		G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
		observer_cb,
		pobserver,
		NULL); /* user data destroy */

	return TRUE;
}

gboolean
upstart_app_launch_observer_delete_app_start (upstart_app_launch_app_observer_t observer, gpointer user_data)
{
	int i;
	observer_t * observert = NULL;
	for (i = 0; i < start_array->len; i++) {
		observert = &g_array_index(start_array, observer_t, i);

		if (observert->func == observer && observert->user_data == user_data) {
			break;
		}
	}

	if (i == start_array->len) {
		return FALSE;
	}

	g_dbus_connection_signal_unsubscribe(observert->conn, observert->sighandle);
	g_object_unref(observert->conn);
	g_array_remove_index_fast(start_array, i);

	if (start_array->len == 0) {
		g_array_free(start_array, TRUE);
		start_array = NULL;
	}

	return TRUE;
}

gboolean
upstart_app_launch_observer_delete_app_stop (upstart_app_launch_app_observer_t observer, gpointer user_data)
{
	int i;
	observer_t * observert = NULL;
	for (i = 0; i < stop_array->len; i++) {
		observert = &g_array_index(stop_array, observer_t, i);

		if (observert->func == observer && observert->user_data == user_data) {
			break;
		}
	}

	if (i == stop_array->len) {
		return FALSE;
	}

	g_dbus_connection_signal_unsubscribe(observert->conn, observert->sighandle);
	g_object_unref(observert->conn);
	g_array_remove_index_fast(stop_array, i);

	if (stop_array->len == 0) {
		g_array_free(stop_array, TRUE);
		stop_array = NULL;
	}

	return FALSE;
}

/* Get all the instances for a given job name */
static void
apps_for_job (NihDBusProxy * upstart, const gchar * name, GArray * apps)
{
	char * job_path = NULL;
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

	gchar ** instances;
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

		gchar * instance_name = NULL;
		if (job_get_name_sync(NULL, instance_proxy, &instance_name) == 0) {
			gchar * dup = g_strdup(instance_name);

			if (g_strcmp0(name, "application-legacy") == 0) {
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
	NihDBusProxy * proxy = nih_proxy_create();
	if (proxy == NULL) {
		return g_new0(gchar *, 1);
	}

	GArray * apps = g_array_new(TRUE, TRUE, sizeof(gchar *));

	apps_for_job(proxy, "application-legacy", apps);
	apps_for_job(proxy, "application-click", apps);

	nih_unref(proxy, NULL);

	return (gchar **)g_array_free(apps, FALSE);
}

GPid
upstart_app_launch_check_app_running (const gchar * appid)
{

	return 0;
}
