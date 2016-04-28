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
#include <glib.h>

using namespace ubuntu::app_launch;

TEST(ApplicationIconFinder, ReturnsDefaultPathWhenNoThemeFileAvailable)
{
    IconFinder finder("/tmp/please/dont/put/stuff/here");
    EXPECT_EQ("/tmp/please/dont/put/stuff/here/app", finder.find("app").value());
}

TEST(ApplicationIconFinder, ReturnsDefaultPathWhenNoAppIconFound)
{
    auto dir = g_get_current_dir();
    auto basePath = std::string(dir) + "/data";
    g_free(dir);
    IconFinder finder(basePath);
    EXPECT_EQ(basePath + "/app_unknown", finder.find("app_unknown").value());
}

TEST(ApplicationIconFinder, ReturnsLargestAvailableIcon)
{
    auto dir = g_get_current_dir();
    auto basePath = std::string(dir) + "/data";
    g_free(dir);
    IconFinder finder(basePath);
    EXPECT_EQ(basePath + "/usr/share/icons/hicolor/24x24/apps/app.xpm", finder.find("app").value());
}

TEST(ApplicationIconFinder, ReturnsIconAsDirectlyGiven)
{
    auto dir = g_get_current_dir();
    auto basePath = std::string(dir) + "/data";
    g_free(dir);
    IconFinder finder(basePath);
    EXPECT_EQ(basePath + "/usr/share/icons/hicolor/scalable/apps/app.svg", finder.find(basePath + "/usr/share/icons/hicolor/scalable/apps/app.svg").value());
}

TEST(ApplicationIconFinder, ReturnsIconFromPixmapWhenGivenExtension)
{
    auto dir = g_get_current_dir();
    auto basePath = std::string(dir) + "/data";
    g_free(dir);
    IconFinder finder(basePath);
    EXPECT_EQ(basePath + "/usr/share/pixmaps/app.png", finder.find("app.png").value());
}
