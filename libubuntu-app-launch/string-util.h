/*
 * Copyright © 2017 Canonical Ltd.
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
 *     Pete Woods <pete.woods@canonical.com>
 */

#include <glib.h>
#include <memory>

#pragma once
#pragma GCC visibility push(default)

namespace ubuntu
{
namespace app_launch
{

typedef std::unique_ptr<gchar, decltype(&g_free)> GCharUPtr;

inline GCharUPtr unique_gchar(gchar* s)
{
    return GCharUPtr(s, &g_free);
}

typedef std::unique_ptr<gchar*, decltype(&g_strfreev)> GCharVUPtr;

inline GCharVUPtr unique_gcharv(gchar** s)
{
    return GCharVUPtr(s, &g_strfreev);
}

}  // namespace app_launch
}  // namespace ubuntu

#pragma GCC visibility pop
