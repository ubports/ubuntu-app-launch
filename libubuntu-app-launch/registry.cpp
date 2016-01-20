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

#include "registry.h"
#include "registry-impl.h"

#include "application-impl-click.h"
#include "application-impl-legacy.h"
#include "application-impl-libertine.h"

#include "helper-impl-click.h"

namespace Ubuntu
{
namespace AppLaunch
{

Registry::Registry ()
{
    impl = std::unique_ptr<Impl>(new Impl(this));
}

Registry::~Registry ()
{

}

std::list<std::shared_ptr<Application>>
                                     Registry::runningApps(std::shared_ptr<Registry> connection)
{
    return connection->impl->thread.executeOnThread<std::list<std::shared_ptr<Application>>>([connection]() ->
                                                                                             std::list<std::shared_ptr<Application>>
    {
        auto strv = ubuntu_app_launch_list_running_apps();
        if (strv == nullptr)
        {
            return {};
        }

        std::list<std::shared_ptr<Application>> list;
        for (int i = 0; strv[i] != nullptr; i++)
        {
            auto appid = AppID::parse(strv[i]);
            auto app = Application::create(appid, connection);
            list.push_back(app);
        }

        g_strfreev(strv);

        return list;
    });
}

std::list<std::shared_ptr<Application>>
                                     Registry::installedApps(std::shared_ptr<Registry> connection)
{
    std::list<std::shared_ptr<Application>> list;

    list.splice(list.begin(), AppImpls::Click::list(connection));
    list.splice(list.begin(), AppImpls::Legacy::list(connection));
    list.splice(list.begin(), AppImpls::Libertine::list(connection));

    return list;
}

std::list<std::shared_ptr<Helper>>
                                Registry::runningHelpers (Helper::Type type, std::shared_ptr<Registry> connection)
{
    std::list<std::shared_ptr<Helper>> list;

    list.splice(list.begin(), HelperImpls::Click::running(type, connection));

    return list;
}

std::shared_ptr<Registry> defaultRegistry;
std::shared_ptr<Registry>
Registry::getDefault()
{
    if (!defaultRegistry)
    {
        defaultRegistry = std::make_shared<Registry>();
    }

    return defaultRegistry;
}

void
Registry::setManager (Manager* manager)
{
    impl->setManager(manager);
}

void
Registry::clearManager ()
{
    impl->clearManager();
}

}; // namespace AppLaunch
}; // namespace Ubuntu
