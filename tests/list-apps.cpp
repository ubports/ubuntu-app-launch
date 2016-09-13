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

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <gtest/gtest.h>

#include "eventually-fixture.h"

#include "application-impl-click.h"
#include "application-impl-legacy.h"
#include "application-impl-libertine.h"
#include "application.h"
#include "registry.h"

class ListApps : public EventuallyFixture
{
protected:
    GTestDBus* testbus = nullptr;
    GDBusConnection* bus = nullptr;

    virtual void SetUp()
    {
        /* Click DB test mode */
        g_setenv("TEST_CLICK_DB", CMAKE_BINARY_DIR "/click-db-dir", TRUE);
        g_setenv("TEST_CLICK_USER", "test-user", TRUE);

        gchar* linkfarmpath = g_build_filename(CMAKE_SOURCE_DIR, "link-farm", nullptr);
        g_setenv("UBUNTU_APP_LAUNCH_LINK_FARM", linkfarmpath, TRUE);
        g_free(linkfarmpath);

        g_setenv("XDG_DATA_DIRS", CMAKE_SOURCE_DIR, TRUE);
        g_setenv("XDG_CACHE_HOME", CMAKE_SOURCE_DIR "/libertine-data", TRUE);
        g_setenv("XDG_DATA_HOME", CMAKE_SOURCE_DIR "/libertine-home", TRUE);

        testbus = g_test_dbus_new(G_TEST_DBUS_NONE);
        g_test_dbus_up(testbus);

        bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
        g_dbus_connection_set_exit_on_close(bus, FALSE);
        g_object_add_weak_pointer(G_OBJECT(bus), (gpointer*)&bus);
    }

    virtual void TearDown()
    {
        g_object_unref(bus);

        g_test_dbus_down(testbus);
        g_clear_object(&testbus);

        ASSERT_EVENTUALLY_EQ(nullptr, bus);
    }

    bool findApp(const std::list<std::shared_ptr<ubuntu::app_launch::Application>>& apps, const std::string& appid)
    {
        auto appId = ubuntu::app_launch::AppID::parse(appid);

        for (auto app : apps)
        {
            if (app->appId() == appId)
            {
                return true;
            }
        }

        return false;
    }
};

TEST_F(ListApps, ListClick)
{
    auto registry = std::make_shared<ubuntu::app_launch::Registry>();
    auto apps = ubuntu::app_launch::app_impls::Click::list(registry);

    EXPECT_EQ(11, apps.size());

    EXPECT_TRUE(findApp(apps, "chatter.robert-ancell_chatter_2"));
    EXPECT_TRUE(findApp(apps, "com.test.bad-version_application_4.5.6"));
    EXPECT_TRUE(findApp(apps, "com.test.good_application_1.2.3"));
    EXPECT_TRUE(findApp(apps, "com.test.mir_mir_1"));
    EXPECT_TRUE(findApp(apps, "com.test.mir_nomir_1"));
    EXPECT_TRUE(findApp(apps, "com.test.multiple_first_1.2.3"));
    EXPECT_TRUE(findApp(apps, "com.test.multiple_second_1.2.3"));
    EXPECT_TRUE(findApp(apps, "com.test.multiple_third_1.2.3"));
    EXPECT_TRUE(findApp(apps, "com.test.multiple_fourth_1.2.3"));
    EXPECT_TRUE(findApp(apps, "com.test.multiple_fifth_1.2.3"));
    EXPECT_TRUE(findApp(apps, "com.test.no-app_no-application_1.2.3"));

    EXPECT_FALSE(findApp(apps, "com.test.no-hooks_application_1.2.3"));
    EXPECT_FALSE(findApp(apps, "com.test.no-json_application_1.2.3"));
    EXPECT_FALSE(findApp(apps, "com.test.no-object_application_1.2.3"));
    EXPECT_FALSE(findApp(apps, "com.test.no-version_application_1.2.3"));
}

TEST_F(ListApps, ListLegacy)
{
    auto registry = std::make_shared<ubuntu::app_launch::Registry>();
    auto apps = ubuntu::app_launch::app_impls::Legacy::list(registry);

    EXPECT_EQ(1, apps.size());
}

TEST_F(ListApps, ListLibertine)
{
    auto registry = std::make_shared<ubuntu::app_launch::Registry>();
    auto apps = ubuntu::app_launch::app_impls::Libertine::list(registry);

    EXPECT_EQ(2, apps.size());

    EXPECT_TRUE(findApp(apps, "container-name_test_0.0"));
    EXPECT_TRUE(findApp(apps, "container-name_user-app_0.0"));
}

TEST_F(ListApps, ListAll)
{
    auto registry = std::make_shared<ubuntu::app_launch::Registry>();

    /* Get all the apps */
    auto apps = ubuntu::app_launch::Registry::installedApps(registry);

    EXPECT_EQ(14, apps.size());
}
