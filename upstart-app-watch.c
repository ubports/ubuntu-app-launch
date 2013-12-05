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

#include "libupstart-app-launch/upstart-app-launch.h"

void
starting (const gchar * appid, gpointer user_data)
{
	g_print("Starting %s\n", appid);
	return;
}

void
started (const gchar * appid, gpointer user_data)
{
	g_print("Started  %s\n", appid);
	return;
}

void
stopped (const gchar * appid, gpointer user_data)
{
	g_print("Stop     %s\n", appid);
	return;
}

void
resume (const gchar * appid, gpointer user_data)
{
	g_print("Resume   %s\n", appid);
	return;
}

void
focus (const gchar * appid, gpointer user_data)
{
	g_print("Focus    %s\n", appid);
	return;
}

void
fail (const gchar * appid, upstart_app_launch_app_failed_t failhow, gpointer user_data)
{
	const gchar * failstr = "unknown";
	switch (failhow) {
		case UPSTART_APP_LAUNCH_APP_FAILED_CRASH:
			failstr = "crashed";
			break;
		case UPSTART_APP_LAUNCH_APP_FAILED_START_FAILURE:
			failstr = "startup";
			break;
	}

	g_print("Fail   %s (%s)\n", appid, failstr);
	return;
}


int
main (int argc, gchar * argv[])
{
	upstart_app_launch_observer_add_app_starting(starting, NULL);
	upstart_app_launch_observer_add_app_started(started, NULL);
	upstart_app_launch_observer_add_app_stop(stopped, NULL);
	upstart_app_launch_observer_add_app_focus(focus, NULL);
	upstart_app_launch_observer_add_app_resume(resume, NULL);
	upstart_app_launch_observer_add_app_failed(fail, NULL);

	GMainLoop * mainloop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(mainloop);

	upstart_app_launch_observer_delete_app_starting(starting, NULL);
	upstart_app_launch_observer_delete_app_started(started, NULL);
	upstart_app_launch_observer_delete_app_stop(stopped, NULL);
	upstart_app_launch_observer_delete_app_focus(focus, NULL);
	upstart_app_launch_observer_delete_app_resume(resume, NULL);
	upstart_app_launch_observer_delete_app_failed(fail, NULL);

	g_main_loop_unref(mainloop);

	return 0;
}
