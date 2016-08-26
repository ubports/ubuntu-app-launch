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
#include <numeric>

#include "eventually-fixture.h"
#include "snapd-mock.h"

#include "application-impl-click.h"
#include "application-impl-legacy.h"
#include "application-impl-libertine.h"
#include "application-impl-snap.h"
#include "application.h"
#include "registry.h"

class ListApps : public EventuallyFixture
{
protected:
    GTestDBus* testbus = nullptr;
    GDBusConnection* bus = nullptr;

    virtual void SetUp()
    {
        /* Ensure it is cleared */
        g_unlink(SNAPD_TEST_SOCKET);

        /* Click DB test mode */
        g_setenv("TEST_CLICK_DB", CMAKE_BINARY_DIR "/click-db-dir", TRUE);
        g_setenv("TEST_CLICK_USER", "test-user", TRUE);

        gchar* linkfarmpath = g_build_filename(CMAKE_SOURCE_DIR, "link-farm", nullptr);
        g_setenv("UBUNTU_APP_LAUNCH_LINK_FARM", linkfarmpath, TRUE);
        g_free(linkfarmpath);

        g_setenv("XDG_DATA_DIRS", CMAKE_SOURCE_DIR, TRUE);
        g_setenv("XDG_CACHE_HOME", CMAKE_SOURCE_DIR "/libertine-data", TRUE);
        g_setenv("XDG_DATA_HOME", CMAKE_SOURCE_DIR "/libertine-home", TRUE);

        g_setenv("UBUNTU_APP_LAUNCH_SNAPD_SOCKET", SNAPD_TEST_SOCKET, TRUE);
        g_setenv("UBUNTU_APP_LAUNCH_SNAP_BASEDIR", SNAP_BASEDIR, TRUE);
        g_setenv("UBUNTU_APP_LAUNCH_DISABLE_SNAPD_TIMEOUT", "You betcha!", TRUE);

        testbus = g_test_dbus_new(G_TEST_DBUS_NONE);
        g_test_dbus_up(testbus);

        bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
        g_dbus_connection_set_exit_on_close(bus, FALSE);
        g_object_add_weak_pointer(G_OBJECT(bus), (gpointer*)&bus);
    }

    virtual void TearDown()
    {
        g_unlink(SNAPD_TEST_SOCKET);

        g_object_unref(bus);

        g_test_dbus_down(testbus);
        g_clear_object(&testbus);

        ASSERT_EVENTUALLY_EQ(nullptr, bus);
    }

    bool findApp(const std::list<std::shared_ptr<ubuntu::app_launch::Application>>& apps, const std::string& appid)
    {
        return findApp(apps, ubuntu::app_launch::AppID::parse(appid));
    }

    bool findApp(const std::list<std::shared_ptr<ubuntu::app_launch::Application>>& apps,
                 const ubuntu::app_launch::AppID& appId)
    {
        for (auto app : apps)
        {
            if (app->appId() == appId)
            {
                return true;
            }
        }

        return false;
    }

    void printApps(const std::list<std::shared_ptr<ubuntu::app_launch::Application>>& apps)
    {
        g_debug("Got apps: %s",
                std::accumulate(apps.begin(), apps.end(), std::string{},
                                [](const std::string& prev, std::shared_ptr<ubuntu::app_launch::Application> app) {
                                    if (prev.empty())
                                    {
                                        return std::string(app->appId());
                                    }
                                    else
                                    {
                                        return prev + ", " + std::string(app->appId());
                                    }
                                })
                    .c_str());
    }
};

TEST_F(ListApps, ListClick)
{
    auto registry = std::make_shared<ubuntu::app_launch::Registry>();
    auto apps = ubuntu::app_launch::app_impls::Click::list(registry);

    printApps(apps);

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

    printApps(apps);

    EXPECT_EQ(1, apps.size());

    EXPECT_TRUE(findApp(apps, ubuntu::app_launch::AppID(ubuntu::app_launch::AppID::Package::from_raw({}),
                                                        ubuntu::app_launch::AppID::AppName::from_raw("no-exec"),
                                                        ubuntu::app_launch::AppID::Version::from_raw({}))));
}

TEST_F(ListApps, ListLibertine)
{
    auto registry = std::make_shared<ubuntu::app_launch::Registry>();
    auto apps = ubuntu::app_launch::app_impls::Libertine::list(registry);

    printApps(apps);

    EXPECT_EQ(2, apps.size());

    EXPECT_TRUE(findApp(apps, "container-name_test_0.0"));
    EXPECT_TRUE(findApp(apps, "container-name_user-app_0.0"));
}

static std::pair<std::string, std::string> interfaces{
    "GET /v2/interfaces HTTP/1.1\r\nHost: http\r\nAccept: */*\r\n\r\n",
    SnapdMock::httpJsonResponse(
        SnapdMock::snapdOkay(SnapdMock::interfacesJson({{"unity8", "unity8-package", {"foo", "bar"}},
                                                        {"unity7", "unity7-package", {"single", "multiple"}},
                                                        {"x11", "x11-package", {"multiple", "hidden"}}

        })))};
static std::pair<std::string, std::string> u8Package{
    "GET /v2/snaps/unity8-package HTTP/1.1\r\nHost: http\r\nAccept: */*\r\n\r\n",
    SnapdMock::httpJsonResponse(SnapdMock::snapdOkay(
        SnapdMock::packageJson("unity8-package", "active", "app", "1.2.3.4", "x123", {"foo", "bar"})))};
static std::pair<std::string, std::string> u7Package{
    "GET /v2/snaps/unity7-package HTTP/1.1\r\nHost: http\r\nAccept: */*\r\n\r\n",
    SnapdMock::httpJsonResponse(SnapdMock::snapdOkay(SnapdMock::packageJson(
        "unity7-package", "active", "app", "1.2.3.4", "x123", {"scope", "single", "multiple"})))};
static std::pair<std::string, std::string> x11Package{
    "GET /v2/snaps/x11-package HTTP/1.1\r\nHost: http\r\nAccept: */*\r\n\r\n",
    SnapdMock::httpJsonResponse(SnapdMock::snapdOkay(
        SnapdMock::packageJson("x11-package", "active", "app", "1.2.3.4", "x123", {"multiple", "hidden"})))};

TEST_F(ListApps, ListSnap)
{
    SnapdMock mock{SNAPD_TEST_SOCKET,
                   {interfaces, u7Package, u7Package, u7Package,      /* unity7 check */
                    interfaces, u8Package, u8Package, u8Package,      /* unity8 check */
                    interfaces, x11Package, x11Package, x11Package}}; /* x11 check */
    auto registry = std::make_shared<ubuntu::app_launch::Registry>();

    auto apps = ubuntu::app_launch::app_impls::Snap::list(registry);

    printApps(apps);

    mock.result();

    EXPECT_EQ(4, apps.size());
    EXPECT_TRUE(findApp(apps, "unity8-package_foo_x123"));
    EXPECT_TRUE(findApp(apps, "unity7-package_single_x123"));
    EXPECT_TRUE(findApp(apps, "unity7-package_multiple_x123"));
    EXPECT_TRUE(findApp(apps, "x11-package_multiple_x123"));

    EXPECT_FALSE(findApp(apps, "unity8-package_bar_x123"));
    EXPECT_FALSE(findApp(apps, "unity7-package_scope_x123"));
    EXPECT_FALSE(findApp(apps, "x11-package_hidden_x123"));
}

TEST_F(ListApps, ListAll)
{
    SnapdMock mock{SNAPD_TEST_SOCKET,
                   {interfaces, u7Package, u7Package, u7Package,      /* unity7 check */
                    interfaces, u8Package, u8Package, u8Package,      /* unity8 check */
                    interfaces, x11Package, x11Package, x11Package}}; /* x11 check */
    auto registry = std::make_shared<ubuntu::app_launch::Registry>();

    /* Get all the apps */
    auto apps = ubuntu::app_launch::Registry::installedApps(registry);

    printApps(apps);

    EXPECT_EQ(18, apps.size());
}
