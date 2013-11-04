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
#include <gio/gio.h>

extern "C" {
#include "upstart-app-launch.h"
#include "libdbustest/dbus-test.h"
}

class LibUAL : public ::testing::Test
{
	protected:
		virtual void SetUp() {
			g_setenv("UPSTART_APP_LAUNCH_USE_SESSION", "1", TRUE);

			/* NOTE: We're doing the bus in each test here */

			return;
		}
		virtual void TearDown() {

			return;
		}

};

TEST_F(LibUAL, BasicApplicationControl)
{
	DbusTestService * service = dbus_test_service_new(NULL);
	DbusTestDbusMock * mock = dbus_test_dbus_mock_new("com.ubuntu.Upstart");
	DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/com/ubuntu/Upstart", "com.ubuntu.Upstart0_6", NULL);

	dbus_test_dbus_mock_object_add_method(mock, obj,
		"EmitEvent",
		G_VARIANT_TYPE("(sasb)"),
		NULL,
		"",
		NULL);

	dbus_test_service_add_task(service, DBUS_TEST_TASK(mock));
	dbus_test_service_start_tasks(service);

	ASSERT_TRUE(upstart_app_launch_start_application("foo", NULL));
	ASSERT_EQ(dbus_test_dbus_mock_object_check_method_call(mock, obj, "EmitEvent", NULL, NULL), 1);

	ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(mock, obj));

	ASSERT_TRUE(upstart_app_launch_stop_application("foo"));
	ASSERT_EQ(dbus_test_dbus_mock_object_check_method_call(mock, obj, "EmitEvent", NULL, NULL), 1);

	g_object_unref(mock);
	g_object_unref(service);

	return;
}
