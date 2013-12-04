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

#include "helpers.h"
#include "desktop-single-trace.h"

int
main (int argc, char * argv[])
{
	/* Nothing is single instance yet */
	if (argc != 2) {
		g_error("Should be called as: %s <app_id>", argv[0]);
		return 1;
	}

	tracepoint(upstart_app_launch, desktop_single_start);

	GKeyFile * keyfile = keyfile_for_appid(argv[1], NULL);

	if (keyfile == NULL) {
		g_error("Unable to find keyfile for application '%s'", argv[0]);
		return 1;
	}

	tracepoint(upstart_app_launch, desktop_single_found);

	gboolean singleinstance = FALSE;

	if (g_key_file_has_key(keyfile, "Desktop Entry", "X-Ubuntu-Single-Instance", NULL)) {
		GError * error = NULL;

		singleinstance = g_key_file_get_boolean(keyfile, "Desktop Entry", "X-Ubuntu-Single-Instance", &error);

		if (error != NULL) {
			g_warning("Unable to get single instance key for app '%s': %s", argv[1], error->message);
			g_error_free(error);
			/* Ensure that if we got an error, we assume standard case */
			singleinstance = FALSE;
		}
	}
	
	g_key_file_free(keyfile);

	tracepoint(upstart_app_launch, desktop_single_finished);

	if (singleinstance) {
		return 0;
	} else {
		return 1;
	}
}
