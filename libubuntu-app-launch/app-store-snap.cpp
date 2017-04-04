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
 *     Ted Gould <ted.gould@canonical.com>
 */

#include "app-store-snap.h"
#include "application-impl-snap.h"
#include "registry-impl.h"

#include <regex>

namespace ubuntu
{
namespace app_launch
{
namespace app_store
{

/************************
 ** Interface Lists
 ************************/

/** All the interfaces that we run XMir for by default */
const std::set<std::string> X11_INTERFACES{"unity7", "x11"};
/** The interface to indicate direct Mir support */
const std::string MIR_INTERFACE{"mir"};
/** The interface to indicate Ubuntu lifecycle support */
const std::string LIFECYCLE_INTERFACE{"unity8"};
/** Snappy has more restrictive appnames than everyone else */
const std::regex appnameRegex{"^[a-zA-Z0-9](?:-?[a-zA-Z0-9])*$"};

Snap::Snap(const std::shared_ptr<Registry::Impl>& registry)
    : Base(registry)
{
}

Snap::~Snap()
{
}

/** Checks if an AppID could be a snap. Note it doesn't look for a desktop
    file just the package, app and version. This is done to make the lookup
    quickly, as this function can be used to select which backend to use
    and we want to reject quickly.

    \param appid Application ID of the snap
    \param registry Registry to use for persistent connections
*/
bool Snap::hasAppId(const AppID& appId)
{
    if (appId.package.value().empty() || appId.version.value().empty())
    {
        return false;
    }

    if (!std::regex_match(appId.appname.value(), appnameRegex))
    {
        return false;
    }

    auto pkginfo = getReg()->snapdInfo.pkgInfo(appId.package);
    return app_impls::Snap::checkPkgInfo(pkginfo, appId);
}

/** Look to see if a package is a valid Snap package name

    \param package Package name
    \param registry Registry to use for persistent connections
*/
bool Snap::verifyPackage(const AppID::Package& package)
{
    try
    {
        auto pkgInfo = getReg()->snapdInfo.pkgInfo(package);
        return pkgInfo != nullptr;
    }
    catch (std::runtime_error& e)
    {
        return false;
    }
}

/** Look to see if an appname is a valid for a Snap package

    \param package Package name
    \param appname Command name
    \param registry Registry to use for persistent connections
*/
bool Snap::verifyAppname(const AppID::Package& package, const AppID::AppName& appname)
{
    if (!std::regex_match(appname.value(), appnameRegex))
    {
        return false;
    }

    auto pkgInfo = getReg()->snapdInfo.pkgInfo(package);

    if (!pkgInfo)
    {
        return false;
    }

    return pkgInfo->appnames.find(appname) != pkgInfo->appnames.end();
}

/** Look for an application name on a Snap package based on a
    wildcard type.

    \param package Package name
    \param card Wildcard to use for finding the appname
    \param registry Registry to use for persistent connections
*/
AppID::AppName Snap::findAppname(const AppID::Package& package, AppID::ApplicationWildcard card)
{
    auto pkgInfo = getReg()->snapdInfo.pkgInfo(package);

    if (!pkgInfo)
    {
        throw std::runtime_error("Packge '" + package.value() + "' doesn't have valid info.");
    }

    if (pkgInfo->appnames.empty())
    {
        throw std::runtime_error("No apps in package '" + package.value() + "' to find");
    }

    switch (card)
    {
        case AppID::ApplicationWildcard::FIRST_LISTED:
            return AppID::AppName::from_raw(*pkgInfo->appnames.begin());
        case AppID::ApplicationWildcard::LAST_LISTED:
            return AppID::AppName::from_raw(*pkgInfo->appnames.rbegin());
        case AppID::ApplicationWildcard::ONLY_LISTED:
            if (pkgInfo->appnames.size() != 1)
            {
                throw std::runtime_error("More than a single app in package '" + package.value() +
                                         "' when requested to find only app");
            }
            return AppID::AppName::from_raw(*pkgInfo->appnames.begin());
    }

    throw std::logic_error("Got a value of the app wildcard enum that can't exist");
}

/** Look for a version of a Snap package

    \param package Package name
    \param appname Not used for snaps
    \param registry Registry to use for persistent connections
*/
AppID::Version Snap::findVersion(const AppID::Package& package, const AppID::AppName& appname)
{
    auto pkgInfo = getReg()->snapdInfo.pkgInfo(package);
    if (pkgInfo)
    {
        return AppID::Version::from_raw(pkgInfo->revision);
    }
    else
    {
        return AppID::Version::from_raw({});
    }
}

/** Operator to compare apps for our sets */
struct appcompare
{
    bool operator()(const std::shared_ptr<Application>& a, const std::shared_ptr<Application>& b) const
    {
        return a->appId() < b->appId();
    }
};

/** Lists all the Snappy apps that are using one of our supported interfaces.
    Also makes sure they're valid.

    \param registry Registry to use for persistent connections
*/
std::list<std::shared_ptr<Application>> Snap::list()
{
    std::set<std::shared_ptr<Application>, appcompare> apps;
    auto reg = getReg();

    auto lifecycleApps = reg->snapdInfo.appsForInterface(LIFECYCLE_INTERFACE);

    auto lifecycleForApp = [&](const AppID& appID) {
        auto iterator = lifecycleApps.find(appID);
        if (iterator == lifecycleApps.end())
        {
            return Application::Info::UbuntuLifecycle::from_raw(false);
        }
        else
        {
            return Application::Info::UbuntuLifecycle::from_raw(true);
        }
    };

    auto addAppsForInterface = [&](const std::string& interface, app_info::Desktop::XMirEnable xMirEnable) {
        for (const auto& id : reg->snapdInfo.appsForInterface(interface))
        {
            auto interfaceInfo = std::make_tuple(xMirEnable, lifecycleForApp(id));
            try
            {
                auto app = std::make_shared<app_impls::Snap>(id, reg, interfaceInfo);
                apps.emplace(app);
            }
            catch (std::runtime_error& e)
            {
                g_debug("Unable to make Snap object for '%s': %s", std::string(id).c_str(), e.what());
            }
        }
    };

    addAppsForInterface(MIR_INTERFACE, app_info::Desktop::XMirEnable::from_raw(false));

    /* If an app has both, this will get rejected */
    for (const auto& interface : X11_INTERFACES)
    {
        addAppsForInterface(interface, app_info::Desktop::XMirEnable::from_raw(true));
    }

    return std::list<std::shared_ptr<Application>>(apps.begin(), apps.end());
}

std::shared_ptr<app_impls::Base> Snap::create(const AppID& appid)
{
    return std::make_shared<app_impls::Snap>(appid, getReg());
}

}  // namespace app_store
}  // namespace app_launch
}  // namespace ubuntu
