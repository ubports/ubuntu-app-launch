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
#include <sys/apparmor.h>

#include "helpers.h"

int
main (int argc, char * argv[])
{
	if (argc != 2 && argc != 3) {
		g_error("Should be called as: %s <app_id> [uri list]", argv[0]);
		return 1;
	}

	GKeyFile * keyfile = keyfile_for_appid(argv[1]);

	if (keyfile == NULL) {
		g_error("Unable to find keyfile for application '%s'", argv[0]);
		return 1;
	}

	gchar * execline = g_key_file_get_string(keyfile, "Desktop Entry", "Exec", NULL);
	g_return_val_if_fail(execline != NULL, 1);

	GArray * newargv = desktop_exec_parse(execline, argc == 3 ? argv[2] : NULL);
	g_free(execline);

	if (newargv == NULL) {
		g_warning("Unable to parse exec line '%s'", execline);
		g_key_file_free(keyfile);
		return 1;
	}

	/* Surface flinger check */
	if (g_getenv("USING_SURFACE_FLINGER") != NULL) {
		gchar * sf = g_strdup_printf("--desktop_file_hint=%s/.local/share/applications/%s.desktop", g_get_home_dir(), argv[1]);
		g_array_append_val(newargv, sf);
	}

	gchar * apparmor = g_key_file_get_string(keyfile, "Desktop Entry", "XCanonicalAppArmorProfile", NULL);
	if (apparmor != NULL) {
		g_debug("Changing to app armor profile '%s' on exec", apparmor);
		aa_change_onexec(apparmor);
		g_free(apparmor);
	}

	g_key_file_free(keyfile);

	gchar ** nargv = (gchar**)g_array_free(newargv, FALSE);

	int execret = execvp(nargv[0], nargv);

	if (execret != 0) {
		g_warning("Unable to exec: %s", strerror(errno));
	}

	return execret;
}
