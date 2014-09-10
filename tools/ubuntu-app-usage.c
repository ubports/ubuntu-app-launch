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
	GPtrArray * retval = g_ptr_array_new_with_free_func(g_object_unref);
	ZeitgeistEvent * event;

	event = zeitgeist_event_new();
	zeitgeist_event_set_actor(event, "application://ubuntu-app-launch.desktop");
	zeitgeist_event_set_interpretation(event, ZEITGEIST_ZG_ACCESS_EVENT);
	zeitgeist_event_set_manifestation(event, ZEITGEIST_ZG_USER_ACTIVITY);
	g_ptr_array_add(retval, event);

	event = zeitgeist_event_new();
	zeitgeist_event_set_actor(event, "application://ubuntu-app-launch.desktop");
	zeitgeist_event_set_interpretation(event, ZEITGEIST_ZG_LEAVE_EVENT);
	zeitgeist_event_set_manifestation(event, ZEITGEIST_ZG_USER_ACTIVITY);
	g_ptr_array_add(retval, event);

	return retval;
}

void
print_usage (GHashTable * usage)
{
	GHashTableIter iter;
	g_hash_table_iter_init(&iter, usage);
	gpointer key, value;

	while (g_hash_table_iter_next(&iter, &key, &value)) {
		gchar * appurl = (gchar *)key;
		appurl += strlen("application://");
		gchar * desktop = g_strrstr(appurl, ".desktop");
		if (desktop != NULL)
			desktop[0] = '\0';
		g_print("%s\t%d seconds\n", appurl, GPOINTER_TO_UINT(value));
	}
}

void
find_events_cb (GObject * obj, GAsyncResult * res, gpointer user_data)
{
	/* No matter what, we want to quit */
	g_main_loop_quit((GMainLoop *)user_data);

	GError * error = NULL;
	ZeitgeistResultSet * results = NULL;

	results = zeitgeist_log_find_events_finish(ZEITGEIST_LOG(obj), res, &error);

	if (error != NULL) {
		g_error("Unable to get ZG events: %s", error->message);
		g_error_free(error);
		return;
	}

	GHashTable * laststop = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_date_time_unref);
	GHashTable * usage = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	while (zeitgeist_result_set_has_next(results)) {
		ZeitgeistEvent * event = zeitgeist_result_set_next_value(results);
		ZeitgeistSubject * subject = zeitgeist_event_get_subject(event, 0);
		const gchar * eventtype = "unknown";
		const gchar * appurl = zeitgeist_subject_get_uri(subject);

		if (g_strcmp0(zeitgeist_event_get_interpretation(event), ZEITGEIST_ZG_ACCESS_EVENT) == 0) {
			eventtype = "started";
			GDateTime * stoptime = g_hash_table_lookup(laststop, appurl);
			if (stoptime != NULL) {
				GDateTime * starttime = g_date_time_new_from_unix_utc(zeitgeist_event_get_timestamp(event) / 1000);

				if (starttime != NULL) {
					GTimeSpan runtime = g_date_time_difference(stoptime, starttime);
					guint seconds = runtime / G_TIME_SPAN_SECOND;
					g_date_time_unref(stoptime);

					/* Update the usage table */
					gint previoususage = GPOINTER_TO_UINT(g_hash_table_lookup(usage, appurl));
					g_hash_table_insert(usage, g_strdup(appurl), GUINT_TO_POINTER(previoususage + seconds));
				}

				g_hash_table_remove(laststop, appurl);
			}
		} else {
			eventtype = "stopped";

			GDateTime * stoptime = g_date_time_new_from_unix_utc(zeitgeist_event_get_timestamp(event) / 1000);
			if (stoptime != NULL) {
				g_date_time_ref(stoptime);
				g_hash_table_insert(laststop, g_strdup(appurl), stoptime);
			} else {
				g_debug("Unable to parse start time for: %s", appurl);
			}
		}

		g_debug("Got %s for '%s'" , eventtype, appurl);

		g_object_unref(subject);
		g_object_unref(event);
	}

	print_usage(usage);

	g_hash_table_destroy(laststop);
	g_hash_table_destroy(usage);
	g_object_unref(results);

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
