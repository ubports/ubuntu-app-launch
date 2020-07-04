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

#if 0
    void setManager (Registry::Manager* manager);
    void clearManager ();
#endif

    /** Shared context thread for events and background tasks
        that UAL subtasks are doing */
    GLib::ContextThread thread;
    /** DBus shared connection for the session bus */
    std::shared_ptr<GDBusConnection> _dbus;
    /** DBus shared connection for the system bus */
    std::shared_ptr<GDBusConnection> _dbus_system;

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

    /* Signal Hints */
    /* NOTE: Static because we don't have registry instances in the C
       code right now. We want these to not be static in the future */
    static void watchingAppStarting(bool rWatching);
    static bool isWatchingAppStarting();

private:
    Registry* _registry;
#if 0
    Registry::Manager* _manager;
#endif

    std::shared_ptr<ClickDB> _clickDB;
    std::shared_ptr<ClickUser> _clickUser;

    void initClick();

    std::shared_ptr<ZeitgeistLog> zgLog_;

    std::shared_ptr<GDBusConnection> cgManager_;

    void initCGManager();

    std::unordered_map<std::string, std::shared_ptr<IconFinder>> _iconFinders;

    /** Getting the Upstart job path is relatively expensive in
        that it requires a DBus call. Worth keeping a cache of. */
    std::map<std::string, std::string> upstartJobPathCache_;
};

}  // namespace app_launch
}  // namespace ubuntu
