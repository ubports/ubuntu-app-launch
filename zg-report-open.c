
#include <zeitgeist.h>

int
main (int argc, char * argv[])
{
	if (argc != 2) {
		g_printerr("Usage: %s <application url>\n", argv[0]);
		return 1;
	}

	ZeitgeistLog * log = zeitgeist_log_get_default();

	ZeitgeistEvent * event = zeitgeist_event_new();
	zeitgeist_event_set_actor(event, "application://upstart-app-launch.desktop");
	zeitgeist_event_set_interpretation(event, ZEITGEIST_ZG_ACCESS_EVENT);
	zeitgeist_event_set_manifestation(event, ZEITGEIST_ZG_USER_ACTIVITY);

	ZeitgeistSubject * subject = zeitgeist_subject_new();
	zeitgeist_subject_set_interpretation(subject, ZEITGEIST_NFO_SOFTWARE);
	zeitgeist_subject_set_manifestation(subject, ZEITGEIST_NFO_SOFTWARE_ITEM);
	zeitgeist_subject_set_mimetype(subject, "application/x-desktop");
	zeitgeist_subject_set_uri(subject, argv[1]);

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
