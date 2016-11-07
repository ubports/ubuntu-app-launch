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
#include "registry.h"
#include "eventually-fixture.h"

class FailureTest : public EventuallyFixture
{
	private:
		GTestDBus * testbus = NULL;
		std::shared_ptr<ubuntu::app_launch::Registry> registry;

	protected:
		virtual void SetUp() {
			testbus = g_test_dbus_new(G_TEST_DBUS_NONE);
			g_test_dbus_up(testbus);

			registry = std::make_shared<ubuntu::app_launch::Registry>();
		}

		virtual void TearDown() {
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
	ubuntu::app_launch::Registry::appFailed().connect([&last_observer](std::shared_ptr<ubuntu::app_launch::Application> app, std::shared_ptr<ubuntu::app_launch::Application::Instance> instance, ubuntu::app_launch::Registry::FailureType type) {
		if (type == ubuntu::app_launch::Registry::FailureType::CRASH) {
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
	ubuntu::app_launch::Registry::appFailed().connect([&last_observer](std::shared_ptr<ubuntu::app_launch::Application> app, std::shared_ptr<ubuntu::app_launch::Application::Instance> instance, ubuntu::app_launch::Registry::FailureType type) {
		if (type == ubuntu::app_launch::Registry::FailureType::CRASH) {
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
	g_setenv("INSTANCE", "foo_bar_x123-1234", TRUE);

	std::string last_observer;
	ubuntu::app_launch::Registry::appFailed().connect([&last_observer](std::shared_ptr<ubuntu::app_launch::Application> app, std::shared_ptr<ubuntu::app_launch::Application::Instance> instance, ubuntu::app_launch::Registry::FailureType type) {
		if (type == ubuntu::app_launch::Registry::FailureType::CRASH) {
			last_observer = app->appId();
		}
	});

	/* Status based */
	ASSERT_TRUE(g_spawn_command_line_sync(APP_FAILED_TOOL, NULL, NULL, NULL, NULL));

	EXPECT_EVENTUALLY_EQ("foo_bar_x123", last_observer);
}

TEST_F(FailureTest, StartTest)
{
	g_setenv("JOB", "application-click", TRUE);
	g_setenv("INSTANCE", "foo", TRUE);
	g_unsetenv("EXIT_STATUS");
	g_unsetenv("EXIT_SIGNAL");

	std::string last_observer;
	ubuntu::app_launch::Registry::appFailed().connect([&last_observer](std::shared_ptr<ubuntu::app_launch::Application> app, std::shared_ptr<ubuntu::app_launch::Application::Instance> instance, ubuntu::app_launch::Registry::FailureType type) {
		if (type == ubuntu::app_launch::Registry::FailureType::START_FAILURE) {
			last_observer = app->appId();
		}
	});

	ASSERT_TRUE(g_spawn_command_line_sync(APP_FAILED_TOOL, NULL, NULL, NULL, NULL));

	EXPECT_EVENTUALLY_EQ("foo", last_observer);
}
