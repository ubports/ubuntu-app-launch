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
#include <ubuntu-app-launch.h>

#ifndef __UBUNTU_APP_OBJECT_H__
#define __UBUNTU_APP_OBJECT_H__ 1

#pragma GCC visibility push(default)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * UbuntuAppLaunchObject: (class) (ref-func ubuntu_app_object_ref) (unref-func ubuntu_app_object_unref)
 * 
 * Represents an application that is running or stopped in the system.
 * At creation time the app id is built.
 */
typedef struct _UbuntuAppLaunchObject UbuntuAppLaunchObject;

/**
 * ubuntu_app_launch_object_create: (constructor)
 *
 */
UbuntuAppLaunchObject * ubuntu_app_launch_object_create       (const gchar * pkg,
                                                  const gchar * app,
                                                  const gchar * version);

void              ubuntu_app_object_ref          (UbuntuAppLaunchObject * obj);
void              ubuntu_app_object_unref        (UbuntuAppLaunchObject * obj);

gboolean          ubuntu_app_object_start        (UbuntuAppLaunchObject *      obj,
                                                  const gchar * const *  uris);

gboolean          ubuntu_app_object_stop         (UbuntuAppLaunchObject * obj);

gchar *           ubuntu_app_object_log_path     (UbuntuAppLaunchObject * obj);

GPid              ubuntu_app_object_primary_pid  (UbuntuAppLaunchObject * obj);

gboolean          ubuntu_app_object_has_pid      (UbuntuAppLaunchObject * obj,
                                                  GPid              pid);

const gchar *     ubuntu_app_object_app_id       (UbuntuAppLaunchObject * obj);

#ifdef __cplusplus
}
#endif

#pragma GCC visibility pop

#endif /* __UBUNTU_APP_OBJECT_H__ */
