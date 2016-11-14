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

#include "jobs-base.h"
#include <gio/gio.h>
#include <map>

namespace ubuntu
{
namespace app_launch
{
namespace jobs
{
namespace manager
{

class Upstart : public Base
{
public:
    Upstart(std::shared_ptr<Registry> registry);
    virtual ~Upstart();

    virtual std::shared_ptr<Application::Instance> launch(
        const AppID& appId,
        const std::string& job,
        const std::string& instance,
        const std::vector<Application::URL>& urls,
        launchMode mode,
        std::function<std::list<std::pair<std::string, std::string>>(void)>& getenv) override;
    virtual std::shared_ptr<Application::Instance> existing(const AppID& appId,
                                                            const std::string& job,
                                                            const std::string& instance,
                                                            const std::vector<Application::URL>& urls) override;

    virtual std::list<std::shared_ptr<Application>> runningApps() override;

    virtual std::vector<std::shared_ptr<instance::Base>> instances(const AppID& appID, const std::string& job) override;

    /* Signals to apps */
    virtual core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>>& appStarted() override;
    virtual core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>>& appStopped() override;
    virtual core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>, Registry::FailureType>&
        appFailed() override;

    std::vector<pid_t> pidsFromCgroup(const std::string& jobpath);

    std::list<std::string> upstartInstancesForJob(const std::string& job);
    std::string upstartJobPath(const std::string& job);

private:
    void initCGManager();

    std::shared_ptr<GDBusConnection> cgManager_;

    /** Getting the Upstart job path is relatively expensive in
        that it requires a DBus call. Worth keeping a cache of. */
    std::map<std::string, std::string> upstartJobPathCache_;

    guint handle_appStarted{0}; /**< GDBus signal watcher handle for app started signal */
    guint handle_appStopped{0}; /**< GDBus signal watcher handle for app stopped signal */
    guint handle_appFailed{0};  /**< GDBus signal watcher handle for app failed signal */

    std::once_flag flag_appStarted; /**< Variable to track to see if signal handlers are installed for application
                                       started */
    std::once_flag flag_appStopped; /**< Variable to track to see if signal handlers are installed for application
                                       stopped */
    std::once_flag
        flag_appFailed; /**< Variable to track to see if signal handlers are installed for application failed */

    void upstartEventEmitted(core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>>& signal,
                             std::shared_ptr<GVariant> params,
                             const std::shared_ptr<Registry>& reg);
};

}  // namespace manager
}  // namespace jobs
}  // namespace app_launch
}  // namespace ubuntu
