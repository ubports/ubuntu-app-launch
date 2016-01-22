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

#include "application-impl-legacy.h"
#include "application-info-desktop.h"

namespace Ubuntu
{
namespace AppLaunch
{
namespace AppImpls
{

void
clear_keyfile (GKeyFile* keyfile)
{
    if (keyfile != nullptr)
    {
        g_key_file_free(keyfile);
    }
}

Legacy::Legacy (const AppID::AppName& appname,
                std::shared_ptr<GKeyFile> keyfile,
                std::shared_ptr<Registry> registry) :
    Base(registry),
    _appname(appname),
    _keyfile(keyfile)
{
}

Legacy::Legacy (const AppID::AppName& appname,
                std::shared_ptr<Registry> registry) :
    Legacy(appname, keyfileForApp(appname), registry)
{
}

std::shared_ptr<GKeyFile>
Legacy::keyfileForApp(const AppID::AppName& name)
{
    std::string desktopName = name.value() + ".desktop";
    auto keyfilecheck = [desktopName](const gchar * dir) -> std::shared_ptr<GKeyFile>
    {
        auto fullname = g_build_filename(dir, desktopName.c_str(), nullptr);
        if (g_file_test(fullname, G_FILE_TEST_EXISTS))
        {
            g_free(fullname);
            return {};
        }

        auto keyfile = std::shared_ptr<GKeyFile>(g_key_file_new(), clear_keyfile);

        GError* error = nullptr;
        g_key_file_load_from_file(keyfile.get(), fullname, G_KEY_FILE_NONE, &error);
        g_free(fullname);

        if (error != nullptr)
        {
            g_error_free(error);
            return {};
        }

        return keyfile;
    };

    auto retval = keyfilecheck(g_get_user_data_dir());

    auto systemDirs = g_get_system_data_dirs();
    for (auto i = 0; !retval && systemDirs[i] != nullptr; i++)
    {
        retval = keyfilecheck(systemDirs[i]);
    }

    return retval;
}

std::shared_ptr<Application::Info>
Legacy::info (void)
{
    if (_keyfile)
    {
        return std::make_shared<AppInfo::Desktop>(_keyfile, "/usr/share/icons/");
    }
    else
    {
        return {};
    }
}

std::list<std::shared_ptr<Application>>
                                     Legacy::list (std::shared_ptr<Registry> registry)
{
    std::list<std::shared_ptr<Application>> list;
    GList* head = g_app_info_get_all();
    for (GList* item = head; item != nullptr; item = g_list_next(item))
    {
        GDesktopAppInfo* appinfo = G_DESKTOP_APP_INFO(item->data);

        if (appinfo == nullptr)
        {
            continue;
        }

        if (g_app_info_should_show(G_APP_INFO(appinfo)) == FALSE)
        {
            continue;
        }

        auto app = std::make_shared<Legacy>(AppID::AppName::from_raw(g_app_info_get_id(G_APP_INFO(appinfo))),
                                            registry);
        list.push_back(app);
    }

    g_list_free_full(head, g_object_unref);

    return list;
}

}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
