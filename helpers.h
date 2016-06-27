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

#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct _EnvHandle EnvHandle;

gboolean  app_id_to_triplet      (const gchar *   app_id,
                                  gchar **        package,
                                  gchar **        application,
                                  gchar **        version);
gchar *   manifest_to_desktop    (const gchar *   app_dir,
                                  const gchar *   app_id);
gchar *   desktop_to_exec        (GKeyFile *      desktop_file,
                                  const gchar *   from);
GArray *  desktop_exec_parse     (const gchar *   execline,
                                  const gchar *   uri_list);
GKeyFile * keyfile_for_appid     (const gchar *   appid,
                                  gchar * *       desktopfile);
void      set_confined_envvars   (EnvHandle *     handle,
                                  const gchar *   package,
                                  const gchar *   app_dir);

/* A handle to group environment setting */
EnvHandle * env_handle_start     (void);
void        env_handle_add       (EnvHandle *     handle,
                                  const gchar *   variable,
                                  const gchar *   value);
void        env_handle_finish    (EnvHandle *     handle);

typedef struct _handshake_t handshake_t;
handshake_t * starting_handshake_start   (const gchar *   app_id);
void      starting_handshake_wait        (handshake_t *   handshake);

GDBusConnection * cgroup_manager_connection (void);
void              cgroup_manager_unref (GDBusConnection * cgroup_manager);
GList *   pids_from_cgroup       (GDBusConnection * cgmanager,
                                  const gchar *   jobname,
                                  const gchar *   instancename);

gboolean   verify_keyfile        (GKeyFile *    inkeyfile,
                                  const gchar * desktop);

G_END_DECLS

