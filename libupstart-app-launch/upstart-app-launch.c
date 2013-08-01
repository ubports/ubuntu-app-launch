
#include "upstart-app-launch.h"
#include <gio/gio.h>

gboolean
upstart_app_launch_start_application (const gchar * appid, const gchar * const * uris)
{
	const gchar * upstart_session = g_getenv("UPSTART_SESSION");
	if (upstart_session == NULL) {
		g_warning("Upstart User Session Not Found");
		return FALSE;
	}

	GError * error = NULL;
	GDBusConnection * upstart = g_dbus_connection_new_for_address_sync(upstart_session,
	                                                                   G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
	                                                                   NULL, /* auth */
	                                                                   NULL, /* cancel */
	                                                                   &error);
	if (error != NULL) {
		g_warning("Unable to connect to Upstart bus: %s", error->message);
		g_error_free(error);
		return FALSE;
	}

	GVariantBuilder builder;
	g_variant_builder_init(&builder, G_VARIANT_TYPE_TUPLE);

	g_variant_builder_add_value(&builder, g_variant_new_string("application-start"));

	g_variant_builder_open(&builder, G_VARIANT_TYPE_ARRAY);

	gchar * env_appid = g_strdup_printf("APP_ID=%s", appid);
	g_variant_builder_add_value(&builder, g_variant_new_take_string(env_appid));

	gchar * env_uris = NULL;

	if (uris != NULL) {
		gchar * urisjoin = g_strjoinv(" ", (gchar **)uris);
		env_uris = g_strdup_printf("APP_URIS=%s", urisjoin);
		g_free(urisjoin);

		g_variant_builder_add_value(&builder, g_variant_new_take_string(env_uris));
	}

	g_variant_builder_close(&builder);
	g_variant_builder_add_value(&builder, g_variant_new_boolean(FALSE));

	g_dbus_connection_call_sync(upstart,
	                            "com.ubuntu.Upstart",
	                            "/com/ubuntu/Upstart",
	                            "com.ubuntu.Upstart0_6",
	                            "EmitEvent",
	                            g_variant_builder_end(&builder),
	                            NULL,
	                            G_DBUS_CALL_FLAGS_NONE,
	                            -1,
	                            NULL,
	                            &error);

	gboolean retval = TRUE;
	if (error != NULL) {
		g_error("Unable to emit event: %s", error->message);
		g_error_free(error);
		retval = FALSE;
	}

	g_object_unref(upstart);

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
