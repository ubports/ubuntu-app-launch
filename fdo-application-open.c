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
#include "libupstart-app-launch/upstart-app-launch.h"

/* Globals */
GPid app_pid = 0;
GMainLoop * mainloop = NULL;
guint connections_open = 0;

/* Lower the connection count and process if it gets to zero */
static void
connection_count_dec (void)
{
	connections_open--;
	if (connections_open == 0) {
		g_main_loop_quit(mainloop);
	}
	return;
}

static void
get_pid_cb (GObject * object, GAsyncResult * res, gpointer user_data)
{
	gchar * connection = (gchar *)user_data;
	GError * error = NULL;
	GVariant * vpid = NULL;

	vpid = g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), res, &error);

	if (error != NULL) {
		g_warning("Unable to query PID for connection '%s': %s", connection, error->message);
		g_error_free(error);
		g_free(connection);

		/* Lowering the connection count, this one is terminal, even if in error */
		connection_count_dec();
		return;
	}

	guint pid = 0;
	g_variant_get(vpid, "(u)", &pid);
	g_variant_unref(vpid);

	if (pid == app_pid) {
		g_debug("Connection: %s", connection);
	}

	g_free(connection);

	return;
}

int
main (int argc, char * argv[])
{
	if (argc != 3) {
		g_error("Should be called as: %s <app_id> <uri list>", argv[0]);
		return 1;
	}

	/* First figure out what we're looking for (and if there is something to look for) */
	app_pid = upstart_app_launch_get_primary_pid(argv[1]);
	if (app_pid == 0) {
		g_warning("Unable to find pid for app id '%s'", argv[1]);
		return 1;
	}

	/* DBus tell us! */
	GError * error = NULL;
	GDBusConnection * session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
	if (error != NULL) {
		g_error("Unable to get session bus");
		g_error_free(error);
		return 1;
	}

	/* List all the connections on dbus.  This sucks that we have to do
	   this, but in the future we should add DBus API to do this lookup
	   instead of having to do it with a bunch of requests */
	GVariant * listnames = g_dbus_connection_call_sync(session,
		"org.freedesktop.DBus",
		"/",
		"org.freedesktop.DBus",
		"ListNames",
		NULL,
		G_VARIANT_TYPE("(as)"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		&error);

	if (error != NULL) {
		g_warning("Unable to get list of names from DBus: %s", error->message);
		g_error_free(error);
		return 1;
	}

	/* Allocate the mainloop now as we know we're going async */
	mainloop = g_main_loop_new(NULL, FALSE);

	GVariant * names = g_variant_get_child_value(listnames, 0);
	GVariantIter iter;
	g_variant_iter_init(&iter, names);
	gchar * name = NULL;

	while (g_variant_iter_loop(&iter, "s", &name)) {
		/* We only want to ask each connection once, this makes that so */
		if (!g_dbus_is_unique_name(name)) {
			continue;
		}

		/* Get the PIDs */
		g_dbus_connection_call(session,
			"org.freedesktop.DBus",
			"/",
			"org.freedesktop.DBus",
			"GetConnectionUnixProcessID",
			g_variant_new("(s)", name),
			G_VARIANT_TYPE("(u)"),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			get_pid_cb, g_strdup(name));

		connections_open++;
	}

	g_variant_unref(names);
	g_variant_unref(listnames);

	if (connections_open != 0) {
		g_main_loop_run(mainloop);
	}

	g_main_loop_unref(mainloop);
	g_object_unref(session);

	return 0;
}
