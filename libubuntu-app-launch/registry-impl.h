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

#include "glib-thread.h"
#include "registry.h"
#include "snapd-info.h"
#include <click.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <map>
#include <unordered_map>
#include <zeitgeist.h>

#pragma once

namespace ubuntu
{
namespace app_launch
{

class IconFinder;

/** \private
    \brief Private implementation of the Registry object

*/
class Registry::Impl
{
public:
    Impl(Registry* registry);
    virtual ~Impl()
    {
        thread.quit();
    }

    std::shared_ptr<JsonObject> getClickManifest(const std::string& package);
    std::list<AppID::Package> getClickPackages();
    std::string getClickDir(const std::string& package);

    static void setManager(std::shared_ptr<Registry::Manager> manager, std::shared_ptr<Registry> registry);
    void clearManager();

    /** Shared context thread for events and background tasks
        that UAL subtasks are doing */
    GLib::ContextThread thread;
    /** DBus shared connection for the session bus */
    std::shared_ptr<GDBusConnection> _dbus;

#ifdef ENABLE_SNAPPY
    /** Snapd information object */
    snapd::Info snapdInfo;
#endif

    std::shared_ptr<IconFinder> getIconFinder(std::string basePath);

    void zgSendEvent(AppID appid, const std::string& eventtype);

    std::vector<pid_t> pidsFromCgroup(const std::string& jobpath);

    /* Upstart Jobs */
    std::list<std::string> upstartInstancesForJob(const std::string& job);
    std::string upstartJobPath(const std::string& job);

    static std::string printJson(std::shared_ptr<JsonObject> jsonobj);
    static std::string printJson(std::shared_ptr<JsonNode> jsonnode);

    /* Signals to discover what is happening to apps */
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>>& appStarted(
        const std::shared_ptr<Registry>& reg);
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>>& appStopped(
        const std::shared_ptr<Registry>& reg);
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>, FailureType>& appFailed(
        const std::shared_ptr<Registry>& reg);
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>, std::vector<pid_t>&>& appPaused(
        const std::shared_ptr<Registry>& reg);
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>, std::vector<pid_t>&>& appResumed(
        const std::shared_ptr<Registry>& reg);

    /* Signal Hints */
    /* NOTE: Static because we don't have registry instances in the C
       code right now. We want these to not be static in the future */
    static void watchingAppStarting(bool rWatching);
    static bool isWatchingAppStarting();

private:
    Registry* _registry;                         /**< The Registry that we're spawned from */
    std::shared_ptr<Registry::Manager> manager_; /**< Application manager if registered */

    std::shared_ptr<ClickDB> _clickDB;     /**< Shared instance of the Click Database */
    std::shared_ptr<ClickUser> _clickUser; /**< Click database filtered by the current user */

    /** Signal object for applications started */
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>> sig_appStarted;
    /** Signal object for applications stopped */
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>> sig_appStopped;
    /** Signal object for applications failed */
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>, FailureType> sig_appFailed;
    /** Signal object for applications paused */
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>, std::vector<pid_t>&>
        sig_appPaused;
    /** Signal object for applications resumed */
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>, std::vector<pid_t>&>
        sig_appResumed;

    guint handle_appStarted{0};            /**< GDBus signal watcher handle for app started signal */
    guint handle_appStopped{0};            /**< GDBus signal watcher handle for app stopped signal */
    guint handle_appFailed{0};             /**< GDBus signal watcher handle for app failed signal */
    guint handle_appPaused{0};             /**< GDBus signal watcher handle for app paused signal */
    guint handle_appResumed{0};            /**< GDBus signal watcher handle for app resumed signal */
    guint handle_managerSignalFocus{0};    /**< GDBus signal watcher handle for app focused signal */
    guint handle_managerSignalResume{0};   /**< GDBus signal watcher handle for app resumed signal */
    guint handle_managerSignalStarting{0}; /**< GDBus signal watcher handle for app starting signal */

    std::once_flag flag_appStarted; /**< Variable to track to see if signal handlers are installed for application
                                       started */
    std::once_flag flag_appStopped; /**< Variable to track to see if signal handlers are installed for application
                                       stopped */
    std::once_flag
        flag_appFailed; /**< Variable to track to see if signal handlers are installed for application failed */
    std::once_flag
        flag_appPaused; /**< Variable to track to see if signal handlers are installed for application paused */
    std::once_flag flag_appResumed;     /**< Variable to track to see if signal handlers are installed for application
                                           resumed */
    std::once_flag flag_managerSignals; /**< Variable to track to see if signal handlers are installed for the manager
                                           signals of focused, resumed and starting */

    void upstartEventEmitted(
        core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>>& signal,
        const std::shared_ptr<GVariant>& params,
        const std::shared_ptr<Registry>& reg);
    void pauseEventEmitted(
        core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>, std::vector<pid_t>&>& signal,
        const std::shared_ptr<GVariant>& params,
        const std::shared_ptr<Registry>& reg);
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

    void initClick();

    /** Shared instance of the Zeitgeist Log */
    std::shared_ptr<ZeitgeistLog> zgLog_;

    /** Shared connection to CGManager */
    std::shared_ptr<GDBusConnection> cgManager_;

    void initCGManager();

    /** All of our icon finders based on the path that they're looking
        into */
    std::unordered_map<std::string, std::shared_ptr<IconFinder>> _iconFinders;

    /** Getting the Upstart job path is relatively expensive in
        that it requires a DBus call. Worth keeping a cache of. */
    std::map<std::string, std::string> upstartJobPathCache_;
};

}  // namespace app_launch
}  // namespace ubuntu
