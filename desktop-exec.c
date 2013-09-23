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

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <glib.h>
#include <gio/gio.h>

#include "helpers.h"

int
main (int argc, char * argv[])
{
	if (argc != 1) {
		g_error("Should be called as: %s", argv[0]);
		return 1;
	}

	const gchar * app_id = g_getenv("APP_ID");

	if (app_id == NULL) {
		g_error("No APP_ID environment variable defined");
		return 1;
	}

	GKeyFile * keyfile = keyfile_for_appid(app_id);

	if (keyfile == NULL) {
		g_error("Unable to find keyfile for application '%s'", app_id);
		return 1;
	}

	gchar * execline = desktop_to_exec(keyfile, app_id);
	g_return_val_if_fail(execline != NULL, 1);
	set_upstart_variable("APP_EXEC", execline);
	g_free(execline);

	gchar * apparmor = g_key_file_get_string(keyfile, "Desktop Entry", "XCanonicalAppArmorProfile", NULL);
	if (apparmor != NULL) {
		set_upstart_variable("APP_EXEC_POLICY", apparmor);
		g_free(apparmor);
	} else {
		set_upstart_variable("APP_EXEC_POLICY", "unconfined");
	}

	g_key_file_free(keyfile);

	return 0;
}
