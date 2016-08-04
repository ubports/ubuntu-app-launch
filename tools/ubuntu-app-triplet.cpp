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

#include <iostream>
#include "libubuntu-app-launch/application.h"

int main(int argc, char* argv[])
{
    ubuntu::app_launch::AppID appid;

    switch (argc)
    {
        case 2:
            appid = ubuntu::app_launch::AppID::discover(argv[1]);
            break;
        case 3:
            appid = ubuntu::app_launch::AppID::discover(argv[1], argv[2]);
            break;
        case 4:
            appid = ubuntu::app_launch::AppID::discover(argv[1], argv[2], argv[3]);
            break;
        default:
            std::cerr << "Usage: " << argv[0] << " <package> [application] [version]" << std::endl;
            return 1;
    }

    std::cout << (std::string)appid << std::endl;
    return 0;
}
