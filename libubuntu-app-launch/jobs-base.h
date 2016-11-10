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
#include <gio/gio.h>
#include <set>

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

    static std::shared_ptr<gchar*> urlsToStrv(const std::vector<Application::URL>& urls);

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
    virtual ~Base();

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

    const std::set<std::string>& getAllJobs();

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
    virtual void setManager(std::shared_ptr<Registry::Manager> manager);
    virtual void clearManager();

protected:
    /** A link to the registry */
    std::weak_ptr<Registry> registry_;

    /** A set of all the job names */
    std::set<std::string> allJobs_;

    /** The DBus connection we're connecting to */
    std::shared_ptr<GDBusConnection> dbus_;

    /** Application manager instance */
    std::shared_ptr<Registry::Manager> manager_;

    /** Signal object for applications started */
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>> sig_appStarted;
    /** Signal object for applications stopped */
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>> sig_appStopped;
    /** Signal object for applications failed */
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>, Registry::FailureType>
        sig_appFailed;
    /** Signal object for applications paused */
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>, std::vector<pid_t>&>
        sig_appPaused;
    /** Signal object for applications resumed */
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>, std::vector<pid_t>&>
        sig_appResumed;

private:
    guint handle_managerSignalFocus{0};    /**< GDBus signal watcher handle for app focused signal */
    guint handle_managerSignalResume{0};   /**< GDBus signal watcher handle for app resumed signal */
    guint handle_managerSignalStarting{0}; /**< GDBus signal watcher handle for app starting signal */

    std::once_flag flag_managerSignals; /**< Variable to track to see if signal handlers are installed for the manager
                                           signals of focused, resumed and starting */

    static std::tuple<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>> managerParams(
        const std::shared_ptr<GVariant>& params, const std::shared_ptr<Registry>& reg);
    static guint managerSignalHelper(const std::shared_ptr<Registry>& reg,
                                     const std::string& signalname,
                                     std::function<void(const std::shared_ptr<Registry>& reg,
                                                        const std::shared_ptr<Application>& app,
                                                        const std::shared_ptr<Application::Instance>& instance,
                                                        const std::shared_ptr<GDBusConnection>&,
                                                        const std::string&,
                                                        const std::shared_ptr<GVariant>&)> responsefunc);
};

}  // namespace manager
}  // namespace jobs
}  // namespace app_launch
}  // namespace ubuntu
