
#include <gio/gio.h>


int
main (int argc, char * argv[])
{
	const gchar * upstart_addr = g_getenv("UPSTART_SESSION");
	if (upstart_addr == NULL) {
		g_print("Doesn't appear to be an upstart user session\n");
		return 1;
	}

	GError * error = NULL;
	GDBusConnection * upstart = g_dbus_connection_new_for_address_sync(upstart_addr,
	                                                                   G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
	                                                                   NULL, /* auth */
	                                                                   NULL, /* cancel */
	                                                                   &error);

	if (error != NULL) {
		g_error("Unable to connect to Upstart bus: %s", error->message);
		g_error_free(error);
		return 1;
	}

	GVariant * instances = g_dbus_connection_call_sync(upstart,
	                                                   "com.ubuntu.Upstart",
	                                                   "/com/ubuntu/Upstart/jobs/application",
	                                                   "com.ubuntu.Upstart0_6.Job",
	                                                   "GetAllInstances",
	                                                   g_variant_new_tuple(NULL, 0),
	                                                   G_VARIANT_TYPE("(ao)"),
	                                                   G_DBUS_CALL_FLAGS_NONE,
	                                                   -1,
	                                                   NULL,
	                                                   &error);

	if (error != NULL) {
		g_error("Unable to list instances: %s", error->message);
		g_error_free(error);
		return;
	}

	/* TODO: Do more */
	g_print("%s\n", g_variant_print(instances, TRUE));
	g_variant_unref(instances);

	g_object_unref(upstart);

	return 0;
}

