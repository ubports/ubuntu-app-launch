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

#include "application.h"
#include "registry.h"

#include <core/signal.h>

namespace ubuntu
{
namespace app_launch
{
namespace jobs
{
namespace instance
{

class Base : public Application::Instance
{
public:
    Base(const AppID& appId,
         const std::string& job,
         const std::string& instance,
         const std::vector<Application::URL>& urls,
         const std::shared_ptr<Registry>& registry);
    virtual ~Base() = default;

    bool isRunning() override;
    bool hasPid(pid_t pid) override;
    void pause() override;
    void resume() override;

    /* OOM Functions */
    void setOomAdjustment(const oom::Score score) override;
    const oom::Score getOomAdjustment() override;

protected:
    /** Application ID */
    const AppID appId_;
    /** Upstart job name */
    const std::string job_;
    /** Instance ID environment value, empty if none */
    const std::string instance_;
    /** The URLs that this was launched for. Only valid on launched jobs, we
        should look at perhaps changing that. */
    std::vector<Application::URL> urls_;
    /** A link to the registry we're using for connections */
    std::shared_ptr<Registry> registry_;

private:
    std::vector<pid_t> forAllPids(std::function<void(pid_t)> eachPid);
    void signalToPid(pid_t pid, int signal);
    std::string pidToOomPath(pid_t pid);
    void oomValueToPid(pid_t pid, const oom::Score oomvalue);
    void oomValueToPidHelper(pid_t pid, const oom::Score oomvalue);
    void pidListToDbus(const std::vector<pid_t>& pids, const std::string& signal);
};

}  // namespace instance

namespace manager
{

/** Flag for whether we should include the testing environment variables */
enum class launchMode
{
    STANDARD, /**< Standard variable set */
    TEST      /**< Include testing environment vars */
};

class Base
{
public:
    Base(const std::shared_ptr<Registry>& registry);
    virtual ~Base() = default;

    virtual std::shared_ptr<Application::Instance> launch(
        const AppID& appId,
        const std::string& job,
        const std::string& instance,
        const std::vector<Application::URL>& urls,
        launchMode mode,
        std::function<std::list<std::pair<std::string, std::string>>(void)>& getenv) = 0;

    virtual std::shared_ptr<Application::Instance> existing(const AppID& appId,
                                                            const std::string& job,
                                                            const std::string& instance,
                                                            const std::vector<Application::URL>& urls) = 0;

    virtual std::list<std::shared_ptr<Application>> runningApps() = 0;

    virtual std::vector<std::shared_ptr<instance::Base>> instances(const AppID& appID, const std::string& job) = 0;

    static std::shared_ptr<Base> determineFactory(std::shared_ptr<Registry> registry);

    /* Signals to apps */
    virtual core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>>& appStarted() = 0;
    virtual core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>>& appStopped() = 0;
    virtual core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>, Registry::FailureType>&
        appFailed() = 0;
    virtual core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>, std::vector<pid_t>&>&
        appPaused() = 0;
    virtual core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>, std::vector<pid_t>&>&
        appResumed() = 0;

    /* App manager */
    virtual void setManager(std::shared_ptr<Registry::Manager> manager) = 0;
    virtual void clearManager() = 0;

protected:
    std::weak_ptr<Registry> registry_;
};

}  // namespace manager
}  // namespace jobs
}  // namespace app_launch
}  // namespace ubuntu
