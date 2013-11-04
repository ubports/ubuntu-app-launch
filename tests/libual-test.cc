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
		DbusTestService * service;
		DbusTestDbusMock * mock;

	protected:
		virtual void SetUp() {
			g_setenv("UPSTART_APP_LAUNCH_USE_SESSION", "1", TRUE);

			service = dbus_test_service_new(NULL);
			mock = dbus_test_dbus_mock_new("com.ubuntu.Upstart");

			DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/com/ubuntu/Upstart", "com.ubuntu.Upstart0_6", NULL);

			dbus_test_dbus_mock_object_add_method(mock, obj,
				"EmitEvent",
				G_VARIANT_TYPE("(sasb)"),
				NULL,
				"",
				NULL);

			dbus_test_service_add_task(service, DBUS_TEST_TASK(mock));
			dbus_test_service_start_tasks(service);

			return;
		}

		virtual void TearDown() {
			g_clear_object(&mock);
			g_clear_object(&service);

			return;
		}

		bool check_env (GVariant * env_array, const gchar * var, const gchar * value) {
			GVariantIter iter;
			g_variant_iter_init(&iter, env_array);
			gchar * envvar = NULL;
			bool found = false;

			while (g_variant_iter_loop(&iter, "s", &envvar)) {
				if (g_str_has_prefix(envvar, var)) {
					if (found) {
						g_warning("Found the env var more than once!");
						return false;
					}

					gchar * combined = g_strdup_printf("%s=%s", var, value);
					if (g_strcmp0(envvar, combined) == 0) {
						found = true;
					}
					g_free(combined);
				}
			}

			return found;
		}
};

TEST_F(LibUAL, StartApplication)
{
	DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/com/ubuntu/Upstart", "com.ubuntu.Upstart0_6", NULL);

	/* Basic make sure we can send the event */
	ASSERT_TRUE(upstart_app_launch_start_application("foo", NULL));
	ASSERT_EQ(dbus_test_dbus_mock_object_check_method_call(mock, obj, "EmitEvent", NULL, NULL), 1);

	ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(mock, obj, NULL));

	/* Now look at the details of the call */
	ASSERT_TRUE(upstart_app_launch_start_application("foo", NULL));

	guint len = 0;
	const DbusTestDbusMockCall * calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "EmitEvent", &len, NULL);
	ASSERT_NE(calls, nullptr);
	ASSERT_EQ(len, 1);

	ASSERT_STREQ(calls->name, "EmitEvent");
	ASSERT_EQ(g_variant_n_children(calls->params), 3);

	GVariant * name = g_variant_get_child_value(calls->params, 0);
	ASSERT_STREQ(g_variant_get_string(name, NULL), "application-start");
	g_variant_unref(name);

	GVariant * block = g_variant_get_child_value(calls->params, 2);
	ASSERT_FALSE(g_variant_get_boolean(block));
	g_variant_unref(block);

	GVariant * env = g_variant_get_child_value(calls->params, 1);
	ASSERT_TRUE(check_env(env, "APP_ID", "foo"));
	g_variant_unref(env);

	ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(mock, obj, NULL));

	/* Let's pass some URLs */
	const gchar * urls[] = {
		"http://ubuntu.com/",
		"https://ubuntu.com/",
		"file:///home/phablet/test.txt",
		NULL
	};
	ASSERT_TRUE(upstart_app_launch_start_application("foo", urls));

	len = 0;
	calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "EmitEvent", &len, NULL);
	ASSERT_NE(calls, nullptr);
	ASSERT_EQ(len, 1);

	env = g_variant_get_child_value(calls->params, 1);
	ASSERT_TRUE(check_env(env, "APP_ID", "foo"));
	gchar * joined = g_strjoinv(" ", (gchar **)urls);
	ASSERT_TRUE(check_env(env, "APP_URIS", joined));
	g_free(joined);
	g_variant_unref(env);


	return;
}

