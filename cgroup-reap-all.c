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

#include "helpers.h"

int kill (pid_t pid, int signal);
pid_t getpgid (pid_t);

int
main (int argc, char * argv[])
{
	/* Break off a new process group */
	setpgid(0, 0);

	GDBusConnection * cgmanager = cgroup_manager_connection();
	g_return_val_if_fail(cgmanager != NULL, -1);

	GPid selfpid = getpid();
	GPid parentpid = getppid();

	/* We're gonna try to kill things forever, literally. It's important
	   enough that we can't consider failure an option. */
	gboolean killed = TRUE;
	while (killed) {
		GList * pidlist = pids_from_cgroup(cgmanager, NULL, NULL);
		GList * head;

		killed = FALSE;

		for (head = pidlist; head != NULL; head = g_list_next(head)) {
			GPid pid = GPOINTER_TO_INT(head->data);

			/* We don't want to kill ourselves, or if we're being executed by
			   a script, that script, either. We also don't want things in our
			   process group which we forked at the opening */
			if (pid != selfpid && pid != parentpid && getpgid(pid) != selfpid) {
				g_debug("Killing pid: %d", pid);
				kill(pid, SIGKILL);
				killed = TRUE;
			}
		}

		g_list_free(pidlist);
	}

	g_clear_object(&cgmanager);

	return 0;
}
