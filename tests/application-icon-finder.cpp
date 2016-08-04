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
 *     Larry Price <larry.price@canonical.com>
 */

#include "application-icon-finder.h"
#include <gtest/gtest.h>

using namespace ubuntu::app_launch;

TEST(ApplicationIconFinder, ReturnsEmptyWhenNoThemeFileAvailable)
{
    IconFinder finder("/tmp/please/dont/put/stuff/here");
    EXPECT_TRUE(finder.find("app").value().empty());
}

TEST(ApplicationIconFinder, ReturnsEmptyWhenNoAppIconFound)
{
    auto basePath = std::string(CMAKE_SOURCE_DIR) + "/data";
    IconFinder finder(basePath);
    EXPECT_TRUE(finder.find("app_unknown").value().empty());
}

TEST(ApplicationIconFinder, ReturnsLargestAvailableIcon)
{
    auto basePath = std::string(CMAKE_SOURCE_DIR) + "/data/usr/share";
    IconFinder finder(basePath);
    EXPECT_EQ(basePath + "/icons/hicolor/24x24/apps/app.xpm", finder.find("app").value());
}

TEST(ApplicationIconFinder, ReturnsLargestAvailableIconIncludingLocalIcons)
{
    auto basePath = std::string(CMAKE_SOURCE_DIR) + "/data/home/test/.local/share";
    IconFinder finder(basePath);
    EXPECT_EQ(basePath + "/icons/hicolor/32x32/apps/steam_123456.png", finder.find("steam_123456").value());
}

TEST(ApplicationIconFinder, ReturnsIconAsDirectlyGiven)
{
    auto basePath = std::string(CMAKE_SOURCE_DIR) + "/data/usr/share";
    IconFinder finder(basePath);
    EXPECT_EQ(basePath + "/icons/hicolor/scalable/apps/app.svg",
              finder.find(basePath + "/icons/hicolor/scalable/apps/app.svg").value());
}

TEST(ApplicationIconFinder, ReturnsIconFromPixmapAsFallback)
{
    auto basePath = std::string(CMAKE_SOURCE_DIR) + "/data/usr/share";
    IconFinder finder(basePath);
    EXPECT_EQ(basePath + "/pixmaps/app2.png", finder.find("app2.png").value());
}

TEST(ApplicationIconFinder, ReturnsIconFromRootThemeDirectory)
{
    auto basePath = std::string(CMAKE_SOURCE_DIR) + "/data/usr/share";
    IconFinder finder(basePath);
    EXPECT_EQ(basePath + "/icons/hicolor/app4.png", finder.find("app4.png").value());
}

TEST(ApplicationIconFinder, ReturnsIconFromRootIconsDirectory)
{
    auto basePath = std::string(CMAKE_SOURCE_DIR) + "/data/usr/share";
    IconFinder finder(basePath);
    EXPECT_EQ(basePath + "/icons/app5.png", finder.find("app5.png").value());
}

TEST(ApplicationIconFinder, ReturnsThresholdIconBasedOnGap)
{
    auto basePath = std::string(CMAKE_SOURCE_DIR) + "/data/usr/share";
    IconFinder finder(basePath);
    EXPECT_EQ(basePath + "/icons/hicolor/22x22/apps/app1.png", finder.find("app1.png").value());
}

TEST(ApplicationIconFinder, IgnoresDirectoriesWithJunkSize)
{
    auto basePath = std::string(CMAKE_SOURCE_DIR) + "/data/usr/share";
    IconFinder finder(basePath);
    EXPECT_EQ(basePath + "/icons/hicolor/16x16/apps/app3.png", finder.find("app3.png").value());
}

TEST(ApplicationIconFinder, FindsHumanityIcon)
{
    auto basePath = std::string(CMAKE_SOURCE_DIR) + "/data/usr/share";
    IconFinder finder(basePath);
    EXPECT_EQ(basePath + "/icons/Humanity/16x16/apps/gedit.png", finder.find("gedit.png").value());
}
