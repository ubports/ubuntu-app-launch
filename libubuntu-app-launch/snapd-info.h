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

/** Class that implements the connection to Snapd allowing us to get info
    from it in a C++ friendly way. */
class Info
{
public:
    Info();
    virtual ~Info() = default;

    /** Information that we can get from snapd about a package */
    struct PkgInfo
    {
        std::string name;               /**< Name of the package */
        std::string version;            /**< Version string provided by the package */
        std::string revision;           /**< Numerical always incrementing revision of the package */
        std::string directory;          /**< Directory that the snap is uncompressed into */
        std::set<std::string> appnames; /**< List of appnames in the snap */
    };
    std::shared_ptr<PkgInfo> pkgInfo(const AppID::Package &package) const;

    std::set<AppID> appsForInterface(const std::string &interface) const;

    std::set<std::string> interfacesForAppId(const AppID &appid) const;

private:
    /** Path to the socket of snapd */
    std::string snapdSocket;
    /** Directory to use as the base for all snap packages when making paths. This
        can be overridden with UBUNTU_APP_LAUNCH_SNAP_BASEDIR */
    std::string snapBasedir;
    /** Result of a check at init to see if the socket is available. If
        not all functions will return null results. */
    bool snapdExists = false;

    std::shared_ptr<JsonNode> snapdJson(const std::string &endpoint) const;
    void forAllPlugs(std::function<void(JsonObject *plugobj)> plugfunc) const;
};

}  // namespace snapd
}  // namespace app_launch
}  // namespace ubuntu
