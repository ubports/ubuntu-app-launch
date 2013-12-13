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
#include <libdbustest/dbus-test.h>
#include <gio/gio.h>

class ExecUtil : public ::testing::Test
{
	protected:
		DbusTestService * service = NULL;
		DbusTestDbusMock * mock = NULL;
		GDBusConnection * bus = NULL;

	protected:
		virtual void SetUp() {
			g_setenv("UPSTART_JOB", "made-up-job", TRUE);
			g_setenv("XDG_DATA_DIRS", CMAKE_SOURCE_DIR, TRUE);
			const gchar * oldpath = g_getenv("PATH");
			gchar * newpath = g_strjoin(":", CMAKE_SOURCE_DIR, oldpath, NULL);
			g_setenv("PATH", newpath, TRUE);
			g_free(newpath);

			service = dbus_test_service_new(NULL);

			mock = dbus_test_dbus_mock_new("com.ubuntu.Upstart");

			DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/com/ubuntu/Upstart", "com.ubuntu.Upstart0_6", NULL);

			dbus_test_dbus_mock_object_add_method(mock, obj,
				"SetEnv",
				G_VARIANT_TYPE("(assb)"),
				NULL,
				"",
				NULL);

			dbus_test_service_add_task(service, DBUS_TEST_TASK(mock));
			dbus_test_service_start_tasks(service);

			bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
			g_dbus_connection_set_exit_on_close(bus, FALSE);
			g_object_add_weak_pointer(G_OBJECT(bus), (gpointer *)&bus);
		}

		virtual void TearDown() {
			g_clear_object(&mock);
			g_clear_object(&service);

			g_object_unref(bus);

			unsigned int cleartry = 0;
			while (bus != NULL && cleartry < 100) {
				g_usleep(100000);
				while (g_main_pending()) {
					g_main_iteration(TRUE);
				}
				cleartry++;
			}
		}
};

TEST_F(ExecUtil, ClickExec)
{
	DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/com/ubuntu/Upstart", "com.ubuntu.Upstart0_6", NULL);

	g_setenv("APP_ID", "com.test.good_application_1.2.3", TRUE);

	g_spawn_command_line_sync(CLICK_EXEC_TOOL, NULL, NULL, NULL, NULL);

	guint len = 0;
	const DbusTestDbusMockCall * calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "SetEnv", &len, NULL);

	ASSERT_EQ(11, len);
	ASSERT_NE(nullptr, calls);

	unsigned int i;

	bool got_app_isolation = false;
	bool got_cache_home = false;
	bool got_config_home = false;
	bool got_data_home = false;
	bool got_runtime_dir = false;
	bool got_data_dirs = false;
	bool got_temp_dir = false;
	bool got_shader_dir = false;
	bool got_app_dir = false;
	bool got_app_exec = false;
	bool got_app_desktop = false;

	for (i = 0; i < len; i++) {
		EXPECT_STREQ("SetEnv", calls[i].name);

		GVariant * envvar = g_variant_get_child_value(calls[i].params, 1);
		gchar * var = g_variant_dup_string(envvar, NULL);
		g_variant_unref(envvar);

		gchar * equal = g_strstr_len(var, -1, "=");
		ASSERT_NE(equal, nullptr);

		equal[0] = '\0';
		gchar * value = &(equal[1]);

		if (g_strcmp0(var, "UBUNTU_APPLICATION_ISOLATION") == 0) {
			EXPECT_STREQ("1", value);
			got_app_isolation = true;
		} else if (g_strcmp0(var, "XDG_CACHE_HOME") == 0) {
			got_cache_home = true;
		} else if (g_strcmp0(var, "XDG_CONFIG_HOME") == 0) {
			got_config_home = true;
		} else if (g_strcmp0(var, "XDG_DATA_HOME") == 0) {
			got_data_home = true;
		} else if (g_strcmp0(var, "XDG_RUNTIME_DIR") == 0) {
			got_runtime_dir = true;
		} else if (g_strcmp0(var, "XDG_DATA_DIRS") == 0) {
			EXPECT_TRUE(g_str_has_prefix(value, CMAKE_SOURCE_DIR "/click-app-dir:"));
			got_data_dirs = true;
		} else if (g_strcmp0(var, "TMPDIR") == 0) {
			EXPECT_TRUE(g_str_has_suffix(value, "com.test.good"));
			got_temp_dir = true;
		} else if (g_strcmp0(var, "__GL_SHADER_DISK_CACHE_PATH") == 0) {
			EXPECT_TRUE(g_str_has_suffix(value, "com.test.good"));
			got_shader_dir = true;
		} else if (g_strcmp0(var, "APP_DIR") == 0) {
			EXPECT_STREQ(CMAKE_SOURCE_DIR "/click-app-dir", value);
			got_app_dir = true;
		} else if (g_strcmp0(var, "APP_EXEC") == 0) {
			EXPECT_STREQ("foo", value);
			got_app_exec = true;
		} else if (g_strcmp0(var, "APP_DESKTOP_FILE") == 0) {
			got_app_desktop = true;
		} else {
			g_warning("Unknown variable! %s", var);
			EXPECT_TRUE(false);
		}

		g_free(var);
	}

	EXPECT_TRUE(got_app_isolation);
	EXPECT_TRUE(got_cache_home);
	EXPECT_TRUE(got_config_home);
	EXPECT_TRUE(got_data_home);
	EXPECT_TRUE(got_runtime_dir);
	EXPECT_TRUE(got_data_dirs);
	EXPECT_TRUE(got_temp_dir);
	EXPECT_TRUE(got_shader_dir);
	EXPECT_TRUE(got_app_dir);
	EXPECT_TRUE(got_app_exec);
	EXPECT_TRUE(got_app_desktop);
}

TEST_F(ExecUtil, DesktopExec)
{
	DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/com/ubuntu/Upstart", "com.ubuntu.Upstart0_6", NULL);

	g_setenv("APP_ID", "foo", TRUE);

	g_spawn_command_line_sync(DESKTOP_EXEC_TOOL, NULL, NULL, NULL, NULL);

	guint len = 0;
	const DbusTestDbusMockCall * calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "SetEnv", &len, NULL);

	ASSERT_EQ(3, len);
	ASSERT_NE(nullptr, calls);

	unsigned int i;

	bool got_app_exec = false;
	bool got_app_desktop = false;
	bool got_app_exec_policy = false;

	for (i = 0; i < len; i++) {
		EXPECT_STREQ("SetEnv", calls[i].name);

		GVariant * envvar = g_variant_get_child_value(calls[i].params, 1);
		gchar * var = g_variant_dup_string(envvar, NULL);
		g_variant_unref(envvar);

		gchar * equal = g_strstr_len(var, -1, "=");
		ASSERT_NE(equal, nullptr);

		equal[0] = '\0';
		gchar * value = &(equal[1]);

		if (g_strcmp0(var, "APP_EXEC") == 0) {
			EXPECT_STREQ("foo", value);
			got_app_exec = true;
		} else if (g_strcmp0(var, "APP_DESKTOP_FILE") == 0) {
			got_app_desktop = true;
		} else if (g_strcmp0(var, "APP_EXEC_POLICY") == 0) {
			EXPECT_STREQ("unconfined", value);
			got_app_exec_policy = true;
		} else {
			g_warning("Unknown variable! %s", var);
			EXPECT_TRUE(false);
		}

		g_free(var);
	}

	EXPECT_TRUE(got_app_exec);
	EXPECT_TRUE(got_app_desktop);
	EXPECT_TRUE(got_app_exec_policy);
}
