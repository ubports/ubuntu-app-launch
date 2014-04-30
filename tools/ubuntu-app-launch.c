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

#include <glib.h>
#include "libubuntu-app-launch/ubuntu-app-launch.h"

const gchar * global_appid = NULL;
int retval = 0;

static void
good_observer (const gchar * appid, gpointer user_data)
{
	if (g_strcmp0(appid, global_appid) != 0) {
		return;
	}

	g_debug("Application '%s' running", appid);
	g_main_loop_quit((GMainLoop *)user_data);
}

static void
bad_observer (const gchar * appid, UbuntuAppLaunchAppFailed failure_type, gpointer user_data)
{
	if (g_strcmp0(appid, global_appid) != 0) {
		return;
	}

	g_debug("Application '%s' failed: %s", appid, failure_type == UBUNTU_APP_LAUNCH_APP_FAILED_CRASH ? "crash" : "startup failure");
	retval = -1;
	g_main_loop_quit((GMainLoop *)user_data);
}

int
main (int argc, gchar * argv[]) {
	if (argc < 2) {
		g_printerr("Usage: %s <app id> [uris]\n", argv[0]);
		return 1;
	}

	global_appid = argv[1];
	GMainLoop * mainloop = g_main_loop_new(NULL, FALSE);

	gchar ** uris = NULL;
	if (argc > 2) {
		int i;

		uris = g_new0(gchar *, argc - 1);

		for (i = 2; i < argc; i++) {
			uris[i - 2] = argv[i];
		}
	}

	ubuntu_app_launch_observer_add_app_started(good_observer, mainloop);
	ubuntu_app_launch_observer_add_app_focus(good_observer, mainloop);

	ubuntu_app_launch_observer_add_app_failed(bad_observer, mainloop);

	ubuntu_app_launch_start_application(global_appid, (const gchar * const *)uris);

	g_main_loop_run(mainloop);

	ubuntu_app_launch_observer_delete_app_started(good_observer, mainloop);
	ubuntu_app_launch_observer_delete_app_focus(good_observer, mainloop);
	ubuntu_app_launch_observer_delete_app_failed(bad_observer, mainloop);

	g_main_loop_unref(mainloop);
	g_free(uris);

	return retval;
}
