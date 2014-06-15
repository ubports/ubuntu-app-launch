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
#include "helpers.h"

int kill (pid_t pid, int signal);

int
main (int argc, char * argv[])
{
	const gchar * appid = g_getenv("APP_ID");
	/* TODO: Use something besides App ID */

	if (appid == NULL) {
		g_warning("Unable to get app id in cgroup reaper");
		return 1;
	}

	/* We're gonna try to kill things forever, litterally. It's important
	   enough that we can't consider failure and option. */
	GList * pidlist = NULL;
	while ((pidlist = pids_from_cgroup(appid)) != NULL) {
		GList * head;

		for (head = pidlist; head != NULL; head = g_list_next(head)) {
			GPid pid = GPOINTER_TO_INT(head->data);
			kill(pid, SIGKILL);
		}

		g_list_free(pidlist);
	}

	return 0;
}
