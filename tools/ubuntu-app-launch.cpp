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

#include "libubuntu-app-launch/application.h"
#include "libubuntu-app-launch/registry.h"
#include <csignal>
#include <future>
#include <iostream>

ubuntu::app_launch::AppID global_appid;
std::promise<int> retval;

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <app id> [uris]" << std::endl;
        return 1;
    }

    global_appid = ubuntu::app_launch::AppID::find(argv[1]);

    std::vector<ubuntu::app_launch::Application::URL> urls;
    for (int i = 2; i < argc; i++)
    {
        urls.push_back(ubuntu::app_launch::Application::URL::from_raw(argv[i]));
    }

    ubuntu::app_launch::Registry::appStarted().connect(
        [](std::shared_ptr<ubuntu::app_launch::Application> app,
           std::shared_ptr<ubuntu::app_launch::Application::Instance> instance) {
            if (app->appId() != global_appid)
            {
                return;
            }

            std::cout << "Started: " << (std::string)app->appId() << std::endl;
            retval.set_value(EXIT_SUCCESS);
        });

    ubuntu::app_launch::Registry::appFailed().connect(
        [](std::shared_ptr<ubuntu::app_launch::Application> app,
           std::shared_ptr<ubuntu::app_launch::Application::Instance> instance,
           ubuntu::app_launch::Registry::FailureType type) {
            if (app->appId() != global_appid)
            {
                return;
            }

            std::cout << "Failed:  " << (std::string)app->appId() << std::endl;
            retval.set_value(EXIT_FAILURE);
        });

    auto app = ubuntu::app_launch::Application::create(global_appid, ubuntu::app_launch::Registry::getDefault());
    app->launch(urls);

    std::signal(SIGTERM, [](int signal) -> void { retval.set_value(EXIT_SUCCESS); });
    return retval.get_future().get();
}
