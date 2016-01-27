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

#include "application-info-desktop.h"

#include <gtest/gtest.h>

class ApplicationInfoDesktop : public ::testing::Test
{
    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }
};

#define DESKTOP "Desktop Entry"

TEST_F(ApplicationInfoDesktop, DefaultState)
{
    auto keyfile = std::shared_ptr<GKeyFile>(g_key_file_new(), g_key_file_free);
    g_key_file_set_string(keyfile.get(), DESKTOP, "Name", "Foo App");
    g_key_file_set_string(keyfile.get(), DESKTOP, "Exec", "foo");
    g_key_file_set_string(keyfile.get(), DESKTOP, "Icon", "foo.png");

    auto appinfo = Ubuntu::AppLaunch::AppInfo::Desktop(keyfile, "/");

    EXPECT_EQ("Foo App", appinfo.name().value());
    EXPECT_EQ("", appinfo.description().value());
    EXPECT_EQ("/foo.png", appinfo.iconPath().value());

    EXPECT_EQ("", appinfo.splash().title.value());
    EXPECT_EQ("", appinfo.splash().image.value());
    EXPECT_EQ("", appinfo.splash().backgroundColor.value());
    EXPECT_EQ("", appinfo.splash().headerColor.value());
    EXPECT_EQ("", appinfo.splash().footerColor.value());
    EXPECT_FALSE( appinfo.splash().showHeader.value());

    EXPECT_TRUE(appinfo.supportedOrientations().portrait);
    EXPECT_TRUE(appinfo.supportedOrientations().landscape);
    EXPECT_TRUE(appinfo.supportedOrientations().invertedPortrait);
    EXPECT_TRUE(appinfo.supportedOrientations().invertedLandscape);

    EXPECT_TRUE(appinfo.rotatesWindowContents().value());

    EXPECT_FALSE(appinfo.ubuntuLifecycle().value());
}

TEST_F(ApplicationInfoDesktop, KeyfileErrors)
{
    EXPECT_THROW(Ubuntu::AppLaunch::AppInfo::Desktop({}, "/"), std::runtime_error);

    auto noname = std::shared_ptr<GKeyFile>(g_key_file_new(), g_key_file_free);
    g_key_file_set_string(noname.get(), DESKTOP, "Comment", "This is a comment");
    g_key_file_set_string(noname.get(), DESKTOP, "Exec", "foo");
    g_key_file_set_string(noname.get(), DESKTOP, "Icon", "foo.png");

    EXPECT_THROW(Ubuntu::AppLaunch::AppInfo::Desktop(noname, "/"), std::runtime_error);

    auto noicon = std::shared_ptr<GKeyFile>(g_key_file_new(), g_key_file_free);
    g_key_file_set_string(noicon.get(), DESKTOP, "Name", "Foo App");
    g_key_file_set_string(noicon.get(), DESKTOP, "Comment", "This is a comment");
    g_key_file_set_string(noicon.get(), DESKTOP, "Exec", "foo");

    EXPECT_THROW(Ubuntu::AppLaunch::AppInfo::Desktop(noicon, "/"), std::runtime_error);
}

TEST_F(ApplicationInfoDesktop, Orientations)
{
    Ubuntu::AppLaunch::Application::Info::Orientations defaultOrientations =
    {
portrait:
        true,
landscape:
        true,
invertedPortrait:
        true,
invertedLandscape:
        true
    };

    auto keyfile = std::shared_ptr<GKeyFile>(g_key_file_new(), g_key_file_free);
    g_key_file_set_string(keyfile.get(), DESKTOP, "Name", "Foo App");
    g_key_file_set_string(keyfile.get(), DESKTOP, "Exec", "foo");
    g_key_file_set_string(keyfile.get(), DESKTOP, "Icon", "foo.png");

    EXPECT_EQ(defaultOrientations, Ubuntu::AppLaunch::AppInfo::Desktop(keyfile, "/").supportedOrientations());

    g_key_file_set_string(keyfile.get(), DESKTOP, "X-Ubuntu-Supported-Orientations", "this should not parse");
    EXPECT_EQ(defaultOrientations, Ubuntu::AppLaunch::AppInfo::Desktop(keyfile, "/").supportedOrientations());

    g_key_file_set_string(keyfile.get(), DESKTOP, "X-Ubuntu-Supported-Orientations", "this;should;not;parse;");
    EXPECT_EQ(defaultOrientations, Ubuntu::AppLaunch::AppInfo::Desktop(keyfile, "/").supportedOrientations());

    g_key_file_set_string(keyfile.get(), DESKTOP, "X-Ubuntu-Supported-Orientations", "portrait;");
    EXPECT_EQ((Ubuntu::AppLaunch::Application::Info::Orientations {portrait: true, landscape: false, invertedPortrait: false, invertedLandscape: false}),
              Ubuntu::AppLaunch::AppInfo::Desktop(keyfile, "/").supportedOrientations());

    g_key_file_set_string(keyfile.get(), DESKTOP, "X-Ubuntu-Supported-Orientations", "landscape;portrait;");
    EXPECT_EQ((Ubuntu::AppLaunch::Application::Info::Orientations {portrait: true, landscape: true, invertedPortrait: false, invertedLandscape: false}),
              Ubuntu::AppLaunch::AppInfo::Desktop(keyfile, "/").supportedOrientations());

    g_key_file_set_string(keyfile.get(), DESKTOP, "X-Ubuntu-Supported-Orientations", "portrait;landscape;");
    EXPECT_EQ((Ubuntu::AppLaunch::Application::Info::Orientations {portrait: true, landscape: true, invertedPortrait: false, invertedLandscape: false}),
              Ubuntu::AppLaunch::AppInfo::Desktop(keyfile, "/").supportedOrientations());

    g_key_file_set_string(keyfile.get(), DESKTOP, "X-Ubuntu-Supported-Orientations", "portrait;landscape;invertedportrait;invertedlandscape;");
    EXPECT_EQ((Ubuntu::AppLaunch::Application::Info::Orientations {portrait: true, landscape: true, invertedPortrait: true, invertedLandscape: true}),
              Ubuntu::AppLaunch::AppInfo::Desktop(keyfile, "/").supportedOrientations());

    g_key_file_set_string(keyfile.get(), DESKTOP, "X-Ubuntu-Supported-Orientations", "PORTRAIT;");
    EXPECT_EQ((Ubuntu::AppLaunch::Application::Info::Orientations {portrait: true, landscape: false, invertedPortrait: false, invertedLandscape: false}),
              Ubuntu::AppLaunch::AppInfo::Desktop(keyfile, "/").supportedOrientations());

    g_key_file_set_string(keyfile.get(), DESKTOP, "X-Ubuntu-Supported-Orientations", "pOrTraIt;lANDscApE;inVErtEDpORtrAit;iNVErtEDLAnDsCapE;");
    EXPECT_EQ((Ubuntu::AppLaunch::Application::Info::Orientations {portrait: true, landscape: true, invertedPortrait: true, invertedLandscape: true}),
              Ubuntu::AppLaunch::AppInfo::Desktop(keyfile, "/").supportedOrientations());

}
