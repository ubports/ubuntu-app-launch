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
#include "helpers.h"

/* Globals */
GPid app_pid = 0;
GMainLoop * mainloop = NULL;
guint connections_open = 0;
const gchar * appid = NULL;
const gchar * input_uris = NULL;
GVariant * app_data = NULL;
gchar * dbus_path = NULL;
guint64 unity_starttime = 0;
guint timer = 0;

/* Unity didn't respond in time, continue on */
static gboolean
timer_cb (gpointer user_data)
{
	g_warning("Unity didn't respond in 500ms to resume the app");
	g_main_loop_quit(mainloop);
	return G_SOURCE_REMOVE;
}

/* Lower the connection count and process if it gets to zero */
static void
connection_count_dec (void)
{
	connections_open--;
	if (connections_open == 0) {
		/* Check time here, either we've already heard from
		   Unity and we should send the data to the app (quit) or
		   we should wait some more */
		guint64 timespent = g_get_monotonic_time() - unity_starttime;
		if (timespent > 500 /* ms */ * 1000 /* ms to us */) {
			g_main_loop_quit(mainloop);
		} else {
			timer = g_timeout_add(500 - (timespent / 1000), timer_cb, NULL);
		}
	}
	return;
}

/* Called when Unity is done unfreezing the application, if we're
   done determining the PID, we can send signals */
static void
unity_resume_cb (GDBusConnection * connection, const gchar * sender, const gchar * path, const gchar * interface, const gchar * signal, GVariant * params, gpointer user_data)
{
	g_debug("Unity Completed Resume");

	if (timer != 0) {
		g_source_remove(timer);
	}

	if (connections_open == 0) {
		g_main_loop_quit(mainloop);
	} else {
		/* Make it look like we started *forever* ago */
		unity_starttime = 0;
	}

	return;
}

/* Turn the input string into something we can send to apps */
static void
parse_uris (void)
{
	if (app_data != NULL) {
		/* Already done */
		return;
	}

	GVariant * uris = NULL;
	gchar ** uri_split = g_strsplit(input_uris, " ", 0);
	if (uri_split[0] == NULL) {
		g_free(uri_split);
		uris = g_variant_new_array(G_VARIANT_TYPE_STRING, NULL, 0);
	} else {
		GVariantBuilder builder;
		g_variant_builder_init(&builder, G_VARIANT_TYPE_ARRAY);

		int i;
		for (i = 0; uri_split[i] != NULL; i++) {
			g_variant_builder_add_value(&builder, g_variant_new_take_string(uri_split[i]));
		}
		g_free(uri_split);

		uris = g_variant_builder_end(&builder);
	}

	GVariant * platform = g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0);

	GVariantBuilder tuple;
	g_variant_builder_init(&tuple, G_VARIANT_TYPE_TUPLE);
	g_variant_builder_add_value(&tuple, uris);
	g_variant_builder_add_value(&tuple, platform);

	app_data = g_variant_builder_end(&tuple);
	g_variant_ref_sink(app_data);

	return;
}

/* Finds us our dbus path to use.  Basically this is the name
   of the application with dots replaced by / and a / tacted on
   the front.  This is recommended here:

   http://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#dbus   
*/
static void
app_id_to_dbus_path (void)
{
	if (dbus_path != NULL) {
		return;
	}

	/* If it's an app id, use the application name, otherwise
	   assume legacy and it's the desktop file name */
	gchar * application = NULL;
	if (!app_id_to_triplet(appid, NULL, &application, NULL)) {
		application = g_strdup(appid);
	}

	gchar * dot = g_utf8_strchr(application, -1, '.');
	while (dot != NULL) {
		dot[0] = '/';
		dot = g_utf8_strchr(application, -1, '.');
	}

	dbus_path = g_strdup_printf("/%s", application);
	g_free(application);
	return;
}

/* Finish the send and decrement the counter */
static void
send_open_cb (GObject * object, GAsyncResult * res, gpointer user_data)
{
	GError * error = NULL;

	g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), res, &error);

	if (error != NULL) {
		/* Mostly just to free the error, but printing for debugging */
		g_debug("Unable to send Open: %s", error->message);
		g_error_free(error);
	}

	connection_count_dec();
	return;
}

/* Sends the Open message to the connection with the URIs we were given */
static void
contact_app (GDBusConnection * bus, const gchar * connection)
{
	parse_uris();
	app_id_to_dbus_path();

	/* Using the FD.o Application interface */
	g_dbus_connection_call(bus,
		connection,
		dbus_path,
		"org.freedesktop.Application",
		"Open",
		app_data,
		NULL,
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		send_open_cb, NULL);

	g_debug("Sending Open request to: %s", connection);

	return;
}

/* Gets the PID for a connection, and if it matches the one we're looking
   for then it tries to send a message to that connection */
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
		/* Trying to send a message to the connection */
		contact_app(G_DBUS_CONNECTION(object), connection);
	} else {
		/* See if we can quit now */
		connection_count_dec();
	}

	g_free(connection);

	return;
}

/* Starts to look for the PID and the connections for that PID */
void
find_appid_pid (void)
{
	GError * error = NULL;

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
		return;
	}

	/* Now as we know we're going async */
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

	return;
}

int
main (int argc, char * argv[])
{
	if (argc != 1) {
		g_error("Should be called as: %s", argv[0]);
		return 1;
	}

	appid = g_getenv("APP_ID");
	input_uris = g_getenv("APP_URIS");

	/* First figure out what we're looking for (and if there is something to look for) */
	app_pid = upstart_app_launch_get_primary_pid(appid);
	if (app_pid == 0) {
		g_warning("Unable to find pid for app id '%s'", appid);
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

	/* Allocate main loop */
	mainloop = g_main_loop_new(NULL, FALSE);

	/* Set up listening for the unfrozen signal from Unity */
	g_dbus_connection_signal_subscribe(session,
		NULL, /* sender */
		"com.canonical.UpstartAppLaunch", /* interface */
		"UnityResumeResponse", /* signal */
		"/", /* path */
		appid, /* arg0 */
		G_DBUS_SIGNAL_FLAGS_NONE,
		unity_resume_cb, mainloop,
		NULL); /* user data destroy */

	/* Send unfreeze to to Unity */
	g_dbus_connection_emit_signal(session,
		NULL, /* destination */
		"/", /* path */
		"com.canonical.UpstartAppLaunch", /* interface */
		"UnityResumeRequest", /* signal */
		g_variant_new("(s)", appid),
		&error);

	/* Now we start a race, we try to get to the point of knowing who
	   to send things to, and Unity is unfrezing it.  When both are
	   done we can send something to the app */
	unity_starttime = g_get_monotonic_time();

	if (error != NULL) {
		/* On error let's not wait for Unity */
		g_warning("Unable to signal Unity: %s", error->message);
		g_error_free(error);
		error = NULL;
		unity_starttime = 0;
	}

	/* If we've got something to give out, start looking for how */
	if (app_uris != NULL) {
		find_appid_pid();
	}

	/* Loop and wait for everything to align */
	g_main_loop_run(mainloop);
	g_debug("Finishing main loop");

	/* Now that we're done sending the info to the app, we can ask
	   Unity to focus the application. */
	g_dbus_connection_emit_signal(session,
		NULL, /* destination */
		"/", /* path */
		"com.canonical.UpstartAppLaunch", /* interface */
		"UnityFocusRequest", /* signal */
		g_variant_new("(s)", appid),
		&error);

	if (error != NULL) {
		g_warning("Unable to request focus to Unity: %s", error->message);
		g_error_free(error);
		error = NULL;
	}

	/* Make sure the signal hits the bus */
	g_dbus_connection_flush_sync(session, NULL, NULL);

	/* Clean up */
	if (app_data != NULL) {
		g_variant_unref(app_data);
	}

	g_main_loop_unref(mainloop);
	g_object_unref(session);
	g_free(dbus_path);

	return 0;
}
