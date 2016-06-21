/*
 * Copyright © 2016 Canonical Ltd.
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

#include <list>
#include <memory>
#include <set>

#include <json-glib/json-glib.h>

#include "appid.h"

namespace ubuntu
{
namespace app_launch
{
namespace snapd
{

class Info
{
public:
    Info();
    virtual ~Info() = default;

    /** Information that we can get from snapd about a package */
    struct PkgInfo
    {
        std::string name;            /**< Name of the package */
        std::string version;         /**< Version string provided by the package */
        std::string revision;        /**< Numerical always incrementing revision of the package */
        std::list<std::string> apps; /**< The list of app names for the package */
        std::string directory;       /**< Directory that the snap is uncompressed into */
    };
    std::shared_ptr<PkgInfo> pkgInfo(const AppID &appid) const;

    std::set<AppID> appsForInterface(const std::string &interface) const;

private:
    std::string snapdSocket;
    bool snapdExists = false;

    std::shared_ptr<JsonNode> snapdJson(const std::string &endpoint) const;
};

}  // namespace snapd
}  // namespace app_launch
}  // namespace ubuntu
