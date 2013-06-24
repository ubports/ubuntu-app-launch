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


#include <zeitgeist.h>

int
main (int argc, char * argv[])
{
	if (argc != 3 || (g_strcmp0(argv[1], "open") != 0 && g_strcmp0(argv[1], "close") != 0)) {
		g_printerr("Usage: %s [open|close] <application url>\n", argv[0]);
		return 1;
	}

	ZeitgeistLog * log = zeitgeist_log_get_default();

	ZeitgeistEvent * event = zeitgeist_event_new();
	zeitgeist_event_set_actor(event, "application://upstart-app-launch.desktop");
	if (g_strcmp0(argv[1], "open") == 0) {
		zeitgeist_event_set_interpretation(event, ZEITGEIST_ZG_ACCESS_EVENT);
	} else {
		zeitgeist_event_set_interpretation(event, ZEITGEIST_ZG_LEAVE_EVENT);
	}
	zeitgeist_event_set_manifestation(event, ZEITGEIST_ZG_USER_ACTIVITY);

	ZeitgeistSubject * subject = zeitgeist_subject_new();
	zeitgeist_subject_set_interpretation(subject, ZEITGEIST_NFO_SOFTWARE);
	zeitgeist_subject_set_manifestation(subject, ZEITGEIST_NFO_SOFTWARE_ITEM);
	zeitgeist_subject_set_mimetype(subject, "application/x-desktop");
	zeitgeist_subject_set_uri(subject, argv[2]);

	zeitgeist_event_add_subject(event, subject);

	GError * error = NULL;
	zeitgeist_log_insert_event_no_reply(log, event, &error);

	if (error != NULL) {
		g_printerr("Unable to log Zeitgeist event: %s", error->message);
		g_error_free(error);
		return 1;
	}

	return 0;
}
