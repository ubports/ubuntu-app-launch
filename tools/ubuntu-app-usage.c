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

#include <zeitgeist.h>

GPtrArray *
build_event_templates (void)
{
	return g_ptr_array_new_with_free_func(g_object_unref);
}

void
find_events_cb (GObject * obj, GAsyncResult * res, gpointer user_data)
{

	return;
}

int
main (int argc, char * argv[])
{
	GMainLoop * main_loop = g_main_loop_new(NULL, FALSE);
	ZeitgeistLog * log = zeitgeist_log_get_default();

	GPtrArray * templates = build_event_templates();
	ZeitgeistTimeRange * time = zeitgeist_time_range_new_anytime();

	zeitgeist_log_find_events(log,
		time, /* time range */
		templates,
		ZEITGEIST_STORAGE_STATE_ANY, /* storage state */
		10000, /* num events */
		ZEITGEIST_RELEVANT_RESULT_TYPE_RECENT, /* result type */
		NULL, /* cancelable */
		find_events_cb,
		main_loop);

	g_ptr_array_unref(templates);
	g_object_unref(time);

	g_main_loop_run(main_loop);
	g_main_loop_unref(main_loop);

	g_object_unref(log);

	return 0;
}
