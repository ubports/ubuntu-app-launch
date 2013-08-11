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
		return 1;
	}

	/* Header */
	g_print("  PID  TYPE  NAME\n");


	GVariant * array = g_variant_get_child_value(instances, 0);
	GVariantIter iter;
	g_variant_iter_init(&iter, array);
	gchar * instance_path = NULL;

	while (g_variant_iter_loop(&iter, "o", &instance_path)) {
		GVariant * propret   = g_dbus_connection_call_sync(upstart,
		                                                   "com.ubuntu.Upstart",
		                                                   instance_path,
		                                                   "org.freedesktop.DBus.Properties",
		                                                   "GetAll",
		                                                   g_variant_new("(s)", "com.ubuntu.Upstart0_6.Instance"),
		                                                   G_VARIANT_TYPE("(a{sv})"),
		                                                   G_DBUS_CALL_FLAGS_NONE,
		                                                   -1,
		                                                   NULL,
		                                                   &error);

		if (error != NULL) {
			g_warning("Unable to get props for '%s': %s", instance_path, error->message);
			g_error_free(error);
			error = NULL;
			continue;
		}

		GVariant * params = g_variant_get_child_value(propret, 0);
		GVariant * vname = g_variant_lookup_value(params, "name", G_VARIANT_TYPE_STRING);

		const gchar * name = NULL;
		if (vname != NULL) {
			name = g_variant_get_string(vname, NULL);
			if (name[0] == '\0') {
				name = "(unnamed)";
			}
		} else {
			name = "(no name)";
		}

		GVariant * processes = g_variant_lookup_value(params, "processes", G_VARIANT_TYPE("a(si)"));
		if (processes != NULL) {
			GVariantIter iproc;
			g_variant_iter_init(&iproc, processes);
			gchar * type;
			gint pid;

			while (g_variant_iter_loop(&iproc, "(si)", &type, &pid)) {
				g_print("%5d  %4s  %s\n", pid, type, name);
			}

			g_variant_unref(processes);
		} else {
			g_warning("No processes for application: %s", name);
		}

		g_variant_unref(vname);
		g_variant_unref(params);
		g_variant_unref(propret);
	}


	g_variant_unref(array);
	g_variant_unref(instances);

	g_object_unref(upstart);

	return 0;
}

