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

#include "application-impl-libertine.h"
#include "application-info-desktop.h"
#include "libertine.h"

namespace Ubuntu
{
namespace AppLaunch
{
namespace AppImpls
{

Libertine::Libertine (const AppID::Package& container,
                      const AppID::AppName& appname,
                      std::shared_ptr<Registry> registry) :
    Base(registry),
    _container(container),
    _appname(appname)
{
    auto container_path = libertine_container_path(container.value().c_str());
    auto container_app_path = g_build_filename(container_path, "usr", "share", "applications",
                                               (appname.value() + ".desktop").c_str(), NULL);

    if (g_file_test(container_app_path, G_FILE_TEST_EXISTS))
    {
        _appinfo = std::shared_ptr<GDesktopAppInfo>(g_desktop_app_info_new_from_filename(container_app_path), [](
                                                        GDesktopAppInfo * info)
        {
            g_clear_object(&info);
        });
    }
    else
    {
        auto home_path = libertine_container_home_path(container.value().c_str());
        auto home_app_path = g_build_filename(home_path, ".local", "share", "applications",
                                              (appname.value() + ".desktop").c_str(), NULL);

        if (g_file_test(home_app_path, G_FILE_TEST_EXISTS))
        {
            _appinfo = std::shared_ptr<GDesktopAppInfo>(g_desktop_app_info_new_from_filename(home_app_path), [](
                                                            GDesktopAppInfo * info)
            {
                g_clear_object(&info);
            });
        }

        g_free(home_app_path);
        g_free(home_path);
    }

    g_free(container_app_path);
    g_free(container_path);
}

std::list<std::shared_ptr<Application>>
                                     Libertine::list (std::shared_ptr<Registry> registry)
{
    std::list<std::shared_ptr<Application>> applist;

    auto containers = libertine_list_containers();

    for (int i = 0; containers[i] != nullptr; i++)
    {
        auto container = containers[i];
        auto apps = libertine_list_apps_for_container(container);

        for (int i = 0; apps[i] !=  nullptr; i++)
        {
            auto sapp = std::make_shared<Libertine>(AppID::Package::from_raw(container), AppID::AppName::from_raw(apps[i]),
                                                    registry);
            applist.push_back(sapp);
        }

        g_strfreev(apps);
    }
    g_strfreev(containers);

    return applist;
}

std::shared_ptr<Application::Info>
Libertine::info (void)
{
    if (_appinfo)
    {
        return std::make_shared<AppInfo::Desktop>(_appinfo, libertine_container_path(_container.value().c_str()));
    }
    else
    {
        return {};
    }
}

}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
