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

#include "registry-impl.h"

namespace ubuntu
{
namespace app_launch
{

Registry::Impl::Impl(Registry* registry)
    : thread(
          []()
          {
          },
          [this]()
          {
              _clickUser.reset();
              _clickDB.reset();

              g_dbus_connection_flush_sync(_dbus.get(), nullptr, nullptr);
              _dbus.reset();
          })
    , _registry(registry)
// _manager(nullptr)
{
    auto cancel = thread.getCancellable();
    _dbus = thread.executeOnThread<std::shared_ptr<GDBusConnection>>(
        [cancel]()
        {
            return std::shared_ptr<GDBusConnection>(g_bus_get_sync(G_BUS_TYPE_SESSION, cancel.get(), nullptr),
                                                    [](GDBusConnection* bus)
                                                    {
                                                        g_clear_object(&bus);
                                                    });
        });
}

void Registry::Impl::initClick()
{
    if (_clickDB && _clickUser)
    {
        return;
    }

    auto init = thread.executeOnThread<bool>(
        [this]()
        {
            GError* error = nullptr;

            if (!_clickDB)
            {
                _clickDB = std::shared_ptr<ClickDB>(click_db_new(), [](ClickDB* db)
                                                    {
                                                        g_clear_object(&db);
                                                    });
                /* If TEST_CLICK_DB is unset, this reads the system database. */
                click_db_read(_clickDB.get(), g_getenv("TEST_CLICK_DB"), &error);

                if (error != nullptr)
                {
                    auto perror = std::shared_ptr<GError>(error, [](GError* error)
                                                          {
                                                              g_error_free(error);
                                                          });
                    throw std::runtime_error(perror->message);
                }
            }

            if (!_clickUser)
            {
                _clickUser = std::shared_ptr<ClickUser>(
                    click_user_new_for_user(_clickDB.get(), g_getenv("TEST_CLICK_USER"), &error), [](ClickUser* user)
                    {
                        g_clear_object(&user);
                    });

                if (error != nullptr)
                {
                    auto perror = std::shared_ptr<GError>(error, [](GError* error)
                                                          {
                                                              g_error_free(error);
                                                          });
                    throw std::runtime_error(perror->message);
                }
            }

            return true;
        });

    if (!init)
    {
        throw std::runtime_error("Unable to initialize the Click Database");
    }
}

std::shared_ptr<JsonObject> Registry::Impl::getClickManifest(const std::string& package)
{
    initClick();

    auto retval = thread.executeOnThread<std::shared_ptr<JsonObject>>(
        [this, package]()
        {
            GError* error = nullptr;
            auto retval = std::shared_ptr<JsonObject>(
                click_user_get_manifest(_clickUser.get(), package.c_str(), &error), [](JsonObject* obj)
                {
                    if (obj != nullptr)
                    {
                        json_object_unref(obj);
                    }
                });

            if (error != nullptr)
            {
                auto perror = std::shared_ptr<GError>(error, [](GError* error)
                                                      {
                                                          g_error_free(error);
                                                      });
                throw std::runtime_error(perror->message);
            }

            return retval;
        });

    if (!retval)
        throw std::runtime_error("Unable to get Click manifest for package: " + package);

    return retval;
}

std::list<AppID::Package> Registry::Impl::getClickPackages()
{
    initClick();

    return thread.executeOnThread<std::list<AppID::Package>>(
        [this]()
        {
            GError* error = nullptr;
            GList* pkgs = click_db_get_packages(_clickDB.get(), FALSE, &error);

            if (error != nullptr)
            {
                auto perror = std::shared_ptr<GError>(error, [](GError* error)
                                                      {
                                                          g_error_free(error);
                                                      });
                throw std::runtime_error(perror->message);
            }

            std::list<AppID::Package> list;
            for (GList* item = pkgs; item != NULL; item = g_list_next(item))
            {
                list.emplace_back(AppID::Package::from_raw((gchar*)item->data));
            }

            g_list_free_full(pkgs, g_free);
            return list;
        });
}

std::string Registry::Impl::getClickDir(const std::string& package)
{
    initClick();

    return thread.executeOnThread<std::string>([this, package]()
                                               {
                                                   GError* error = nullptr;
                                                   auto dir =
                                                       click_user_get_path(_clickUser.get(), package.c_str(), &error);

                                                   if (error != nullptr)
                                                   {
                                                       auto perror = std::shared_ptr<GError>(error, [](GError* error)
                                                                                             {
                                                                                                 g_error_free(error);
                                                                                             });
                                                       throw std::runtime_error(perror->message);
                                                   }

                                                   std::string cppdir(dir);
                                                   g_free(dir);
                                                   return cppdir;
                                               });
}

#if 0
void
Registry::Impl::setManager (Registry::Manager* manager)
{
    if (_manager != nullptr)
    {
        throw std::runtime_error("Already have a manager and trying to set another");
    }

    _manager = manager;
}

void
Registry::Impl::clearManager ()
{
    _manager = nullptr;
}
#endif

};  // namespace app_launch
};  // namespace ubuntu
