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

#include <gtest/gtest.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <ubuntu-app-launch.h>
#include "eventually-fixture.h"

class FailureTest : public EventuallyFixture
{
	private:
		GTestDBus * testbus = NULL;

	protected:
		virtual void SetUp() {
			testbus = g_test_dbus_new(G_TEST_DBUS_NONE);
			g_test_dbus_up(testbus);
		}

		virtual void TearDown() {
			g_test_dbus_down(testbus);
			g_clear_object(&testbus);
			return;
		}
};

static void
failed_observer (const gchar * appid, UbuntuAppLaunchAppFailed reason, gpointer user_data)
{
	if (reason == UBUNTU_APP_LAUNCH_APP_FAILED_CRASH) {
		std::string * last = static_cast<std::string *>(user_data);
		*last = appid;
	}
	return;
}

TEST_F(FailureTest, CrashTest)
{
	g_setenv("EXIT_STATUS", "-100", TRUE);
	g_setenv("JOB", "application-click", TRUE);
	g_setenv("INSTANCE", "foo", TRUE);

	std::string last_observer;
	ASSERT_TRUE(ubuntu_app_launch_observer_add_app_failed(failed_observer, &last_observer));

	/* Status based */
	ASSERT_TRUE(g_spawn_command_line_sync(APP_FAILED_TOOL, NULL, NULL, NULL, NULL));

	EXPECT_EVENTUALLY_EQ("foo", last_observer);

	last_observer.clear();
	g_unsetenv("EXIT_STATUS");
	g_setenv("EXIT_SIGNAL", "KILL", TRUE);

	/* Signal based */
	ASSERT_TRUE(g_spawn_command_line_sync(APP_FAILED_TOOL, NULL, NULL, NULL, NULL));

	EXPECT_EVENTUALLY_EQ("foo", last_observer);

	ASSERT_TRUE(ubuntu_app_launch_observer_delete_app_failed(failed_observer, &last_observer));

	return;
}

TEST_F(FailureTest, LegacyTest)
{
	g_setenv("EXIT_STATUS", "-100", TRUE);
	g_setenv("JOB", "application-legacy", TRUE);
	g_setenv("INSTANCE", "foo-1234", TRUE);

	std::string last_observer;
	ASSERT_TRUE(ubuntu_app_launch_observer_add_app_failed(failed_observer, &last_observer));

	/* Status based */
	ASSERT_TRUE(g_spawn_command_line_sync(APP_FAILED_TOOL, NULL, NULL, NULL, NULL));

	EXPECT_EVENTUALLY_EQ("foo", last_observer);

	ASSERT_TRUE(ubuntu_app_launch_observer_delete_app_failed(failed_observer, &last_observer));

	return;
}

static void
failed_start_observer (const gchar * appid, UbuntuAppLaunchAppFailed reason, gpointer user_data)
{
	if (reason == UBUNTU_APP_LAUNCH_APP_FAILED_START_FAILURE) {
		std::string * last = static_cast<std::string *>(user_data);
		*last = appid;
	}
	return;
}

TEST_F(FailureTest, StartTest)
{
	g_setenv("JOB", "application-click", TRUE);
	g_setenv("INSTANCE", "foo", TRUE);
	g_unsetenv("EXIT_STATUS");
	g_unsetenv("EXIT_SIGNAL");

	std::string last_observer;
	ASSERT_TRUE(ubuntu_app_launch_observer_add_app_failed(failed_start_observer, &last_observer));

	ASSERT_TRUE(g_spawn_command_line_sync(APP_FAILED_TOOL, NULL, NULL, NULL, NULL));

	EXPECT_EVENTUALLY_EQ("foo", last_observer);

	ASSERT_TRUE(ubuntu_app_launch_observer_delete_app_failed(failed_start_observer, &last_observer));

	return;
}
