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

#include <algorithm>
#include <numeric>
#include <regex>

#include "registry-impl.h"
#include "registry.h"

#include "application-impl-click.h"
#include "application-impl-legacy.h"
#include "application-impl-libertine.h"
#include "application-impl-snap.h"

#include "helper-impl-click.h"

namespace ubuntu
{
namespace app_launch
{

Registry::Registry()
{
    impl = std::unique_ptr<Impl>(new Impl(this));
}

Registry::~Registry()
{
}

std::list<std::shared_ptr<Application>> Registry::runningApps(std::shared_ptr<Registry> connection)
{
    std::list<std::string> instances;

    /* Get all the legacy instances */
    instances.splice(instances.begin(), connection->impl->upstartInstancesForJob("application-legacy"));
    /* Get all the snap instances */
    instances.splice(instances.begin(), connection->impl->upstartInstancesForJob("application-snap"));

    /* Remove the instance ID */
    std::transform(instances.begin(), instances.end(), instances.begin(), [](std::string &instancename) -> std::string {
        static const std::regex instanceregex("^(.*)-[0-9]*$");
        std::smatch match;
        if (std::regex_match(instancename, match, instanceregex))
        {
            return match[1].str();
        }
        else
        {
            g_warning("Unable to match instance name: %s", instancename.c_str());
            return {};
        }
    });

    /* Deduplicate Set */
    std::set<std::string> instanceset;
    for (auto instance : instances)
    {
        if (!instance.empty())
            instanceset.insert(instance);
    }

    /* Add in the click instances */
    for (auto instance : connection->impl->upstartInstancesForJob("application-click"))
    {
        instanceset.insert(instance);
    }

    g_debug("Overall there are %d instances: %s", int(instanceset.size()),
            std::accumulate(instanceset.begin(), instanceset.end(), std::string{},
                            [](const std::string &instr, std::string instance) {
                                return instr.empty() ? instance : instr + ", " + instance;
                            })
                .c_str());

    /* Convert to Applications */
    std::list<std::shared_ptr<Application>> apps;
    for (auto instance : instanceset)
    {
        auto appid = AppID::find(connection, instance);
        auto app = Application::create(appid, connection);
        apps.push_back(app);
    }

    return apps;
}

std::list<std::shared_ptr<Application>> Registry::installedApps(std::shared_ptr<Registry> connection)
{
    std::list<std::shared_ptr<Application>> list;

    list.splice(list.begin(), app_impls::Click::list(connection));
    list.splice(list.begin(), app_impls::Legacy::list(connection));
    list.splice(list.begin(), app_impls::Libertine::list(connection));
    list.splice(list.begin(), app_impls::Snap::list(connection));

    return list;
}

std::list<std::shared_ptr<Helper>> Registry::runningHelpers(Helper::Type type, std::shared_ptr<Registry> connection)
{
    std::list<std::shared_ptr<Helper>> list;

    list.splice(list.begin(), helper_impls::Click::running(type, connection));

    return list;
}

std::shared_ptr<Registry> defaultRegistry;
std::shared_ptr<Registry> Registry::getDefault()
{
    if (!defaultRegistry)
    {
        defaultRegistry = std::make_shared<Registry>();
    }

    return defaultRegistry;
}

void Registry::clearDefault()
{
    defaultRegistry.reset();
}

}  // namespace app_launch
}  // namespace ubuntu
