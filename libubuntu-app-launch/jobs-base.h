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
#include "signal-unsubscriber.h"
#include "string-util.h"

#include <core/signal.h>
#include <gio/gio.h>
#include <map>
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
         const std::shared_ptr<Registry::Impl>& registry);
    virtual ~Base() = default;

    bool isRunning() override;
    bool hasPid(pid_t pid) override;
    void pause() override;
    void resume() override;
    void focus() override;

    const std::string& getInstanceId()
    {
        return instance_;
    }

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
    std::shared_ptr<Registry::Impl> registry_;

    std::vector<pid_t> forAllPids(std::function<void(pid_t)> eachPid);
    static void pidListToDbus(const std::shared_ptr<Registry::Impl>& reg,
                              const AppID& appid,
                              const std::string& instanceid,
                              const std::vector<pid_t>& pids,
                              const std::string& signal);
    static void signalToPid(pid_t pid, int signal);
    static void oomValueToPid(pid_t pid, const oom::Score oomvalue);
    static void oomValueToPidHelper(pid_t pid, const oom::Score oomvalue);
    static std::string pidToOomPath(pid_t pid);
    static GCharVUPtr urlsToStrv(const std::vector<Application::URL>& urls);
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
    Base(const Registry& registry);
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

    virtual std::list<std::shared_ptr<Application>> runningApps();
    virtual std::list<std::shared_ptr<Helper>> runningHelpers(const Helper::Type& type);

    virtual std::list<std::string> runningAppIds(const std::list<std::string>& jobs) = 0;

    virtual std::vector<std::shared_ptr<instance::Base>> instances(const AppID& appID, const std::string& job) = 0;

    const std::list<std::string>& getAllApplicationJobs() const;

    static std::shared_ptr<Base> determineFactory(const Registry& registry);

    /* Signals to apps */
    virtual core::Signal<const std::shared_ptr<Application>&, const std::shared_ptr<Application::Instance>&>&
        appStarted();
    virtual core::Signal<const std::shared_ptr<Application>&, const std::shared_ptr<Application::Instance>&>&
        appStopped();
    virtual core::Signal<const std::shared_ptr<Application>&,
                         const std::shared_ptr<Application::Instance>&,
                         Registry::FailureType>&
        appFailed();
    virtual core::Signal<const std::shared_ptr<Application>&,
                         const std::shared_ptr<Application::Instance>&,
                         const std::vector<pid_t>&>&
        appPaused();
    virtual core::Signal<const std::shared_ptr<Application>&,
                         const std::shared_ptr<Application::Instance>&,
                         const std::vector<pid_t>&>&
        appResumed();
    /* Signals to helpers */
    virtual core::Signal<const std::shared_ptr<Helper>&, const std::shared_ptr<Helper::Instance>&>& helperStarted(
        Helper::Type type);
    virtual core::Signal<const std::shared_ptr<Helper>&, const std::shared_ptr<Helper::Instance>&>& helperStopped(
        Helper::Type type);
    virtual core::
        Signal<const std::shared_ptr<Helper>&, const std::shared_ptr<Helper::Instance>&, Registry::FailureType>&
        helperFailed(Helper::Type type);
    /* Job signals from implementations */
    virtual core::Signal<const std::string&, const std::string&, const std::string&>& jobStarted() = 0;
    virtual core::Signal<const std::string&, const std::string&, const std::string&>& jobStopped() = 0;
    virtual core::Signal<const std::string&, const std::string&, const std::string&, Registry::FailureType>&
        jobFailed() = 0;

    /* App manager */
    virtual void setManager(std::shared_ptr<Registry::Manager> manager);
    virtual void clearManager();

protected:
    /** A link to the registry */
    const Registry& registry_;

    /** A set of all the job names used by applications */
    std::list<std::string> allApplicationJobs_;

    /** Application manager instance */
    std::shared_ptr<Registry::Manager> manager_;

private:
    /** Signal object for applications started */
    core::Signal<const std::shared_ptr<Application>&, const std::shared_ptr<Application::Instance>&> sig_appStarted;
    /** Signal object for applications stopped */
    core::Signal<const std::shared_ptr<Application>&, const std::shared_ptr<Application::Instance>&> sig_appStopped;
    /** Signal object for applications failed */
    core::Signal<const std::shared_ptr<Application>&,
                 const std::shared_ptr<Application::Instance>&,
                 Registry::FailureType>
        sig_appFailed;
    /** Signal object for applications paused */
    core::Signal<const std::shared_ptr<Application>&,
                 const std::shared_ptr<Application::Instance>&,
                 const std::vector<pid_t>&>
        sig_appPaused;
    /** Signal object for applications resumed */
    core::Signal<const std::shared_ptr<Application>&,
                 const std::shared_ptr<Application::Instance>&,
                 const std::vector<pid_t>&>
        sig_appResumed;

    std::map<std::string,
             std::shared_ptr<core::Signal<const std::shared_ptr<Helper>&, const std::shared_ptr<Helper::Instance>&>>>
        sig_helpersStarted;
    std::map<std::string,
             std::shared_ptr<core::Signal<const std::shared_ptr<Helper>&, const std::shared_ptr<Helper::Instance>&>>>
        sig_helpersStopped;
    std::map<std::string,
             std::shared_ptr<core::Signal<const std::shared_ptr<Helper>&,
                                          const std::shared_ptr<Helper::Instance>&,
                                          Registry::FailureType>>>
        sig_helpersFailed;

    ManagedDBusSignalConnection handle_managerSignalFocus{
        DBusSignalUnsubscriber{}}; /**< GDBus signal watcher handle for app focused signal */
    ManagedDBusSignalConnection handle_managerSignalResume{
        DBusSignalUnsubscriber{}}; /**< GDBus signal watcher handle for app resumed signal */
    ManagedDBusSignalConnection handle_managerSignalStarting{
        DBusSignalUnsubscriber{}}; /**< GDBus signal watcher handle for app starting signal */
    ManagedDBusSignalConnection handle_appPaused{
        DBusSignalUnsubscriber{}}; /**< GDBus signal watcher handle for app paused signal */
    ManagedDBusSignalConnection handle_appResumed{
        DBusSignalUnsubscriber{}}; /**< GDBus signal watcher handle for app resumed signal */

    std::once_flag flag_managerSignals; /**< Variable to track to see if signal handlers are installed for the manager
                                           signals of focused, resumed and starting */
    std::once_flag flag_appStarted;     /**< Variable to track to see if signal handlers are installed for application
                                           started */
    std::once_flag flag_appStopped;     /**< Variable to track to see if signal handlers are installed for application
                                           stopped */
    std::once_flag
        flag_appFailed; /**< Variable to track to see if signal handlers are installed for application failed */
    std::once_flag
        flag_appPaused; /**< Variable to track to see if signal handlers are installed for application paused */
    std::once_flag flag_appResumed; /**< Variable to track to see if signal handlers are installed for application
                                       resumed */

    void pauseEventEmitted(core::Signal<const std::shared_ptr<Application>&,
                                        const std::shared_ptr<Application::Instance>&,
                                        const std::vector<pid_t>&>& signal,
                           const std::shared_ptr<GVariant>& params,
                           const std::shared_ptr<Registry::Impl>& reg);

    static std::tuple<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>> managerParams(
        const std::shared_ptr<GVariant>& params, const std::shared_ptr<Registry::Impl>& reg);
    guint managerSignalHelper(const std::string& signalname,
                              std::function<void(const std::shared_ptr<Registry::Impl>& reg,
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
