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
#include "libubuntu-app-launch/registry.h"

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <helper type>" << std::endl;
        return 1;
    }

    auto type = Ubuntu::AppLaunch::Helper::Type::from_raw(argv[1]);
    auto helpers = Ubuntu::AppLaunch::Registry::runningHelpers(type);
    for (auto helper : helpers)
    {
        std::cout << (std::string)helper->appId() << std::endl;
    }

    return 0;
}
