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

    virtual std::list<std::string> runningAppIds(const std::list<std::string>& jobs) override;

    virtual std::vector<std::shared_ptr<instance::Base>> instances(const AppID& appID, const std::string& job) override;

    /* Signals to apps */
    virtual core::Signal<const std::string&, const std::string&, const std::string&>& jobStarted() override;
    virtual core::Signal<const std::string&, const std::string&, const std::string&>& jobStopped() override;
    virtual core::Signal<const std::string&, const std::string&, const std::string&, Registry::FailureType>& jobFailed()
        override;

    std::vector<pid_t> pidsFromCgroup(const std::string& jobpath);

    std::list<std::string> upstartInstancesForJob(const std::string& job);
    std::string upstartJobPath(const std::string& job);

private:
    void initCGManager();

    std::shared_ptr<GDBusConnection> cgManager_;

    /** Getting the Upstart job path is relatively expensive in
        that it requires a DBus call. Worth keeping a cache of. */
    std::map<std::string, std::string> upstartJobPathCache_;

    core::Signal<const std::string&, const std::string&, const std::string&> sig_jobStarted;
    core::Signal<const std::string&, const std::string&, const std::string&> sig_jobStopped;
    core::Signal<const std::string&, const std::string&, const std::string&, Registry::FailureType> sig_jobFailed;

    guint handle_jobStarted{0}; /**< GDBus signal watcher handle for app started signal */
    guint handle_jobStopped{0}; /**< GDBus signal watcher handle for app stopped signal */
    guint handle_jobFailed{0};  /**< GDBus signal watcher handle for app failed signal */

    std::once_flag flag_jobStarted; /**< Variable to track to see if signal handlers are installed for application
                                       started */
    std::once_flag flag_jobStopped; /**< Variable to track to see if signal handlers are installed for application
                                       stopped */
    std::once_flag
        flag_jobFailed; /**< Variable to track to see if signal handlers are installed for application failed */

    void upstartEventEmitted(core::Signal<const std::string&, const std::string&, const std::string&>& signal,
                             std::shared_ptr<GVariant> params,
                             const std::shared_ptr<Registry>& reg);
};

}  // namespace manager
}  // namespace jobs
}  // namespace app_launch
}  // namespace ubuntu
