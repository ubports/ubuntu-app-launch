/*
 * Copyright © 2015 Canonical Ltd.
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

#include "libubuntu-app-launch/ubuntu-app-launch.h"

int
main (int argc, char * argv[])
{
	GList * pids;
	GList * iter;

	if (argc != 2) {
		g_printerr("Usage: %s <app id>\n", argv[0]);
		return 1;
	}

	pids = ubuntu_app_launch_get_pids(argv[1]);
	if (pids == NULL) {
		return 1;
	}

	for (iter = pids; iter != NULL; iter = g_list_next(iter)) {
		g_print("%d\n", (GPid)GPOINTER_TO_INT(iter->data));
	}

	g_list_free(pids);

	return 0;
}

