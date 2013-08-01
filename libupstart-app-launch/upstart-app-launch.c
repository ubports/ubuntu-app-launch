
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

gchar **
upstart_app_launch_list_running_apps (void)
{
	gchar ** retval = g_new(gchar *, 1);
	retval[0] = NULL;

	return retval;
}

GPid
upstart_app_launch_check_app_running (const gchar * appid)
{

	return 0;
}
