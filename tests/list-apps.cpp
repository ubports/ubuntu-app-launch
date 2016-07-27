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

#include "application.h"
#include "registry.h"

#include "application-impl-click.h"
#include "application-impl-legacy.h"
#include "application-impl-libertine.h"
#include "application-impl-snap.h"

#include "snapd-mock.h"

class ListApps : public ::testing::Test
{
protected:
    GDBusConnection* bus = NULL;

    virtual void SetUp()
    {
        /* Click DB test mode */
        g_setenv("TEST_CLICK_DB", "click-db-dir", TRUE);
        g_setenv("TEST_CLICK_USER", "test-user", TRUE);

        gchar* linkfarmpath = g_build_filename(CMAKE_SOURCE_DIR, "link-farm", NULL);
        g_setenv("UBUNTU_APP_LAUNCH_LINK_FARM", linkfarmpath, TRUE);
        g_free(linkfarmpath);

        g_setenv("XDG_DATA_DIRS", CMAKE_SOURCE_DIR, TRUE);
        g_setenv("XDG_CACHE_HOME", CMAKE_SOURCE_DIR "/libertine-data", TRUE);
        g_setenv("XDG_DATA_HOME", CMAKE_SOURCE_DIR "/libertine-home", TRUE);

        g_setenv("UBUNTU_APP_LAUNCH_SNAPD_SOCKET", SNAPD_TEST_SOCKET, TRUE);
        g_setenv("UBUNTU_APP_LAUNCH_SNAP_BASEDIR", SNAP_BASEDIR, TRUE);

        bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
        g_dbus_connection_set_exit_on_close(bus, FALSE);
        g_object_add_weak_pointer(G_OBJECT(bus), (gpointer*)&bus);
    }

    virtual void TearDown()
    {
        g_unlink(SNAPD_TEST_SOCKET);

        g_object_unref(bus);

        unsigned int cleartry = 0;
        while (bus != NULL && cleartry < 100)
        {
            pause(100);
            cleartry++;
        }
        ASSERT_EQ(nullptr, bus);
    }

    void pause(guint time = 0)
    {
        if (time > 0)
        {
            GMainLoop* mainloop = g_main_loop_new(NULL, FALSE);

            g_timeout_add(time,
                          [](gpointer pmainloop) -> gboolean {
                              g_main_loop_quit(static_cast<GMainLoop*>(pmainloop));
                              return G_SOURCE_REMOVE;
                          },
                          mainloop);

            g_main_loop_run(mainloop);

            g_main_loop_unref(mainloop);
        }

        while (g_main_pending())
        {
            g_main_iteration(TRUE);
        }
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

    EXPECT_EQ(0, apps.size());
}

TEST_F(ListApps, ListLegacy)
{
    auto registry = std::make_shared<ubuntu::app_launch::Registry>();
    auto apps = ubuntu::app_launch::app_impls::Legacy::list(registry);

    EXPECT_EQ(0, apps.size());
}

TEST_F(ListApps, ListLibertine)
{
    auto registry = std::make_shared<ubuntu::app_launch::Registry>();
    auto apps = ubuntu::app_launch::app_impls::Libertine::list(registry);

    EXPECT_EQ(0, apps.size());
}

TEST_F(ListApps, ListSnap)
{
    SnapdMock mock{
        SNAPD_TEST_SOCKET,
        {{"GET /v2/interfaces HTTP/1.1\r\nHost: http\r\nAccept: */*\r\n\r\n",
          SnapdMock::httpJsonResponse(
              SnapdMock::snapdOkay(SnapdMock::interfacesJson({{"unity8", "test-package", {"foo", "bar"}}})))},
         {"GET /v2/interfaces HTTP/1.1\r\nHost: http\r\nAccept: */*\r\n\r\n",
          SnapdMock::httpJsonResponse(
              SnapdMock::snapdOkay(SnapdMock::interfacesJson({{"unity8", "test-package", {"foo", "bar"}}})))},
         {"GET /v2/snaps/test-package HTTP/1.1\r\nHost: http\r\nAccept: */*\r\n\r\n",
          SnapdMock::httpJsonResponse(SnapdMock::snapdOkay(
              SnapdMock::packageJson("test-package", "active", "app", "1.2.3.4", "x123", {"foo", "bar"})))},
         {"GET /v2/snaps/test-package HTTP/1.1\r\nHost: http\r\nAccept: */*\r\n\r\n",
          SnapdMock::httpJsonResponse(SnapdMock::snapdOkay(
              SnapdMock::packageJson("test-package", "active", "app", "1.2.3.4", "x123", {"foo", "bar"})))},
         {"GET /v2/snaps/test-package HTTP/1.1\r\nHost: http\r\nAccept: */*\r\n\r\n",
          SnapdMock::httpJsonResponse(SnapdMock::snapdOkay(
              SnapdMock::packageJson("test-package", "active", "app", "1.2.3.4", "x123", {"foo", "bar"})))},
         {"GET /v2/interfaces HTTP/1.1\r\nHost: http\r\nAccept: */*\r\n\r\n",
          SnapdMock::httpJsonResponse(
              SnapdMock::snapdOkay(SnapdMock::interfacesJson({{"unity8", "test-package", {"foo", "bar"}}})))}}};
    auto registry = std::make_shared<ubuntu::app_launch::Registry>();

    auto apps = ubuntu::app_launch::app_impls::Snap::list(registry);

    mock.result();

    EXPECT_EQ(1, apps.size());
    EXPECT_TRUE(findApp(apps, "test-package_foo_x123"));
    EXPECT_FALSE(findApp(apps, "test-package_bar_x123"));
}

TEST_F(ListApps, ListAll)
{
    auto registry = std::make_shared<ubuntu::app_launch::Registry>();
    auto apps = ubuntu::app_launch::Registry::installedApps(registry);

    EXPECT_EQ(0, apps.size());
}
