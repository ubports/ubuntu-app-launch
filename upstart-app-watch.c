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
started (const gchar * appid, GPid pid, gpointer user_data)
{
	g_print("Start  %s", appid);
	return;
}

void
stopped (const gchar * appid, GPid pid, gpointer user_data)
{
	g_print("Stop   %s", appid);
	return;
}

int
main (int argc, gchar * argv[])
{
	upstart_app_launch_observer_add_app_start(started, NULL);
	upstart_app_launch_observer_add_app_stop(stopped, NULL);

	GMainLoop * mainloop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(mainloop);

	upstart_app_launch_observer_delete_app_start(started, NULL);
	upstart_app_launch_observer_delete_app_stop(stopped, NULL);

	g_main_loop_unref(mainloop);

	return 0;
}
