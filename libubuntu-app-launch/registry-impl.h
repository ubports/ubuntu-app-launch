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

#include "app-store-base.h"
#include "glib-thread.h"
#include "info-watcher-zg.h"
#include "jobs-base.h"
#include "registry.h"
#include "snapd-info.h"
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
    Impl(Registry& registry);
    Impl(Registry& registry, std::list<std::shared_ptr<app_store::Base>> appStores);

    virtual ~Impl()
    {
        thread.quit();
    }

    static void setManager(const std::shared_ptr<Registry::Manager>& manager,
                           const std::shared_ptr<Registry>& registry);
    void clearManager();

    /** Shared context thread for events and background tasks
        that UAL subtasks are doing */
    GLib::ContextThread thread;
    /** DBus shared connection for the session bus */
    std::shared_ptr<GDBusConnection> _dbus;

    /** Snapd information object */
    snapd::Info snapdInfo;

    std::shared_ptr<jobs::manager::Base> jobs;

    std::shared_ptr<IconFinder> getIconFinder(std::string basePath);

    virtual void zgSendEvent(AppID appid, const std::string& eventtype);

    static std::string printJson(std::shared_ptr<JsonObject> jsonobj);
    static std::string printJson(std::shared_ptr<JsonNode> jsonnode);

    /* Signal Hints */
    /* NOTE: Static because we don't have registry instances in the C
       code right now. We want these to not be static in the future */
    static void watchingAppStarting(bool rWatching);
    static bool isWatchingAppStarting();

    const std::string& oomHelper() const
    {
        return oomHelper_;
    }

    static std::shared_ptr<info_watcher::Zeitgeist> getZgWatcher(const std::shared_ptr<Registry>& reg)
    {
        std::call_once(reg->impl->zgWatcherOnce_,
                       [reg] { reg->impl->zgWatcher_ = std::make_shared<info_watcher::Zeitgeist>(reg); });
        return reg->impl->zgWatcher_;
    }

    core::Signal<const std::shared_ptr<Application>&>& appInfoUpdated(const std::shared_ptr<Registry>& reg);
    core::Signal<const std::shared_ptr<Application>&>& appAdded(const std::shared_ptr<Registry>& reg);
    core::Signal<const AppID&>& appRemoved(const std::shared_ptr<Registry>& reg);

    std::list<std::shared_ptr<app_store::Base>> appStores()
    {
        return _appStores;
    }

    void setAppStores(std::list<std::shared_ptr<app_store::Base>>& newlist)
    {
        _appStores = newlist;
    }

private:
    Registry& _registry; /**< The Registry that we're spawned from */

    /** Shared instance of the Zeitgeist Log */
    std::shared_ptr<ZeitgeistLog> zgLog_;

    /** All of our icon finders based on the path that they're looking
        into */
    std::unordered_map<std::string, std::shared_ptr<IconFinder>> _iconFinders;

    /** Path to the OOM Helper */
    std::string oomHelper_;

    /** Application stores */
    std::list<std::shared_ptr<app_store::Base>> _appStores;

    /** Signal for application info changing */
    core::Signal<const std::shared_ptr<Application>&> sig_appInfoUpdated;
    /** Signal for applications added */
    core::Signal<const std::shared_ptr<Application>&> sig_appAdded;
    /** Signal for applications removed */
    core::Signal<const AppID&> sig_appRemoved;

    void infoWatchersSetup(const std::shared_ptr<Registry>& reg);
    /** Flag to see if we've initialized the info watcher list */
    std::once_flag flag_infoWatchersSetup;
    struct infoWatcherConnections
    {
        core::ScopedConnection infoUpdated;
        core::ScopedConnection appAdded;
        core::ScopedConnection appRemoved;
    };
    /** List of info watchers along with a signal handle to our connection to their update signal */
    std::list<std::pair<std::shared_ptr<info_watcher::Base>, infoWatcherConnections>> infoWatchers_;

protected:
    /** ZG Info Watcher */
    std::shared_ptr<info_watcher::Zeitgeist> zgWatcher_;
    /** Init checker for ZG Watcher */
    std::once_flag zgWatcherOnce_;
};

}  // namespace app_launch
}  // namespace ubuntu
