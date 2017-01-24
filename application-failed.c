
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
	const gchar * job = g_getenv("JOB");
	g_return_val_if_fail(job != NULL, -1);

	const gchar * instance = g_getenv("INSTANCE");
	g_return_val_if_fail(instance != NULL, -1);

	gboolean crashed = FALSE;
	if (g_getenv("EXIT_STATUS") != NULL || g_getenv("EXIT_SIGNAL") != NULL) {
		crashed = TRUE;
	}

	gchar * appid = g_strdup(instance);
	gchar * lasthyphenstanding = NULL;
	if (g_strcmp0(job, "application-legacy") == 0
			|| g_strcmp0(job, "application-snap") == 0) {
		lasthyphenstanding = g_strrstr(appid, "-");
		if (lasthyphenstanding != NULL) {
			lasthyphenstanding[0] = '\0';
		} else {
			g_warning("Legacy job instance '%s' is missing a hyphen", appid);
		}
	}

	if (lasthyphenstanding == NULL) {
		lasthyphenstanding = "";
	} else {
		lasthyphenstanding++;
	}

	GDBusConnection * bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	g_return_val_if_fail(bus != NULL, -1);

	GError * error = NULL;
	g_dbus_connection_emit_signal(bus,
		NULL, /* destination */
		"/", /* path */
		"com.canonical.UbuntuAppLaunch",
		"ApplicationFailed",
		g_variant_new("(sss)", appid, lasthyphenstanding, crashed ? "crash" : "start-failure"),
		&error);

	g_debug("Emitting failed event '%s' for app '%s'", crashed ? "crash" : "start-failure", appid);

	if (error != NULL) {
		g_warning("Unable to emit signal: %s", error->message);
		g_error_free(error);
		return -1;
	}

	g_dbus_connection_flush_sync(bus, NULL, NULL);
	g_object_unref(bus);
	g_free(appid);

	return 0;
}
