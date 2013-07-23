
#include "helpers.h"

/* Take an app ID and validate it and then break it up
   and spit it out.  These are newly allocated strings */
gboolean
app_id_to_triplet (const gchar * app_id, gchar ** package, gchar ** application, gchar ** version)
{
	/* 'Parse' the App ID */
	gchar ** app_id_segments = g_strsplit(app_id, "_", 4);
	if (g_strv_length(app_id_segments) != 3) {
		g_warning("Unable to parse Application ID: %s", app_id);
		g_strfreev(app_id_segments);
		return FALSE;
	}

	if (package != NULL) {
		*package = app_id_segments[0];
	} else {
		g_free(app_id_segments[0]);
	}

	if (application != NULL) {
		*application = app_id_segments[1];
	} else {
		g_free(app_id_segments[1]);
	}

	if (version != NULL) {
		*version = app_id_segments[2];
	} else {
		g_free(app_id_segments[2]);
	}

	g_free(app_id_segments);
	return TRUE;
}

/* Take a manifest, parse it, find the application and
   and then return the path to the desktop file */
gchar *
manifest_to_desktop (const gchar * app_dir, const gchar * app_id)
{

	return NULL;
}

/* Take a desktop file, make sure that it makes sense and
   then return the exec line */
gchar *
desktop_to_exec (GKeyFile * desktop_file)
{

	return NULL;
}

