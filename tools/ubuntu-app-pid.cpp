/*
 * Copyright 2015 Canonical Ltd.
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

#include <iostream>
#include <libubuntu-app-launch/registry.h>
#include <libubuntu-app-launch/application.h>

int main(int argc, char* argv[])
{

    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <app id>" << std::endl;
        return 1;
    }

    auto appid = ubuntu::app_launch::AppID::find(argv[1]);
    if (appid.empty()) {
        std::cerr << "Unable to find app for appid: " << argv[1] << std::endl;
        return 1;
    }

    try {
        auto app = ubuntu::app_launch::Application::create(appid, ubuntu::app_launch::Registry::getDefault());
        auto pid = app->instances()[0]->primaryPid();

        if (pid == 0)
        {
            return 1;
        }

        std::cout << pid << std::endl;
        return 0;
    } catch (std::runtime_error &e) {
        std::cerr << "Unable to find application for '" << std::string(appid) << "': " << e.what() << std::endl;
        return 1;
    }
}
