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
#include <cstdlib>

namespace
{
#define DESKTOP "Desktop Entry"

class ApplicationInfoDesktop : public ::testing::Test
{
protected:
    ApplicationInfoDesktop()
      : test_dekstop_env("SomeFreeDesktop")
    {
    }

    virtual void SetUp()
    {
        setenv("XDG_CURRENT_DESKTOP", test_dekstop_env.c_str(), true);
    }

    virtual void TearDown()
    {
    }

    std::shared_ptr<GKeyFile> defaultKeyfile()
    {
        auto keyfile = std::shared_ptr<GKeyFile>(g_key_file_new(), g_key_file_free);
        g_key_file_set_string(keyfile.get(), DESKTOP, "Type", "Application");
        g_key_file_set_string(keyfile.get(), DESKTOP, "Name", "Foo App");
        g_key_file_set_string(keyfile.get(), DESKTOP, "Exec", "foo");
        g_key_file_set_string(keyfile.get(), DESKTOP, "Icon", "foo.png");
        return keyfile;
    }

    const std::string test_dekstop_env;
};


TEST_F(ApplicationInfoDesktop, DefaultState)
{
    auto appinfo = ubuntu::app_launch::app_info::Desktop(defaultKeyfile(), "/");

    EXPECT_EQ("Foo App", appinfo.name().value());
    EXPECT_EQ("", appinfo.description().value());
    EXPECT_EQ("/foo.png", appinfo.iconPath().value());
    EXPECT_EQ("", appinfo.defaultDepartment().value());

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

    EXPECT_FALSE(appinfo.rotatesWindowContents().value());

    EXPECT_FALSE(appinfo.supportsUbuntuLifecycle().value());
}

TEST_F(ApplicationInfoDesktop, KeyfileErrors)
{
    // empty
    EXPECT_THROW(ubuntu::app_launch::app_info::Desktop({}, "/"), std::runtime_error);

    // empty name
    auto noname = defaultKeyfile();
    g_key_file_remove_key(noname.get(), DESKTOP, "Name", nullptr);
    EXPECT_THROW(ubuntu::app_launch::app_info::Desktop(noname, "/"), std::runtime_error);

    // empty icon
    auto noicon = defaultKeyfile();
    g_key_file_remove_key(noicon.get(), DESKTOP, "Icon", nullptr);
    EXPECT_THROW(ubuntu::app_launch::app_info::Desktop(noicon, "/"), std::runtime_error);

    // wrong type
    auto wrongtype = defaultKeyfile();
    g_key_file_set_string(wrongtype.get(), DESKTOP, "Type", "MimeType");
    EXPECT_THROW(ubuntu::app_launch::app_info::Desktop(wrongtype, "/"), std::runtime_error);

    // not displayable
    auto nodisplay = defaultKeyfile();
    g_key_file_set_string(nodisplay.get(), DESKTOP, "NoDisplay", "true");
    EXPECT_THROW(ubuntu::app_launch::app_info::Desktop(nodisplay, "/"), std::runtime_error);

    // hidden
    auto hidden = defaultKeyfile();
    g_key_file_set_string(hidden.get(), DESKTOP, "Hidden", "true");
    EXPECT_THROW(ubuntu::app_launch::app_info::Desktop(hidden, "/"), std::runtime_error);

    // not shown in Unity
    auto notshowin = defaultKeyfile();
    g_key_file_set_string(notshowin.get(), DESKTOP, "NotShowIn", ("Gnome;" + test_dekstop_env + ";").c_str());
    EXPECT_THROW(ubuntu::app_launch::app_info::Desktop(notshowin, "/"), std::runtime_error);

    // only show in not Unity
    auto onlyshowin = defaultKeyfile();
    g_key_file_set_string(onlyshowin.get(), DESKTOP, "OnlyShowIn", "KDE;Gnome;");
    EXPECT_THROW(ubuntu::app_launch::app_info::Desktop(onlyshowin, "/"), std::runtime_error);
}

TEST_F(ApplicationInfoDesktop, KeyfileDefaultDepartment)
{
    auto keyfile = defaultKeyfile();
    g_key_file_set_string(keyfile.get(), DESKTOP, "X-Ubuntu-Default-Department-ID", "foo");
    EXPECT_NO_THROW(ubuntu::app_launch::app_info::Desktop(keyfile, "/"));
}

TEST_F(ApplicationInfoDesktop, KeyfileScreenshotPath)
{
    auto keyfile = defaultKeyfile();
    g_key_file_set_string(keyfile.get(), DESKTOP, "X-Screenshot", "foo.png");
    EXPECT_EQ("/foo.png", ubuntu::app_launch::app_info::Desktop(keyfile, "/").screenshotPath().value());
}

TEST_F(ApplicationInfoDesktop, KeyfileKeywords)
{
    std::vector<std::string>expectedKeywords{"foo", "bar", "baz"};

    auto keyfile = defaultKeyfile();
    g_key_file_set_string(keyfile.get(), DESKTOP, "Keywords", "foo;bar;baz;");
    EXPECT_EQ(expectedKeywords, ubuntu::app_launch::app_info::Desktop(keyfile, "/").keywords().value());
}

TEST_F(ApplicationInfoDesktop, KeyfileShowListEdgeCases)
{
  // Not appearing in not show list
  auto notshowin = defaultKeyfile();
  g_key_file_set_string(notshowin.get(), DESKTOP, "NotShowIn", "Gnome;KDE;");
  EXPECT_NO_THROW(ubuntu::app_launch::app_info::Desktop(notshowin, "/"));

  // Appearing explicitly in only show list
  auto onlyshowin = defaultKeyfile();
  g_key_file_set_string(onlyshowin.get(), DESKTOP, "OnlyShowIn", (test_dekstop_env + ";Gnome;").c_str());
  EXPECT_NO_THROW(ubuntu::app_launch::app_info::Desktop(onlyshowin, "/"));

  // Appearing explicitly in only show list not first
  auto onlyshowinmiddle = defaultKeyfile();
  g_key_file_set_string(onlyshowinmiddle.get(), DESKTOP, "OnlyShowIn", ("Gnome;" + test_dekstop_env + ";KDE;").c_str());
  EXPECT_NO_THROW(ubuntu::app_launch::app_info::Desktop(onlyshowinmiddle, "/"));
}

TEST_F(ApplicationInfoDesktop, Orientations)
{
    ubuntu::app_launch::Application::Info::Orientations defaultOrientations =
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

    auto keyfile = defaultKeyfile();

    EXPECT_EQ(defaultOrientations, ubuntu::app_launch::app_info::Desktop(keyfile, "/").supportedOrientations());

    g_key_file_set_string(keyfile.get(), DESKTOP, "X-Ubuntu-Supported-Orientations", "this should not parse");
    EXPECT_EQ(defaultOrientations, ubuntu::app_launch::app_info::Desktop(keyfile, "/").supportedOrientations());

    g_key_file_set_string(keyfile.get(), DESKTOP, "X-Ubuntu-Supported-Orientations", "this;should;not;parse;");
    EXPECT_EQ(defaultOrientations, ubuntu::app_launch::app_info::Desktop(keyfile, "/").supportedOrientations());

    g_key_file_set_string(keyfile.get(), DESKTOP, "X-Ubuntu-Supported-Orientations", "portrait;");
    EXPECT_EQ((ubuntu::app_launch::Application::Info::Orientations {portrait: true, landscape: false, invertedPortrait: false, invertedLandscape: false}),
              ubuntu::app_launch::app_info::Desktop(keyfile, "/").supportedOrientations());

    g_key_file_set_string(keyfile.get(), DESKTOP, "X-Ubuntu-Supported-Orientations", "landscape;portrait;");
    EXPECT_EQ((ubuntu::app_launch::Application::Info::Orientations {portrait: true, landscape: true, invertedPortrait: false, invertedLandscape: false}),
              ubuntu::app_launch::app_info::Desktop(keyfile, "/").supportedOrientations());

    g_key_file_set_string(keyfile.get(), DESKTOP, "X-Ubuntu-Supported-Orientations", "landscape  ;  portrait;	invertedPortrait");
    EXPECT_EQ((ubuntu::app_launch::Application::Info::Orientations {portrait: true, landscape: true, invertedPortrait: true, invertedLandscape: false}),
              ubuntu::app_launch::app_info::Desktop(keyfile, "/").supportedOrientations());

    g_key_file_set_string(keyfile.get(), DESKTOP, "X-Ubuntu-Supported-Orientations", "portrait;landscape;");
    EXPECT_EQ((ubuntu::app_launch::Application::Info::Orientations {portrait: true, landscape: true, invertedPortrait: false, invertedLandscape: false}),
              ubuntu::app_launch::app_info::Desktop(keyfile, "/").supportedOrientations());

    g_key_file_set_string(keyfile.get(), DESKTOP, "X-Ubuntu-Supported-Orientations", "portrait;landscape;invertedportrait;invertedlandscape;");
    EXPECT_EQ((ubuntu::app_launch::Application::Info::Orientations {portrait: true, landscape: true, invertedPortrait: true, invertedLandscape: true}),
              ubuntu::app_launch::app_info::Desktop(keyfile, "/").supportedOrientations());

    g_key_file_set_string(keyfile.get(), DESKTOP, "X-Ubuntu-Supported-Orientations", "PORTRAIT;");
    EXPECT_EQ((ubuntu::app_launch::Application::Info::Orientations {portrait: true, landscape: false, invertedPortrait: false, invertedLandscape: false}),
              ubuntu::app_launch::app_info::Desktop(keyfile, "/").supportedOrientations());

    g_key_file_set_string(keyfile.get(), DESKTOP, "X-Ubuntu-Supported-Orientations", "pOrTraIt;lANDscApE;inVErtEDpORtrAit;iNVErtEDLAnDsCapE;");
    EXPECT_EQ((ubuntu::app_launch::Application::Info::Orientations {portrait: true, landscape: true, invertedPortrait: true, invertedLandscape: true}),
              ubuntu::app_launch::app_info::Desktop(keyfile, "/").supportedOrientations());

    g_key_file_set_string(keyfile.get(), DESKTOP, "X-Ubuntu-Supported-Orientations", "primary;");
    EXPECT_EQ((ubuntu::app_launch::Application::Info::Orientations {portrait: false, landscape: false, invertedPortrait: false, invertedLandscape: false}),
              ubuntu::app_launch::app_info::Desktop(keyfile, "/").supportedOrientations());

    g_key_file_set_string(keyfile.get(), DESKTOP, "X-Ubuntu-Supported-Orientations", "foobar;primary;");
    EXPECT_EQ(defaultOrientations,
              ubuntu::app_launch::app_info::Desktop(keyfile, "/").supportedOrientations());
}
} //anonymous namespace
