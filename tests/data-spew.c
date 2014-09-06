/*
 * Copyright © 2014 Canonical Ltd.
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

gboolean
timeout (gpointer user_data)
{
	g_print("Data\n");
	return TRUE;
}

int
main (int argc, char * argv[])
{
	GMainLoop * loop = g_main_loop_new(NULL, FALSE);

	g_timeout_add(100, timeout, NULL);

	g_main_loop_run(loop);
	g_main_loop_unref(loop);

	return 0;
}
