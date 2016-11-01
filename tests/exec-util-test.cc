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

#include <map>
#include <functional>

#include <gtest/gtest.h>
#include <libdbustest/dbus-test.h>
#include <gio/gio.h>
#include <libubuntu-app-launch/ubuntu-app-launch.h>
#include <libubuntu-app-launch/registry.h>

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
			g_setenv("XDG_DATA_HOME", CMAKE_SOURCE_DIR "/libertine-home", TRUE);
			g_setenv("UBUNTU_APP_LAUNCH_LIBERTINE_LAUNCH", "libertine-launch", TRUE);
			g_setenv("UBUNTU_APP_LAUNCH_SNAPD_SOCKET", "/this/should/not/exist", TRUE);
			g_setenv("UBUNTU_APP_LAUNCH_SYSTEMD_PATH", "/this/should/not/exist", TRUE);

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
			ubuntu::app_launch::Registry::clearDefault();

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

		inline void StartCheckEnv (const std::string& appid, std::map<std::string, std::function<void(const gchar *)>> enums) {
			std::map<std::string, bool> env_found;
			DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/com/test/job", "com.ubuntu.Upstart0_6.Job", nullptr);

			g_setenv("TEST_CLICK_DB", "click-db-dir", TRUE);
			g_setenv("TEST_CLICK_USER", "test-user", TRUE);
			g_setenv("UBUNTU_APP_LAUNCH_LINK_FARM", CMAKE_SOURCE_DIR "/link-farm", TRUE);

			ASSERT_TRUE(ubuntu_app_launch_start_application(appid.c_str(), nullptr));

			guint len = 0;
			const DbusTestDbusMockCall * calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Start", &len, nullptr);

			ASSERT_EQ(1, len);
			ASSERT_NE(nullptr, calls);
			ASSERT_STREQ("Start", calls[0].name);

			GVariant * envarray = g_variant_get_child_value(calls[0].params, 0);
			GVariantIter iter;
			g_variant_iter_init(&iter, envarray);
			gchar * envvar = NULL;

			while (g_variant_iter_loop(&iter, "s", &envvar)) {
				g_debug("Looking at variable: %s", envvar);
				gchar * var = g_strdup(envvar);

				gchar * equal = g_strstr_len(var, -1, "=");
				ASSERT_NE(equal, nullptr);

				equal[0] = '\0';
				gchar * value = &(equal[1]);

				/* Test the variable */
				auto varfunc = enums[var];
				EXPECT_NE(nullptr, varfunc);
				if (varfunc) {
					varfunc(value);
				} else {
					g_warning("Unable to find function for '%s'", var);
				}

				/* Mark it as found */
				env_found[var] = true;

				g_free(var);
			}

			g_variant_unref(envarray);

			for(auto enumval : enums) {
				EXPECT_TRUE(env_found[enumval.first]);
				if (!env_found[enumval.first]) {
					g_warning("Unable to find enum %s", enumval.first.c_str());
				}
			}
		}
};

static void
nocheck (const gchar *)
{
}

TEST_F(ExecUtil, ClickExec)
{
#define APP_DIR CMAKE_SOURCE_DIR "/click-root-dir/.click/users/test-user/com.test.good"

	StartCheckEnv("com.test.good_application_1.2.3", {
		{"UBUNTU_APPLICATION_ISOLATION", [](const gchar * value) {
			EXPECT_STREQ("1", value); }},
		{"XDG_CACHE_HOME", nocheck},
		{"XDG_CONFIG_HOME", nocheck},
		{"XDG_DATA_HOME", nocheck},
		{"XDG_RUNTIME_DIR", nocheck},
		{"XDG_DATA_DIRS", [](const gchar * value) {
			EXPECT_TRUE(g_str_has_prefix(value, APP_DIR ":")); }},
		{"TMPDIR", [](const gchar * value) {
			EXPECT_TRUE(g_str_has_suffix(value, "com.test.good")); }},
		{"__GL_SHADER_DISK_CACHE_PATH", [](const gchar * value) {
			EXPECT_TRUE(g_str_has_suffix(value, "com.test.good")); }},
		{"APP_DIR", [](const gchar * value) {
			EXPECT_STREQ(APP_DIR, value); }},
		{"APP_EXEC", [](const gchar * value) {
			EXPECT_STREQ("grep", value); }},
		{"APP_ID", [](const gchar * value) {
			EXPECT_STREQ("com.test.good_application_1.2.3", value); }},
		{"APP_EXEC_POLICY", [](const gchar * value) {
			EXPECT_STREQ("com.test.good_application_1.2.3", value); }},
		{"APP_LAUNCHER_PID", [](const gchar * value) {
			EXPECT_EQ(getpid(), atoi(value)); }},
		{"APP_DESKTOP_FILE_PATH", [](const gchar * value) {
			EXPECT_STREQ(APP_DIR "/application.desktop", value); }},
		{"APP_XMIR_ENABLE", [](const gchar * value) {
			EXPECT_STREQ("0", value); }},
		{"QML2_IMPORT_PATH", nocheck},
	});

#undef APP_DIR
}

TEST_F(ExecUtil, DesktopExec)
{
	StartCheckEnv("foo", {
		{"APP_EXEC", [](const gchar * value) {
			EXPECT_STREQ("foo", value); }},
		{"APP_DESKTOP_FILE_PATH", [](const gchar * value) {
			EXPECT_STREQ(CMAKE_SOURCE_DIR "/applications/foo.desktop", value); }},
		{"APP_EXEC_POLICY", [](const gchar * value) {
			EXPECT_STREQ("unconfined", value); }},
		{"APP_ID", [](const gchar * value) {
			EXPECT_STREQ("foo", value); }},
		{"INSTANCE_ID", nocheck},
		{"APP_LAUNCHER_PID", [](const gchar * value) {
			EXPECT_EQ(getpid(), atoi(value)); }},
		{"APP_XMIR_ENABLE", [](const gchar * value) {
			EXPECT_STREQ("0", value); }},
	});
}

TEST_F(ExecUtil, DesktopMir)
{
	StartCheckEnv("xmir", {
		{"APP_EXEC", [](const gchar * value) {
			EXPECT_STREQ("libertine-launch xfoo", value); }},
		{"APP_DESKTOP_FILE_PATH", [](const gchar * value) {
			EXPECT_STREQ(CMAKE_SOURCE_DIR "/applications/xmir.desktop", value); }},
		{"APP_EXEC_POLICY", [](const gchar * value) {
			EXPECT_STREQ("unconfined", value); }},
		{"APP_ID", [](const gchar * value) {
			EXPECT_STREQ("xmir", value); }},
		{"INSTANCE_ID", nocheck},
		{"APP_LAUNCHER_PID", [](const gchar * value) {
			EXPECT_EQ(getpid(), atoi(value)); }},
		{"APP_XMIR_ENABLE", [](const gchar * value) {
			EXPECT_STREQ("1", value); }},
	});
}

TEST_F(ExecUtil, DesktopNoMir)
{
	StartCheckEnv("noxmir", {
		{"APP_EXEC", [](const gchar * value) {
			EXPECT_STREQ("noxmir", value); }},
		{"APP_DESKTOP_FILE_PATH", [](const gchar * value) {
			EXPECT_STREQ(CMAKE_SOURCE_DIR "/applications/noxmir.desktop", value); }},
		{"APP_EXEC_POLICY", [](const gchar * value) {
			EXPECT_STREQ("unconfined", value); }},
		{"APP_ID", [](const gchar * value) {
			EXPECT_STREQ("noxmir", value); }},
		{"INSTANCE_ID", nocheck},
		{"APP_LAUNCHER_PID", [](const gchar * value) {
			EXPECT_EQ(getpid(), atoi(value)); }},
		{"APP_XMIR_ENABLE", [](const gchar * value) {
			EXPECT_STREQ("0", value); }},
	});
}

TEST_F(ExecUtil, ClickMir)
{
	StartCheckEnv("com.test.mir_mir_1", {
		{"UBUNTU_APPLICATION_ISOLATION", nocheck},
		{"XDG_CACHE_HOME", nocheck},
		{"XDG_CONFIG_HOME", nocheck},
		{"XDG_DATA_HOME", nocheck},
		{"XDG_RUNTIME_DIR", nocheck},
		{"XDG_DATA_DIRS", nocheck},
		{"TMPDIR", nocheck},
		{"__GL_SHADER_DISK_CACHE_PATH", nocheck},
		{"APP_DIR", nocheck},
		{"APP_EXEC", nocheck},
		{"APP_ID", [](const gchar * value) {
			EXPECT_STREQ("com.test.mir_mir_1", value); }},
		{"APP_EXEC_POLICY", [](const gchar * value) {
			EXPECT_STREQ("com.test.mir_mir_1", value); }},
		{"APP_LAUNCHER_PID", nocheck},
		{"APP_DESKTOP_FILE_PATH", nocheck},
		{"APP_XMIR_ENABLE", [](const gchar * value) {
			EXPECT_STREQ("1", value); }},
		{"QML2_IMPORT_PATH", nocheck},
	});
}

TEST_F(ExecUtil, ClickNoMir)
{
	StartCheckEnv("com.test.mir_nomir_1", {
		{"UBUNTU_APPLICATION_ISOLATION", nocheck},
		{"XDG_CACHE_HOME", nocheck},
		{"XDG_CONFIG_HOME", nocheck},
		{"XDG_DATA_HOME", nocheck},
		{"XDG_RUNTIME_DIR", nocheck},
		{"XDG_DATA_DIRS", nocheck},
		{"TMPDIR", nocheck},
		{"__GL_SHADER_DISK_CACHE_PATH", nocheck},
		{"APP_DIR", nocheck},
		{"APP_EXEC", nocheck},
		{"APP_ID", [](const gchar * value) {
			EXPECT_STREQ("com.test.mir_nomir_1", value); }},
		{"APP_EXEC_POLICY", [](const gchar * value) {
			EXPECT_STREQ("com.test.mir_nomir_1", value); }},
		{"APP_LAUNCHER_PID", nocheck},
		{"APP_DESKTOP_FILE_PATH", nocheck},
		{"APP_XMIR_ENABLE", [](const gchar * value) {
			EXPECT_STREQ("0", value); }},
		{"QML2_IMPORT_PATH", nocheck},
	});
}

TEST_F(ExecUtil, LibertineExec)
{
	StartCheckEnv("container-name_test_0.0", {
		{"APP_EXEC", [](const gchar * value) {
			EXPECT_STREQ("libertine-launch \"--id=container-name\" test", value); }},
		{"APP_EXEC_POLICY", [](const gchar * value) {
			EXPECT_STREQ("unconfined", value); }},
		{"APP_ID", [](const gchar * value) {
			EXPECT_STREQ("container-name_test_0.0", value); }},
		{"APP_LAUNCHER_PID", [](const gchar * value) {
			EXPECT_EQ(getpid(), atoi(value)); }},
		{"INSTANCE_ID", nocheck},
		{"APP_XMIR_ENABLE", [](const gchar * value) {
			EXPECT_STREQ("1", value); }},
	});
}

TEST_F(ExecUtil, LibertineExecUser)
{
	StartCheckEnv("container-name_user-app_0.0", {
		{"APP_EXEC", [](const gchar * value) {
			EXPECT_STREQ("libertine-launch \"--id=container-name\" user-app", value); }},
		{"APP_EXEC_POLICY", [](const gchar * value) {
			EXPECT_STREQ("unconfined", value); }},
		{"APP_ID", [](const gchar * value) {
			EXPECT_STREQ("container-name_user-app_0.0", value); }},
		{"APP_LAUNCHER_PID", [](const gchar * value) {
			EXPECT_EQ(getpid(), atoi(value)); }},
		{"INSTANCE_ID", nocheck},
		{"APP_XMIR_ENABLE", [](const gchar * value) {
			EXPECT_STREQ("1", value); }},
	});
}
