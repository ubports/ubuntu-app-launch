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

#include "app-store-libertine.h"
#include "application-impl-libertine.h"
#include "string-util.h"

#include "libertine.h"

namespace ubuntu
{
namespace app_launch
{
namespace app_store
{

Libertine::Libertine()
{
}

Libertine::~Libertine()
{
}

/** Checks the AppID by making sure the version is "0.0" and then
    calling verifyAppname() to check the rest.

    \param appid AppID to check
    \param registry persistent connections to use
*/
bool Libertine::hasAppId(const AppID& appid, const std::shared_ptr<Registry>& registry)
{
    try
    {
        if (appid.version.value() != "0.0")
        {
            return false;
        }

        return verifyAppname(appid.package, appid.appname, registry);
    }
    catch (std::runtime_error& e)
    {
        return false;
    }
}

/** Verify a package name by getting the list of containers from
    liblibertine and ensuring it is in that list.

    \param package Container name
    \param registry persistent connections to use
*/
bool Libertine::verifyPackage(const AppID::Package& package, const std::shared_ptr<Registry>& registry)
{
    auto containers = unique_gcharv(libertine_list_containers());

    for (int i = 0; containers.get()[i] != nullptr; i++)
    {
        auto container = containers.get()[i];
        if (container == package.value())
        {
            return true;
        }
    }

    return false;
}

/** Gets the list of applications from the container using liblibertine
    and see if @appname is in that list.

    \param package Container name
    \param appname Application name to look for
    \param registry persistent connections to use
*/
bool Libertine::verifyAppname(const AppID::Package& package,
                              const AppID::AppName& appname,
                              const std::shared_ptr<Registry>& registry)
{
    auto apps = unique_gcharv(libertine_list_apps_for_container(package.value().c_str()));

    for (int i = 0; apps.get()[i] != nullptr; i++)
    {
        auto appid = AppID::parse(apps.get()[i]);
        if (appid.appname.value() == appname.value())
        {
            return true;
        }
    }

    return false;
}

/** We don't really have a way to implement this for Libertine, any
    search wouldn't really make sense. We just throw an error.

    \param package Container name
    \param card Application search paths
    \param registry persistent connections to use
*/
AppID::AppName Libertine::findAppname(const AppID::Package& package,
                                      AppID::ApplicationWildcard card,
                                      const std::shared_ptr<Registry>& registry)
{
    throw std::runtime_error("Legacy apps can't be discovered by package");
}

/** Function to return "0.0"

    \param package Container name (unused)
    \param appname Application name (unused)
    \param registry persistent connections to use (unused)
*/
AppID::Version Libertine::findVersion(const AppID::Package& package,
                                      const AppID::AppName& appname,
                                      const std::shared_ptr<Registry>& registry)
{
    return AppID::Version::from_raw("0.0");
}

std::list<std::shared_ptr<Application>> Libertine::list(const std::shared_ptr<Registry>& registry)
{
    std::list<std::shared_ptr<Application>> applist;

    auto containers = unique_gcharv(libertine_list_containers());

    for (int i = 0; containers.get()[i] != nullptr; i++)
    {
        auto container = containers.get()[i];
        auto apps = unique_gcharv(libertine_list_apps_for_container(container));

        for (int j = 0; apps.get()[j] != nullptr; j++)
        {
            try
            {
                auto appid = AppID::parse(apps.get()[j]);
                auto sapp = std::make_shared<app_impls::Libertine>(appid.package, appid.appname, registry->impl);
                applist.emplace_back(sapp);
            }
            catch (std::runtime_error& e)
            {
                g_debug("Unable to create application for libertine appname '%s': %s", apps.get()[j], e.what());
            }
        }
    }

    return applist;
}

std::shared_ptr<app_impls::Base> Libertine::create(const AppID& appid, const std::shared_ptr<Registry::Impl>& registry)
{
    return std::make_shared<app_impls::Libertine>(appid.package, appid.appname, registry);
}

}  // namespace app_store
}  // namespace app_launch
}  // namespace ubuntu
