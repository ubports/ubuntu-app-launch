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
#include <nih/alloc.h>
#include <libnih-dbus.h>
#include "libubuntu-app-launch/ubuntu-app-launch.h"
#include "helpers.h"
#include "second-exec-core.h"
#include "ubuntu-app-launch-trace.h"
#include "ual-tracepoint.h"

typedef struct {
	GDBusConnection * bus;
	gchar * appid;
	gchar * instanceid;
	gchar ** input_uris;
	GPid app_pid;
	guint connections_open;
	GVariant * app_data;
	gchar * dbus_path;
	guint64 unity_starttime;
	GSource * timer;
	guint signal;
} second_exec_t;

static void second_exec_complete (second_exec_t * data);

static GSource *
thread_default_timeout (guint interval, GSourceFunc func, gpointer data)
{
	GSource * src = g_timeout_source_new(interval);
	GMainContext * context = g_main_context_get_thread_default();

	g_source_set_callback(src, func, data, NULL);

	g_source_attach(src, context);

	return src;
}

/* Unity didn't respond in time, continue on */
static gboolean
timer_cb (gpointer user_data)
{
	ual_tracepoint(second_exec_resume_timeout, ((second_exec_t *)user_data)->appid);
	g_warning("Unity didn't respond in 500ms to resume the app");

	second_exec_complete(user_data);
	return G_SOURCE_REMOVE;
}

/* Lower the connection count and process if it gets to zero */
static void
connection_count_dec (second_exec_t * data)
{
	ual_tracepoint(second_exec_connection_complete, data->appid);
	data->connections_open--;
	if (data->connections_open == 0) {
		g_debug("Finished finding connections");
		/* Check time here, either we've already heard from
		   Unity and we should send the data to the app (quit) or
		   we should wait some more */
		guint64 timespent = g_get_monotonic_time() - data->unity_starttime;
		if (timespent > 500 /* ms */ * 1000 /* ms to us */) {
			second_exec_complete(data);
		} else {
			g_debug("Timer Set");
			data->timer = thread_default_timeout(500 - (timespent / 1000), timer_cb, data);
		}
	}
	return;
}

/* Called when Unity is done unfreezing the application, if we're
   done determining the PID, we can send signals */
static void
unity_resume_cb (GDBusConnection * connection, const gchar * sender, const gchar * path, const gchar * interface, const gchar * signal, GVariant * params, gpointer user_data)
{
	second_exec_t * data = (second_exec_t *)user_data;
	g_debug("Unity Completed Resume");
	ual_tracepoint(second_exec_resume_complete, data->appid);

	if (data->timer != NULL) {
		g_source_destroy(data->timer);
		g_source_unref(data->timer);
		data->timer = NULL;
	}

	if (data->connections_open == 0) {
		second_exec_complete(data);
	} else {
		/* Make it look like we started *forever* ago */
		data->unity_starttime = 0;
	}

	return;
}

/* Turn the input string into something we can send to apps */
static void
parse_uris (second_exec_t * data)
{
	if (data->app_data != NULL) {
		/* Already done */
		return;
	}

	GVariant * uris = NULL;

	if (data->input_uris == NULL) {
		uris = g_variant_new_array(G_VARIANT_TYPE_STRING, NULL, 0);
	} else {
		GVariantBuilder builder;
		g_variant_builder_init(&builder, G_VARIANT_TYPE_ARRAY);

		int i;
		for (i = 0; data->input_uris[i] != NULL; i++) {
			g_variant_builder_add_value(&builder, g_variant_new_string(data->input_uris[i]));
		}

		uris = g_variant_builder_end(&builder);
	}

	GVariant * platform = g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0);

	GVariantBuilder tuple;
	g_variant_builder_init(&tuple, G_VARIANT_TYPE_TUPLE);
	g_variant_builder_add_value(&tuple, uris);
	g_variant_builder_add_value(&tuple, platform);

	data->app_data = g_variant_builder_end(&tuple);
	g_variant_ref_sink(data->app_data);

	return;
}

/* Finds us our dbus path to use.  Basically this is the name
   of the application with dots replaced by / and a / tacted on
   the front.  This is recommended here:

   http://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#dbus   
*/
static void
app_id_to_dbus_path (second_exec_t * data)
{
	if (data->dbus_path != NULL) {
		return;
	}

	GString * str = g_string_sized_new(strlen(data->appid) + 2); /* base case, we just need a / and a null */
	g_string_append_c(str, '/');

	int i;
	for (i = 0; data->appid[i] != '\0'; i++) {
		if ((data->appid[i] >= 'a' && data->appid[i] <= 'z') ||
			(data->appid[i] >= 'A' && data->appid[i] <= 'Z') ||
			(data->appid[i] >= '0' && data->appid[i] <= '9' && i != 0)) {
			g_string_append_c(str, data->appid[i]);
			continue;
		}

		g_string_append_printf(str, "_%2x", data->appid[i]);
	}

	data->dbus_path = g_string_free(str, FALSE);
	g_debug("DBus Path: %s", data->dbus_path);

	return;
}

/* Finish the send and decrement the counter */
static void
send_open_cb (GObject * object, GAsyncResult * res, gpointer user_data)
{
	GError * error = NULL;

	ual_tracepoint(second_exec_app_contacted, ((second_exec_t *)user_data)->appid);

	g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), res, &error);

	if (error != NULL) {
		ual_tracepoint(second_exec_app_error, ((second_exec_t *)user_data)->appid);
		/* Mostly just to free the error, but printing for debugging */
		g_debug("Unable to send Open: %s", error->message);
		g_error_free(error);
	}

	connection_count_dec(user_data);
	return;
}

/* Sends the Open message to the connection with the URIs we were given */
static void
contact_app (GDBusConnection * bus, const gchar * dbus_name, second_exec_t * data)
{
	ual_tracepoint(second_exec_contact_app, data->appid, dbus_name);

	parse_uris(data);
	app_id_to_dbus_path(data);

	/* Using the FD.o Application interface */
	g_dbus_connection_call(bus,
		dbus_name,
		data->dbus_path,
		"org.freedesktop.Application",
		"Open",
		data->app_data,
		NULL,
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		send_open_cb, data);

	g_debug("Sending Open request to: %s", dbus_name);

	return;
}

typedef struct {
	gchar * name;
	second_exec_t * data;
} get_pid_t;

/* Gets the PID for a connection, and if it matches the one we're looking
   for then it tries to send a message to that connection */
static void
get_pid_cb (GObject * object, GAsyncResult * res, gpointer user_data)
{
	get_pid_t * data = (get_pid_t *)user_data;
	GError * error = NULL;
	GVariant * vpid = NULL;

	ual_tracepoint(second_exec_got_pid, data->data->appid, data->name);

	vpid = g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), res, &error);

	if (error != NULL) {
		g_warning("Unable to query PID for dbus name '%s': %s", data->name, error->message);
		g_error_free(error);

		/* Lowering the connection count, this one is terminal, even if in error */
		connection_count_dec(data->data);

		g_free(data->name);
		g_free(data);

		return;
	}

	guint pid = 0;
	g_variant_get(vpid, "(u)", &pid);
	g_variant_unref(vpid);

	if (pid == data->data->app_pid) {
		/* Trying to send a message to the connection */
		contact_app(G_DBUS_CONNECTION(object), data->name, data->data);
	} else {
		/* See if we can quit now */
		connection_count_dec(data->data);
	}

	g_free(data->name);
	g_free(data);

	return;
}

/* Starts to look for the PID and the connections for that PID */
static void
find_appid_pid (GDBusConnection * session, second_exec_t * data)
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

	g_debug("Got bus names");
	ual_tracepoint(second_exec_got_dbus_names, data->appid);

	g_debug("Primary PID: %d", data->app_pid);
	ual_tracepoint(second_exec_got_primary_pid, data->appid);

	/* Get the names */
	GVariant * names = g_variant_get_child_value(listnames, 0);
	GVariantIter iter;
	g_variant_iter_init(&iter, names);
	gchar * name = NULL;

	while (g_variant_iter_loop(&iter, "s", &name)) {
		/* We only want to ask each connection once, this makes that so */
		if (!g_dbus_is_unique_name(name)) {
			continue;
		}
		
		get_pid_t * pid_data = g_new0(get_pid_t, 1);
		pid_data->data = data;
		pid_data->name = g_strdup(name);

		ual_tracepoint(second_exec_request_pid, data->appid, pid_data->name);

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
			get_pid_cb, pid_data);

		data->connections_open++;
	}

	g_variant_unref(names);
	g_variant_unref(listnames);

	return;
}

gboolean
second_exec (GDBusConnection * session, GCancellable * cancel, GPid pid, const gchar * app_id, const gchar * instance_id, gchar ** appuris)
{
	ual_tracepoint(second_exec_start, app_id);
	GError * error = NULL;

	/* Setup our continuation data */
	second_exec_t * data = g_new0(second_exec_t, 1);
	data->appid = g_strdup(app_id);
	data->instanceid = g_strdup(instance_id);
	data->input_uris = g_strdupv(appuris);
	data->bus = g_object_ref(session);
	data->app_pid = pid;

	/* Set up listening for the unfrozen signal from Unity */
	data->signal = g_dbus_connection_signal_subscribe(session,
		NULL, /* sender */
		"com.canonical.UbuntuAppLaunch", /* interface */
		"UnityResumeResponse", /* signal */
		"/", /* path */
		app_id, /* arg0 */
		G_DBUS_SIGNAL_FLAGS_NONE,
		unity_resume_cb, data,
		NULL); /* user data destroy */

	g_debug("Sending resume request");
	ual_tracepoint(second_exec_emit_resume, app_id);

	/* Send unfreeze to to Unity */
	g_dbus_connection_emit_signal(session,
		NULL, /* destination */
		"/", /* path */
		"com.canonical.UbuntuAppLaunch", /* interface */
		"UnityResumeRequest", /* signal */
		g_variant_new("(ss)", app_id, instance_id),
		&error);

	/* Now we start a race, we try to get to the point of knowing who
	   to send things to, and Unity is unfrezing it.  When both are
	   done we can send something to the app */
	data->unity_starttime = g_get_monotonic_time();

	if (error != NULL) {
		/* On error let's not wait for Unity */
		g_warning("Unable to signal Unity: %s", error->message);
		g_error_free(error);
		error = NULL;
		data->unity_starttime = 0;
	}

	/* If we've got something to give out, start looking for how */
	if (data->input_uris != NULL) {
		find_appid_pid(session, data);
	} else {
		g_debug("No URIs to send");
	}

	/* Loop and wait for everything to align */
	if (data->connections_open == 0) {
		if (data->unity_starttime == 0) {
			second_exec_complete(data);
		} else {
			data->timer = thread_default_timeout(500, timer_cb, data);
		}
	}

	return TRUE;
}

static void
second_exec_complete (second_exec_t * data)
{
	GError * error = NULL;
	ual_tracepoint(second_exec_emit_focus, data->appid);

	/* Now that we're done sending the info to the app, we can ask
	   Unity to focus the application. */
	g_dbus_connection_emit_signal(data->bus,
		NULL, /* destination */
		"/", /* path */
		"com.canonical.UbuntuAppLaunch", /* interface */
		"UnityFocusRequest", /* signal */
		g_variant_new("(ss)", data->appid, data->instanceid),
		&error);

	if (error != NULL) {
		g_warning("Unable to request focus to Unity: %s", error->message);
		g_error_free(error);
		error = NULL;
	}

	/* Make sure the signal hits the bus */
	g_dbus_connection_flush_sync(data->bus, NULL, &error);
	if (error != NULL) {
		g_warning("Unable to flush session bus: %s", error->message);
		g_error_free(error);
		error = NULL;
	}

	ual_tracepoint(second_exec_finish, data->appid);
	g_debug("Second Exec complete");

	/* Clean up */
	if (data->signal != 0)
		g_dbus_connection_signal_unsubscribe(data->bus, data->signal);

	if (data->timer != NULL) {
		g_source_destroy(data->timer);
		g_source_unref(data->timer);
	}
	g_object_unref(data->bus);
	if (data->app_data != NULL)
		g_variant_unref(data->app_data);
	g_free(data->appid);
	g_free(data->instanceid);
	g_strfreev(data->input_uris);
	g_free(data->dbus_path);
	g_free(data);

	return;
}
