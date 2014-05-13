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
#include <upstart-app-launch.h>

#ifndef __UBUNTU_APP_OBJECT_H__
#define __UBUNTU_APP_OBJECT_H__ 1

#pragma GCC visibility push(default)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _UbuntuAppObject UbuntuAppObject;

UbuntuAppObject * ubuntu_app_object_create (const gchar *                     pkg,
                                            const gchar *                     app,
                                            const gchar *                     version);

void ubuntu_app_object_free (UbuntuAppObject * obj);

gboolean   ubuntu_app_object_start         (UbuntuAppObject * obj,
                                            const gchar * const *             uris);

gboolean   ubuntu_app_object_stop         (UbuntuAppObject * obj);

gchar *   ubuntu_app_object_log_path         (UbuntuAppObject * obj);

GPid   ubuntu_app_object_primary_pid         (UbuntuAppObject * obj);

gboolean   ubuntu_app_object_has_pid             (UbuntuAppObject * obj,
GPid                              pid);

const gchar *   ubuntu_app_object_app_id         (UbuntuAppObject * obj);

#ifdef __cplusplus
}
#endif

#pragma GCC visibility pop

#endif /* __UBUNTU_APP_OBJECT_H__ */
