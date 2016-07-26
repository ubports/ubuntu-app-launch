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
#include <gtest/gtest.h>

#include "application.h"
#include "registry.h"

class ListApps : public ::testing::Test
{
protected:
    GDBusConnection* bus = NULL;
    std::shared_ptr<ubuntu::app_launch::Registry> registry;

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

        g_setenv("UBUNTU_APP_LAUNCH_SNAPD_SOCKET", "/this/should/not/exist", TRUE);

        bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
        g_dbus_connection_set_exit_on_close(bus, FALSE);
        g_object_add_weak_pointer(G_OBJECT(bus), (gpointer*)&bus);

        registry = std::make_shared<ubuntu::app_launch::Registry>();
    }

    virtual void TearDown()
    {
        registry.reset();

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
};

TEST_F(ListApps, Init)
{
}
