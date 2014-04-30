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

#include "ubuntu-app-launch.h"
#include "ubuntu-app-launch-mock.h"

static GPid primary_pid = 0;
static gchar * primary_pid_appid = NULL;

GPid
ubuntu_app_launch_get_primary_pid (const gchar * appid)
{
	g_free(primary_pid_appid);
	primary_pid_appid = g_strdup(appid);
	return primary_pid;
}

void
ubuntu_app_launch_mock_set_primary_pid (GPid pid)
{
	primary_pid = pid;
	return;
}
