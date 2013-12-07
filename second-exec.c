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

#include "libupstart-app-launch/second-exec-core.h"

int
main (int argc, char * argv[])
{
	if (argc != 1) {
		g_error("Should be called as: %s", argv[0]);
		return 1;
	}

	const gchar * appid = g_getenv("APP_ID");
	const gchar * appuris = g_getenv("APP_URIS");

	g_setenv("LTTNG_UST_REGISTER_TIMEOUT", "0", FALSE); /* Set to zero if not set */

	if (second_exec(appid, appuris)) {
		return 0;
	} else {
		return 1;
	}
}
