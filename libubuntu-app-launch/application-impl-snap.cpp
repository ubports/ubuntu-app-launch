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

#include "application-impl-snap.h"
#include "application-info-desktop.h"
#include "registry-impl.h"

namespace ubuntu
{
namespace app_launch
{
namespace app_impls
{

Snap::Snap(const AppID& appid, const std::shared_ptr<Registry>& registry)
    : Base(registry)
{
}

std::list<std::shared_ptr<Application>> Snap::list(const std::shared_ptr<Registry>& registry)
{
    std::list<std::shared_ptr<Application>> apps;

    for (auto interface : {"unity7", "unity8"})
    {
        for (auto id : registry->impl->snapdInfo.appsForInterface(interface))
        {
            auto app = std::make_shared<Snap>(id, registry);
            apps.push_back(app);
        }
    }

    return apps;
}

AppID Snap::appId()
{
    return _appid;
}

std::shared_ptr<Application::Info> Snap::info()
{
    return {};
}

std::vector<std::shared_ptr<Application::Instance>> Snap::instances()
{
    return {};
}

std::shared_ptr<Application::Instance> Snap::launch(const std::vector<Application::URL>& urls)
{
    return {};
}

std::shared_ptr<Application::Instance> Snap::launchTest(const std::vector<Application::URL>& urls)
{
    return {};
}

}  // namespace app_impls
}  // namespace app_launch
}  // namespace ubuntu
