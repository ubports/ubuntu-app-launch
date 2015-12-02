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

#include "ubuntu-app-object.h"

struct _UbuntuAppLaunchObject {
	gchar * appid;
};

UbuntuAppLaunchObject *
ubuntu_app_launch_object_create (const gchar * pkg, const gchar * app, const gchar * version)
{
	gchar * appid = ubuntu_app_launch_triplet_to_app_id(pkg, app, version);
	if (appid == NULL)
		return NULL;

	UbuntuAppLaunchObject * obj = g_new0(UbuntuAppLaunchObject, 1);
	obj->appid = appid;

	return obj;
}

void
ubuntu_app_object_free (UbuntuAppLaunchObject * obj)
{
	g_return_if_fail(obj != NULL);
	g_free(obj->appid);
	g_free(obj);
}

gboolean
ubuntu_app_object_start (UbuntuAppLaunchObject * obj, const gchar * const * uris)
{
	g_return_val_if_fail(obj != NULL, FALSE);
	return ubuntu_app_launch_start_application(obj->appid, uris);
}

gboolean
ubuntu_app_object_stop (UbuntuAppLaunchObject * obj)
{
	g_return_val_if_fail(obj != NULL, FALSE);
	return ubuntu_app_launch_stop_application(obj->appid);
}

gchar *
ubuntu_app_object_log_path (UbuntuAppLaunchObject * obj)
{
	g_return_val_if_fail(obj != NULL, NULL);
	return ubuntu_app_launch_application_log_path(obj->appid);
}

GPid
ubuntu_app_object_primary_pid (UbuntuAppLaunchObject * obj)
{
	g_return_val_if_fail(obj != NULL, 0);
	return ubuntu_app_launch_get_primary_pid(obj->appid);
}

gboolean
ubuntu_app_object_has_pid (UbuntuAppLaunchObject * obj, GPid pid)
{
	g_return_val_if_fail(obj != NULL, FALSE);
	return ubuntu_app_launch_pid_in_app_id(pid, obj->appid);
}

const gchar *
ubuntu_app_object_app_id (UbuntuAppLaunchObject * obj)
{
	g_return_val_if_fail(obj != NULL, NULL);
	return obj->appid;
}
