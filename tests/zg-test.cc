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
#include <libdbustest/dbus-test.h>

TEST(ZGEvent, OpenTest)
{
	DbusTestService * service = dbus_test_service_new(NULL);

	DbusTestDbusMock * mock = dbus_test_dbus_mock_new("org.gnome.zeitgeist.Engine");
	DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/org/gnome/zeitgeist/log/activity", "org.gnome.zeitgeist.Log", NULL);

	dbus_test_dbus_mock_object_add_method(mock, obj,
		"InsertEvents",
		G_VARIANT_TYPE("a(asaasay)"),
		G_VARIANT_TYPE("au"),
		"ret = [ 0 ]",
		NULL);

	dbus_test_service_add_task(service, DBUS_TEST_TASK(mock));

	DbusTestProcess * zgevent = dbus_test_process_new(ZG_EVENT_TOOL);
	dbus_test_process_append_param(zgevent, "open");
	g_setenv("APP_ID", "foo", 1);
	dbus_test_task_set_wait_for(DBUS_TEST_TASK(zgevent), "org.gnome.zeitgeist.Engine");
	dbus_test_task_set_name(DBUS_TEST_TASK(zgevent), "ZGEvent");

	dbus_test_service_add_task(service, DBUS_TEST_TASK(zgevent));

	dbus_test_service_start_tasks(service);

	/* Give it time to send the event and exit */
	g_usleep(100000);
	while (g_main_pending()) {
		g_main_iteration(TRUE);
	}

	ASSERT_EQ(dbus_test_task_get_state(DBUS_TEST_TASK(zgevent)), DBUS_TEST_TASK_STATE_FINISHED);
	ASSERT_TRUE(dbus_test_task_passed(DBUS_TEST_TASK(zgevent)));

	guint numcalls = 0;
	const DbusTestDbusMockCall * calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "InsertEvents", &numcalls, NULL);

	ASSERT_NE(calls, nullptr);
	ASSERT_EQ(numcalls, 1);

	g_object_unref(zgevent);
	g_object_unref(mock);
	g_object_unref(service);
}
