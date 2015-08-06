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
#include <libubuntu-app-launch/ubuntu-app-launch.h>

class ExecUtil : public ::testing::Test
{
	protected:
		DbusTestService * service = NULL;
		DbusTestDbusMock * mock = NULL;
		GDBusConnection * bus = NULL;

	protected:
		static void starting_cb (const gchar * appid, gpointer user_data) {
			g_debug("I'm too sexy to callback");
		}

		virtual void SetUp() {
			g_setenv("UPSTART_JOB", "made-up-job", TRUE);
			g_setenv("XDG_DATA_DIRS", CMAKE_SOURCE_DIR, TRUE);
			g_setenv("XDG_CACHE_HOME", CMAKE_SOURCE_DIR "/libertine-data", TRUE);
			g_setenv("UBUNTU_APP_LAUNCH_LIBERTINE_LAUNCH", "libertine-launch", TRUE);

			service = dbus_test_service_new(NULL);

			mock = dbus_test_dbus_mock_new("com.ubuntu.Upstart");

			DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/com/ubuntu/Upstart", "com.ubuntu.Upstart0_6", NULL);

			dbus_test_dbus_mock_object_add_method(mock, obj,
				"GetJobByName",
				G_VARIANT_TYPE("s"),
				G_VARIANT_TYPE("o"),
				"ret = dbus.ObjectPath('/com/test/job')",
				NULL);

			DbusTestDbusMockObject * jobobj = dbus_test_dbus_mock_get_object(mock, "/com/test/job", "com.ubuntu.Upstart0_6.Job", NULL);

			dbus_test_dbus_mock_object_add_method(mock, jobobj,
				"Start",
				G_VARIANT_TYPE("(asb)"),
				NULL,
				"",
				NULL);

			dbus_test_service_add_task(service, DBUS_TEST_TASK(mock));
			dbus_test_service_start_tasks(service);

			bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
			g_dbus_connection_set_exit_on_close(bus, FALSE);
			g_object_add_weak_pointer(G_OBJECT(bus), (gpointer *)&bus);

			/* Make the handshake clear faster */
			ubuntu_app_launch_observer_add_app_starting(starting_cb, NULL);
		}

		virtual void TearDown() {
			ubuntu_app_launch_observer_delete_app_starting(starting_cb, NULL);

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
	DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/com/test/job", "com.ubuntu.Upstart0_6.Job", NULL);

	g_setenv("TEST_CLICK_DB", "click-db-dir", TRUE);
	g_setenv("TEST_CLICK_USER", "test-user", TRUE);
	g_setenv("UBUNTU_APP_LAUNCH_LINK_FARM", CMAKE_SOURCE_DIR "/link-farm", TRUE);

	ASSERT_TRUE(ubuntu_app_launch_start_application("com.test.good_application_1.2.3", NULL));

	guint len = 0;
	const DbusTestDbusMockCall * calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Start", &len, NULL);

	ASSERT_EQ(1, len);
	ASSERT_NE(nullptr, calls);
	ASSERT_STREQ("Start", calls[0].name);

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
	bool got_app_id = false;
	bool got_app_pid = false;
	bool got_app_desktop_path = false;

#define APP_DIR CMAKE_SOURCE_DIR "/click-root-dir/.click/users/test-user/com.test.good"

	GVariant * envarray = g_variant_get_child_value(calls[0].params, 0);
	GVariantIter iter;
	g_variant_iter_init(&iter, envarray);
	gchar * envvar = NULL;

	while (g_variant_iter_loop(&iter, "s", &envvar)) {
		gchar * var = g_strdup(envvar);

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
			EXPECT_TRUE(g_str_has_prefix(value, APP_DIR ":"));
			got_data_dirs = true;
		} else if (g_strcmp0(var, "TMPDIR") == 0) {
			EXPECT_TRUE(g_str_has_suffix(value, "com.test.good"));
			got_temp_dir = true;
		} else if (g_strcmp0(var, "__GL_SHADER_DISK_CACHE_PATH") == 0) {
			EXPECT_TRUE(g_str_has_suffix(value, "com.test.good"));
			got_shader_dir = true;
		} else if (g_strcmp0(var, "APP_DIR") == 0) {
			EXPECT_STREQ(APP_DIR, value);
			got_app_dir = true;
		} else if (g_strcmp0(var, "APP_EXEC") == 0) {
			EXPECT_STREQ("foo", value);
			got_app_exec = true;
		} else if (g_strcmp0(var, "APP_ID") == 0) {
			EXPECT_STREQ("com.test.good_application_1.2.3", value);
			got_app_id = true;
		} else if (g_strcmp0(var, "APP_LAUNCHER_PID") == 0) {
			EXPECT_EQ(getpid(), atoi(value));
			got_app_pid = true;
		} else if (g_strcmp0(var, "APP_DESKTOP_FILE_PATH") == 0) {
			EXPECT_STREQ(APP_DIR "/application.desktop", value);
			got_app_desktop_path = true;
		} else {
			g_warning("Unknown variable! %s", var);
			EXPECT_TRUE(false);
		}

		g_free(var);
	}

	g_variant_unref(envarray);

#undef APP_DIR

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
	EXPECT_TRUE(got_app_id);
	EXPECT_TRUE(got_app_pid);
	EXPECT_TRUE(got_app_desktop_path);
}

TEST_F(ExecUtil, DesktopExec)
{
	DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/com/test/job", "com.ubuntu.Upstart0_6.Job", NULL);

	ASSERT_TRUE(ubuntu_app_launch_start_application("foo", NULL));

	guint len = 0;
	const DbusTestDbusMockCall * calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Start", &len, NULL);

	ASSERT_EQ(1, len);
	ASSERT_NE(nullptr, calls);
	ASSERT_STREQ("Start", calls[0].name);

	unsigned int i;

	bool got_app_exec = false;
	bool got_app_desktop_path = false;
	bool got_app_exec_policy = false;
	bool got_app_id = false;
	bool got_app_pid = false;
	bool got_instance_id = false;

	GVariant * envarray = g_variant_get_child_value(calls[0].params, 0);
	GVariantIter iter;
	g_variant_iter_init(&iter, envarray);
	gchar * envvar = NULL;

	while (g_variant_iter_loop(&iter, "s", &envvar)) {
		gchar * var = g_strdup(envvar);

		gchar * equal = g_strstr_len(var, -1, "=");
		ASSERT_NE(equal, nullptr);

		equal[0] = '\0';
		gchar * value = &(equal[1]);

		if (g_strcmp0(var, "APP_EXEC") == 0) {
			EXPECT_STREQ("foo", value);
			got_app_exec = true;
		} else if (g_strcmp0(var, "APP_DESKTOP_FILE_PATH") == 0) {
			EXPECT_STREQ(CMAKE_SOURCE_DIR "/applications/foo.desktop", value);
			got_app_desktop_path = true;
		} else if (g_strcmp0(var, "APP_EXEC_POLICY") == 0) {
			EXPECT_STREQ("unconfined", value);
			got_app_exec_policy = true;
		} else if (g_strcmp0(var, "APP_ID") == 0) {
			EXPECT_STREQ("foo", value);
			got_app_id = true;
		} else if (g_strcmp0(var, "APP_LAUNCHER_PID") == 0) {
			EXPECT_EQ(getpid(), atoi(value));
			got_app_pid = true;
		} else if (g_strcmp0(var, "INSTANCE_ID") == 0) {
			got_instance_id = true;
		} else {
			g_warning("Unknown variable! %s", var);
			EXPECT_TRUE(false);
		}

		g_free(var);
	}

	g_variant_unref(envarray);

	EXPECT_TRUE(got_app_exec);
	EXPECT_TRUE(got_app_desktop_path);
	EXPECT_TRUE(got_app_exec_policy);
	EXPECT_TRUE(got_app_id);
	EXPECT_TRUE(got_app_pid);
	EXPECT_TRUE(got_instance_id);
}

TEST_F(ExecUtil, DesktopMir)
{
	DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/com/test/job", "com.ubuntu.Upstart0_6.Job", NULL);

	ASSERT_TRUE(ubuntu_app_launch_start_application("xmir", NULL));

	guint len = 0;
	const DbusTestDbusMockCall * calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Start", &len, NULL);

	ASSERT_EQ(1, len);
	ASSERT_NE(nullptr, calls);
	ASSERT_STREQ("Start", calls[0].name);

	unsigned int i;

	bool got_mir = false;

	GVariant * envarray = g_variant_get_child_value(calls[0].params, 0);
	GVariantIter iter;
	g_variant_iter_init(&iter, envarray);
	gchar * envvar = NULL;

	while (g_variant_iter_loop(&iter, "s", &envvar)) {
		gchar * var = g_strdup(envvar);

		gchar * equal = g_strstr_len(var, -1, "=");
		ASSERT_NE(equal, nullptr);

		equal[0] = '\0';
		gchar * value = &(equal[1]);

		if (g_strcmp0(var, "APP_XMIR_ENABLE") == 0) {
			EXPECT_STREQ("1", value);
			got_mir = true;
		}

		g_free(var);
	}

	g_variant_unref(envarray);

	EXPECT_TRUE(got_mir);
}

TEST_F(ExecUtil, DesktopNoMir)
{
	DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/com/test/job", "com.ubuntu.Upstart0_6.Job", NULL);

	ASSERT_TRUE(ubuntu_app_launch_start_application("noxmir", NULL));

	guint len = 0;
	const DbusTestDbusMockCall * calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Start", &len, NULL);

	ASSERT_EQ(1, len);
	ASSERT_NE(nullptr, calls);
	ASSERT_STREQ("Start", calls[0].name);

	unsigned int i;

	bool got_mir = false;

	GVariant * envarray = g_variant_get_child_value(calls[0].params, 0);
	GVariantIter iter;
	g_variant_iter_init(&iter, envarray);
	gchar * envvar = NULL;

	while (g_variant_iter_loop(&iter, "s", &envvar)) {
		gchar * var = g_strdup(envvar);

		gchar * equal = g_strstr_len(var, -1, "=");
		ASSERT_NE(equal, nullptr);

		equal[0] = '\0';
		gchar * value = &(equal[1]);

		if (g_strcmp0(var, "APP_XMIR_ENABLE") == 0) {
			EXPECT_STREQ("0", value);
			got_mir = true;
		}

		g_free(var);
	}

	g_variant_unref(envarray);

	EXPECT_TRUE(got_mir);
}

TEST_F(ExecUtil, ClickMir)
{
	DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/com/test/job", "com.ubuntu.Upstart0_6.Job", NULL);

	g_setenv("TEST_CLICK_DB", "click-db-dir", TRUE);
	g_setenv("TEST_CLICK_USER", "test-user", TRUE);
	g_setenv("UBUNTU_APP_LAUNCH_LINK_FARM", CMAKE_SOURCE_DIR "/link-farm", TRUE);

	ASSERT_TRUE(ubuntu_app_launch_start_application("com.test.mir_mir_1", NULL));

	guint len = 0;
	const DbusTestDbusMockCall * calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Start", &len, NULL);

	ASSERT_EQ(1, len);
	ASSERT_NE(nullptr, calls);
	ASSERT_STREQ("Start", calls[0].name);

	unsigned int i;

	bool got_mir = false;

	GVariant * envarray = g_variant_get_child_value(calls[0].params, 0);
	GVariantIter iter;
	g_variant_iter_init(&iter, envarray);
	gchar * envvar = NULL;

	while (g_variant_iter_loop(&iter, "s", &envvar)) {
		gchar * var = g_strdup(envvar);

		gchar * equal = g_strstr_len(var, -1, "=");
		ASSERT_NE(equal, nullptr);

		equal[0] = '\0';
		gchar * value = &(equal[1]);

		if (g_strcmp0(var, "APP_XMIR_ENABLE") == 0) {
			EXPECT_STREQ("1", value);
			got_mir = true;
		}

		g_free(var);
	}

	g_variant_unref(envarray);

	EXPECT_TRUE(got_mir);
}

TEST_F(ExecUtil, ClickNoMir)
{
	DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/com/test/job", "com.ubuntu.Upstart0_6.Job", NULL);

	g_setenv("TEST_CLICK_DB", "click-db-dir", TRUE);
	g_setenv("TEST_CLICK_USER", "test-user", TRUE);
	g_setenv("UBUNTU_APP_LAUNCH_LINK_FARM", CMAKE_SOURCE_DIR "/link-farm", TRUE);

	ASSERT_TRUE(ubuntu_app_launch_start_application("com.test.mir_nomir_1", NULL));

	guint len = 0;
	const DbusTestDbusMockCall * calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Start", &len, NULL);

	ASSERT_EQ(1, len);
	ASSERT_NE(nullptr, calls);
	ASSERT_STREQ("Start", calls[0].name);

	unsigned int i;

	bool got_mir = false;

	GVariant * envarray = g_variant_get_child_value(calls[0].params, 0);
	GVariantIter iter;
	g_variant_iter_init(&iter, envarray);
	gchar * envvar = NULL;

	while (g_variant_iter_loop(&iter, "s", &envvar)) {
		gchar * var = g_strdup(envvar);

		gchar * equal = g_strstr_len(var, -1, "=");
		ASSERT_NE(equal, nullptr);

		equal[0] = '\0';
		gchar * value = &(equal[1]);

		if (g_strcmp0(var, "APP_XMIR_ENABLE") == 0) {
			EXPECT_STREQ("0", value);
			got_mir = true;
		}

		g_free(var);
	}

	g_variant_unref(envarray);

	EXPECT_TRUE(got_mir);
}

TEST_F(ExecUtil, LibertineExec)
{
	DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/com/test/job", "com.ubuntu.Upstart0_6.Job", NULL);

	ASSERT_TRUE(ubuntu_app_launch_start_application("container-name_test_0.0", NULL));

	guint len = 0;
	const DbusTestDbusMockCall * calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Start", &len, NULL);

	ASSERT_EQ(1, len);
	ASSERT_NE(nullptr, calls);
	ASSERT_STREQ("Start", calls[0].name);

	unsigned int i;

	bool got_app_exec = false;
	bool got_app_exec_policy = false;
	bool got_app_id = false;
	bool got_app_pid = false;
	bool got_instance_id = false;
	bool got_mir = false;

	GVariant * envarray = g_variant_get_child_value(calls[0].params, 0);
	GVariantIter iter;
	g_variant_iter_init(&iter, envarray);
	gchar * envvar = NULL;

	while (g_variant_iter_loop(&iter, "s", &envvar)) {
		gchar * var = g_strdup(envvar);

		gchar * equal = g_strstr_len(var, -1, "=");
		ASSERT_NE(equal, nullptr);

		equal[0] = '\0';
		gchar * value = &(equal[1]);

		if (g_strcmp0(var, "APP_EXEC") == 0) {
			EXPECT_STREQ("libertine-launch \"container-name\" test", value);
			got_app_exec = true;
		} else if (g_strcmp0(var, "APP_EXEC_POLICY") == 0) {
			EXPECT_STREQ("unconfined", value);
			got_app_exec_policy = true;
		} else if (g_strcmp0(var, "APP_ID") == 0) {
			EXPECT_STREQ("container-name_test_0.0", value);
			got_app_id = true;
		} else if (g_strcmp0(var, "APP_LAUNCHER_PID") == 0) {
			EXPECT_EQ(getpid(), atoi(value));
			got_app_pid = true;
		} else if (g_strcmp0(var, "INSTANCE_ID") == 0) {
			got_instance_id = true;
		} else if (g_strcmp0(var, "APP_XMIR_ENABLE") == 0) {
			EXPECT_STREQ("1", value);
			got_mir = true;
		} else {
			g_warning("Unknown variable! %s", var);
			EXPECT_TRUE(false);
		}

		g_free(var);
	}

	g_variant_unref(envarray);

	EXPECT_TRUE(got_app_exec);
	EXPECT_TRUE(got_app_exec_policy);
	EXPECT_TRUE(got_app_id);
	EXPECT_TRUE(got_app_pid);
	EXPECT_TRUE(got_instance_id);
	EXPECT_TRUE(got_mir);
}
