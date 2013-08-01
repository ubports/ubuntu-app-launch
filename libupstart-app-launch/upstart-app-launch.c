
#include "upstart-app-launch.h"
#include <upstart.h>
#include <nih/alloc.h>

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

gboolean
upstart_app_launch_observer_add_app_start (upstart_app_launch_app_observer_t observer, gpointer user_data)
{

	return FALSE;
}

gboolean
upstart_app_launch_observer_add_app_stop (upstart_app_launch_app_observer_t observer, gpointer user_data)
{

	return FALSE;
}

gboolean
upstart_app_launch_observer_delete_app_start (upstart_app_launch_app_observer_t observer, gpointer user_data)
{

	return FALSE;
}

gboolean
upstart_app_launch_observer_delete_app_stop (upstart_app_launch_app_observer_t observer, gpointer user_data)
{

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
