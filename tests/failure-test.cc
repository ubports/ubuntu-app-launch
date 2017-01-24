/*
 * Copyright 2013 Canonical Ltd.
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

#include "eventually-fixture.h"
#include "registry.h"
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <gtest/gtest.h>

class FailureTest : public EventuallyFixture
{
private:
    GTestDBus* testbus = NULL;

protected:
    std::shared_ptr<ubuntu::app_launch::Registry> registry;

    virtual void SetUp() override
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

        testbus = g_test_dbus_new(G_TEST_DBUS_NONE);
        g_test_dbus_up(testbus);

        registry = std::make_shared<ubuntu::app_launch::Registry>();
    }

    virtual void TearDown() override
    {
        registry.reset();

        g_test_dbus_down(testbus);
        g_clear_object(&testbus);
    }
};

TEST_F(FailureTest, CrashTest)
{
    g_setenv("EXIT_STATUS", "-100", TRUE);
    g_setenv("JOB", "application-click", TRUE);
    g_setenv("INSTANCE", "foo", TRUE);

    std::string last_observer;
    ubuntu::app_launch::Registry::appFailed(registry).connect(
        [&last_observer](std::shared_ptr<ubuntu::app_launch::Application> app,
                         std::shared_ptr<ubuntu::app_launch::Application::Instance> instance,
                         ubuntu::app_launch::Registry::FailureType type) {
            if (type == ubuntu::app_launch::Registry::FailureType::CRASH)
            {
                last_observer = app->appId();
            }
        });

    /* Status based */
    ASSERT_TRUE(g_spawn_command_line_sync(APP_FAILED_TOOL, NULL, NULL, NULL, NULL));

    EXPECT_EVENTUALLY_EQ("foo", last_observer);

    last_observer.clear();
    g_unsetenv("EXIT_STATUS");
    g_setenv("EXIT_SIGNAL", "KILL", TRUE);

    /* Signal based */
    ASSERT_TRUE(g_spawn_command_line_sync(APP_FAILED_TOOL, NULL, NULL, NULL, NULL));

    EXPECT_EVENTUALLY_EQ("foo", last_observer);
}

TEST_F(FailureTest, LegacyTest)
{
    g_setenv("EXIT_STATUS", "-100", TRUE);
    g_setenv("JOB", "application-legacy", TRUE);
    g_setenv("INSTANCE", "foo-1234", TRUE);

    std::string last_observer;
    ubuntu::app_launch::Registry::appFailed(registry).connect(
        [&last_observer](std::shared_ptr<ubuntu::app_launch::Application> app,
                         std::shared_ptr<ubuntu::app_launch::Application::Instance> instance,
                         ubuntu::app_launch::Registry::FailureType type) {
            g_debug("Signal handler called");
            if (type == ubuntu::app_launch::Registry::FailureType::CRASH)
            {
                last_observer = app->appId();
            }
        });

    /* Status based */
    ASSERT_TRUE(g_spawn_command_line_sync(APP_FAILED_TOOL, NULL, NULL, NULL, NULL));

    EXPECT_EVENTUALLY_EQ("foo", last_observer);
}

TEST_F(FailureTest, SnapTest)
{
    g_setenv("EXIT_STATUS", "-100", TRUE);
    g_setenv("JOB", "application-snap", TRUE);
    g_setenv("INSTANCE", "com.test.good_application_1.2.3-1234", TRUE);

    std::string last_observer;
    ubuntu::app_launch::Registry::appFailed(registry).connect(
        [&last_observer](std::shared_ptr<ubuntu::app_launch::Application> app,
                         std::shared_ptr<ubuntu::app_launch::Application::Instance> instance,
                         ubuntu::app_launch::Registry::FailureType type) {
            if (type == ubuntu::app_launch::Registry::FailureType::CRASH)
            {
                last_observer = app->appId();
            }
        });

    /* Status based */
    ASSERT_TRUE(g_spawn_command_line_sync(APP_FAILED_TOOL, NULL, NULL, NULL, NULL));

    EXPECT_EVENTUALLY_EQ("com.test.good_application_1.2.3", last_observer);
}

TEST_F(FailureTest, StartTest)
{
    g_setenv("JOB", "application-click", TRUE);
    g_setenv("INSTANCE", "foo", TRUE);
    g_unsetenv("EXIT_STATUS");
    g_unsetenv("EXIT_SIGNAL");

    std::string last_observer;
    ubuntu::app_launch::Registry::appFailed(registry).connect(
        [&last_observer](std::shared_ptr<ubuntu::app_launch::Application> app,
                         std::shared_ptr<ubuntu::app_launch::Application::Instance> instance,
                         ubuntu::app_launch::Registry::FailureType type) {
            if (type == ubuntu::app_launch::Registry::FailureType::START_FAILURE)
            {
                last_observer = app->appId();
            }
        });

    ASSERT_TRUE(g_spawn_command_line_sync(APP_FAILED_TOOL, NULL, NULL, NULL, NULL));

    EXPECT_EVENTUALLY_EQ("foo", last_observer);
}
