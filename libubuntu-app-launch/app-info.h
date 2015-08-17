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

#pragma once

#include <glib.h>

gboolean app_info_legacy (const gchar * appid, gchar ** appdir, gchar ** appdesktop);
gboolean app_info_libertine (const gchar * appid, gchar ** appdir, gchar ** appdesktop);
gboolean app_info_click (const gchar * appid, gchar ** appdir, gchar ** appdesktop);

gchar * click_triplet_to_app_id (const gchar * pkg, const gchar * app, const gchar * ver);
gchar * libertine_triplet_to_app_id (const gchar * pkg, const gchar * app, const gchar * ver);
