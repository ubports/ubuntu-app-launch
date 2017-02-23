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

#include "application-impl-legacy.h"
#include "application-impl-libertine.h"
#include "application-impl-snap.h"

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

std::list<std::shared_ptr<Application>> Registry::runningApps(std::shared_ptr<Registry> registry)
{
    if (!registry->impl->jobs)
    {
        registry->impl->jobs = jobs::manager::Base::determineFactory(registry);
    }

    return registry->impl->jobs->runningApps();
}

std::list<std::shared_ptr<Application>> Registry::installedApps(std::shared_ptr<Registry> connection)
{
    std::list<std::shared_ptr<Application>> list;

    list.splice(list.begin(), app_impls::Legacy::list(connection));
    list.splice(list.begin(), app_impls::Libertine::list(connection));
    list.splice(list.begin(), app_impls::Snap::list(connection));

    return list;
}

std::list<std::shared_ptr<Helper>> Registry::runningHelpers(Helper::Type type, std::shared_ptr<Registry> registry)
{
    if (!registry->impl->jobs)
    {
        registry->impl->jobs = jobs::manager::Base::determineFactory(registry);
    }

    return registry->impl->jobs->runningHelpers(type);
}

/* Quick little helper to bundle up standard code */
inline void setJobs(const std::shared_ptr<Registry>& registry)
{
    if (!registry->impl->jobs)
    {
        registry->impl->jobs = jobs::manager::Base::determineFactory(registry);
    }
}

void Registry::setManager(const std::shared_ptr<Manager>& manager, const std::shared_ptr<Registry>& registry)
{
    setJobs(registry);
    registry->impl->jobs->setManager(manager);
}

void Registry::clearManager()
{
    if (!impl->jobs)
    {
        return;
    }

    impl->jobs->clearManager();
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

core::Signal<const std::shared_ptr<Application>&, const std::shared_ptr<Application::Instance>&>& Registry::appStarted(
    const std::shared_ptr<Registry>& reg)
{
    setJobs(reg);
    return reg->impl->jobs->appStarted();
}

core::Signal<const std::shared_ptr<Application>&, const std::shared_ptr<Application::Instance>&>& Registry::appStopped(
    const std::shared_ptr<Registry>& reg)
{
    setJobs(reg);
    return reg->impl->jobs->appStopped();
}

core::Signal<const std::shared_ptr<Application>&, const std::shared_ptr<Application::Instance>&, Registry::FailureType>&
    Registry::appFailed(const std::shared_ptr<Registry>& reg)
{
    setJobs(reg);
    return reg->impl->jobs->appFailed();
}

core::Signal<const std::shared_ptr<Application>&,
             const std::shared_ptr<Application::Instance>&,
             const std::vector<pid_t>&>&
    Registry::appPaused(const std::shared_ptr<Registry>& reg)
{
    setJobs(reg);
    return reg->impl->jobs->appPaused();
}

core::Signal<const std::shared_ptr<Application>&,
             const std::shared_ptr<Application::Instance>&,
             const std::vector<pid_t>&>&
    Registry::appResumed(const std::shared_ptr<Registry>& reg)
{
    setJobs(reg);
    return reg->impl->jobs->appResumed();
}

core::Signal<const std::shared_ptr<Helper>&, const std::shared_ptr<Helper::Instance>&>& Registry::helperStarted(
    Helper::Type type, const std::shared_ptr<Registry>& reg)
{
    setJobs(reg);
    return reg->impl->jobs->helperStarted(type);
}

core::Signal<const std::shared_ptr<Helper>&, const std::shared_ptr<Helper::Instance>&>& Registry::helperStopped(
    Helper::Type type, const std::shared_ptr<Registry>& reg)
{
    setJobs(reg);
    return reg->impl->jobs->helperStopped(type);
}

core::Signal<const std::shared_ptr<Helper>&, const std::shared_ptr<Helper::Instance>&, Registry::FailureType>&
    Registry::helperFailed(Helper::Type type, const std::shared_ptr<Registry>& reg)
{
    setJobs(reg);
    return reg->impl->jobs->helperFailed(type);
}

core::Signal<const std::shared_ptr<Application>&>& Registry::appInfoUpdated(const std::shared_ptr<Registry>& reg)
{
    return reg->impl->appInfoUpdated(reg);
}

}  // namespace app_launch
}  // namespace ubuntu
