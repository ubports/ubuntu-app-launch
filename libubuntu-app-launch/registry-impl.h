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
#include <click.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <unordered_map>

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

    GLib::ContextThread thread;

    std::shared_ptr<IconFinder> getIconFinder(std::string basePath);

private:
    Registry* _registry;
#if 0
    Registry::Manager* _manager;
#endif

    std::shared_ptr<ClickDB> _clickDB;
    std::shared_ptr<ClickUser> _clickUser;

    std::shared_ptr<GDBusConnection> _dbus;

    void initClick();

    std::unordered_map<std::string, std::shared_ptr<IconFinder>> _iconFinders;
};

};  // namespace app_launch
};  // namespace ubuntu
