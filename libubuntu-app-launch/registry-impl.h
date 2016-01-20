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

#include "registry.h"
#include "glib-thread.h"

#include <json-glib/json-glib.h>
#include <click.h>
#include <gio/gio.h>

#pragma once

namespace Ubuntu
{
namespace AppLaunch
{

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

    void setManager (Registry::Manager* manager);
    void clearManager ();

    GLib::ContextThread thread;
private:
    Registry* _registry;
    Registry::Manager* _manager;

    std::shared_ptr<ClickDB> _clickDB;
    std::shared_ptr<ClickUser> _clickUser;

    std::shared_ptr<GDBusConnection> _dbus;

    void initClick ();
};

}; // namespace AppLaunch
}; // namespace Ubuntu
