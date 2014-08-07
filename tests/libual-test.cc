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
#include "ubuntu-app-launch.h"
#include "libdbustest/dbus-test.h"
}

class LibUAL : public ::testing::Test
{
	protected:
		DbusTestService * service = NULL;
		DbusTestDbusMock * mock = NULL;
		DbusTestDbusMock * cgmock = NULL;
		GDBusConnection * bus = NULL;
		std::string last_focus_appid;
		std::string last_resume_appid;
		guint resume_timeout = 0;

	private:
		static void focus_cb (const gchar * appid, gpointer user_data) {
			g_debug("Focus Callback: %s", appid);
			LibUAL * _this = static_cast<LibUAL *>(user_data);
			_this->last_focus_appid = appid;
		}

		static void resume_cb (const gchar * appid, gpointer user_data) {
			g_debug("Resume Callback: %s", appid);
			LibUAL * _this = static_cast<LibUAL *>(user_data);
			_this->last_resume_appid = appid;

			if (_this->resume_timeout > 0) {
				_this->pause(_this->resume_timeout);
			}
		}

	protected:
		/* Useful debugging stuff, but not on by default.  You really want to
		   not get all this noise typically */
		void debugConnection() {
			if (true) return;

			DbusTestBustle * bustle = dbus_test_bustle_new("test.bustle");
			dbus_test_service_add_task(service, DBUS_TEST_TASK(bustle));
			g_object_unref(bustle);

			DbusTestProcess * monitor = dbus_test_process_new("dbus-monitor");
			dbus_test_service_add_task(service, DBUS_TEST_TASK(monitor));
			g_object_unref(monitor);
		}

		virtual void SetUp() {
			gchar * linkfarmpath = g_build_filename(CMAKE_SOURCE_DIR, "link-farm", NULL);
			g_setenv("UBUNTU_APP_LAUNCH_LINK_FARM", linkfarmpath, TRUE);
			g_free(linkfarmpath);

			g_setenv("XDG_DATA_DIRS", CMAKE_SOURCE_DIR, TRUE);
			g_setenv("XDG_CACHE_HOME", CMAKE_SOURCE_DIR, TRUE);

			service = dbus_test_service_new(NULL);

			debugConnection();

			mock = dbus_test_dbus_mock_new("com.ubuntu.Upstart");

			DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/com/ubuntu/Upstart", "com.ubuntu.Upstart0_6", NULL);

			dbus_test_dbus_mock_object_add_method(mock, obj,
				"EmitEvent",
				G_VARIANT_TYPE("(sasb)"),
				NULL,
				"",
				NULL);

			dbus_test_dbus_mock_object_add_method(mock, obj,
				"GetJobByName",
				G_VARIANT_TYPE("s"),
				G_VARIANT_TYPE("o"),
				"if args[0] == 'application-click':\n"
				"	ret = dbus.ObjectPath('/com/test/application_click')\n"
				"elif args[0] == 'application-legacy':\n"
				"	ret = dbus.ObjectPath('/com/test/application_legacy')\n"
				"elif args[0] == 'untrusted-helper':\n"
				"	ret = dbus.ObjectPath('/com/test/untrusted/helper')\n",
				NULL);

			/* Click App */
			DbusTestDbusMockObject * jobobj = dbus_test_dbus_mock_get_object(mock, "/com/test/application_click", "com.ubuntu.Upstart0_6.Job", NULL);

			dbus_test_dbus_mock_object_add_method(mock, jobobj,
				"Start",
				G_VARIANT_TYPE("(asb)"),
				NULL,
				"if args[0][0] == 'APP_ID=foo':"
				"    raise dbus.exceptions.DBusException('Foo running', name='com.ubuntu.Upstart0_6.Error.AlreadyStarted')",
				NULL);

			dbus_test_dbus_mock_object_add_method(mock, jobobj,
				"Stop",
				G_VARIANT_TYPE("(asb)"),
				NULL,
				"",
				NULL);

			dbus_test_dbus_mock_object_add_method(mock, jobobj,
				"GetAllInstances",
				NULL,
				G_VARIANT_TYPE("ao"),
				"ret = [ dbus.ObjectPath('/com/test/app_instance') ]",
				NULL);

			DbusTestDbusMockObject * instobj = dbus_test_dbus_mock_get_object(mock, "/com/test/app_instance", "com.ubuntu.Upstart0_6.Instance", NULL);
			dbus_test_dbus_mock_object_add_property(mock, instobj,
				"name",
				G_VARIANT_TYPE_STRING,
				g_variant_new_string("foo"),
				NULL);
			gchar * process_var = g_strdup_printf("[('main', %d)]", getpid());
			dbus_test_dbus_mock_object_add_property(mock, instobj,
				"processes",
				G_VARIANT_TYPE("a(si)"),
				g_variant_new_parsed(process_var),
				NULL);
			g_free(process_var);

			/*  Legacy App */
			DbusTestDbusMockObject * ljobobj = dbus_test_dbus_mock_get_object(mock, "/com/test/application_legacy", "com.ubuntu.Upstart0_6.Job", NULL);

			dbus_test_dbus_mock_object_add_method(mock, ljobobj,
				"Start",
				G_VARIANT_TYPE("(asb)"),
				NULL,
				"",
				NULL);

			dbus_test_dbus_mock_object_add_method(mock, ljobobj,
				"Stop",
				G_VARIANT_TYPE("(asb)"),
				NULL,
				"",
				NULL);

			dbus_test_dbus_mock_object_add_method(mock, ljobobj,
				"GetAllInstances",
				NULL,
				G_VARIANT_TYPE("ao"),
				"ret = [ dbus.ObjectPath('/com/test/legacy_app_instance') ]",
				NULL);

			DbusTestDbusMockObject * linstobj = dbus_test_dbus_mock_get_object(mock, "/com/test/legacy_app_instance", "com.ubuntu.Upstart0_6.Instance", NULL);
			dbus_test_dbus_mock_object_add_property(mock, linstobj,
				"name",
				G_VARIANT_TYPE_STRING,
				g_variant_new_string("bar-2342345"),
				NULL);
			dbus_test_dbus_mock_object_add_property(mock, linstobj,
				"processes",
				G_VARIANT_TYPE("a(si)"),
				g_variant_new_parsed("[('main', 5678)]"),
				NULL);

			/*  Untrusted Helper */
			DbusTestDbusMockObject * uhelperobj = dbus_test_dbus_mock_get_object(mock, "/com/test/untrusted/helper", "com.ubuntu.Upstart0_6.Job", NULL);

			dbus_test_dbus_mock_object_add_method(mock, uhelperobj,
				"Start",
				G_VARIANT_TYPE("(asb)"),
				NULL,
				"",
				NULL);

			dbus_test_dbus_mock_object_add_method(mock, uhelperobj,
				"Stop",
				G_VARIANT_TYPE("(asb)"),
				NULL,
				"",
				NULL);

			dbus_test_dbus_mock_object_add_method(mock, uhelperobj,
				"GetAllInstances",
				NULL,
				G_VARIANT_TYPE("ao"),
				"ret = [ dbus.ObjectPath('/com/test/untrusted/helper/instance'), dbus.ObjectPath('/com/test/untrusted/helper/multi_instance') ]",
				NULL);

			DbusTestDbusMockObject * uhelperinstance = dbus_test_dbus_mock_get_object(mock, "/com/test/untrusted/helper/instance", "com.ubuntu.Upstart0_6.Instance", NULL);
			dbus_test_dbus_mock_object_add_property(mock, uhelperinstance,
				"name",
				G_VARIANT_TYPE_STRING,
				g_variant_new_string("untrusted-type::com.foo_bar_43.23.12"),
				NULL);

			DbusTestDbusMockObject * unhelpermulti = dbus_test_dbus_mock_get_object(mock, "/com/test/untrusted/helper/multi_instance", "com.ubuntu.Upstart0_6.Instance", NULL);
			dbus_test_dbus_mock_object_add_property(mock, unhelpermulti,
				"name",
				G_VARIANT_TYPE_STRING,
				g_variant_new_string("untrusted-type:24034582324132:com.bar_foo_8432.13.1"),
				NULL);

			/* Create the cgroup manager mock */
			cgmock = dbus_test_dbus_mock_new("org.test.cgmock");
			g_setenv("UBUNTU_APP_LAUNCH_CG_MANAGER_NAME", "org.test.cgmock", TRUE);

			DbusTestDbusMockObject * cgobject = dbus_test_dbus_mock_get_object(cgmock, "/org/linuxcontainers/cgmanager", "org.linuxcontainers.cgmanager0_0", NULL);
			dbus_test_dbus_mock_object_add_method(cgmock, cgobject,
				"GetTasks",
				G_VARIANT_TYPE("(ss)"),
				G_VARIANT_TYPE("ai"),
				"ret = [100, 200, 300]",
				NULL);

			/* Put it together */
			dbus_test_service_add_task(service, DBUS_TEST_TASK(mock));
			dbus_test_service_add_task(service, DBUS_TEST_TASK(cgmock));
			dbus_test_service_start_tasks(service);

			bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
			g_dbus_connection_set_exit_on_close(bus, FALSE);
			g_object_add_weak_pointer(G_OBJECT(bus), (gpointer *)&bus);

			/* Make sure we pretend the CG manager is just on our bus */
			g_setenv("UBUNTU_APP_LAUNCH_CG_MANAGER_SESSION_BUS", "YES", TRUE);

			ASSERT_TRUE(ubuntu_app_launch_observer_add_app_focus(focus_cb, this));
			ASSERT_TRUE(ubuntu_app_launch_observer_add_app_resume(resume_cb, this));
		}

		virtual void TearDown() {
			ubuntu_app_launch_observer_delete_app_focus(focus_cb, this);
			ubuntu_app_launch_observer_delete_app_resume(resume_cb, this);

			g_clear_object(&mock);
			g_clear_object(&cgmock);
			g_clear_object(&service);

			g_object_unref(bus);

			unsigned int cleartry = 0;
			while (bus != NULL && cleartry < 100) {
				pause(100);
				cleartry++;
			}
			ASSERT_EQ(bus, nullptr);
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

					if (value != NULL) {
						gchar * combined = g_strdup_printf("%s=%s", var, value);
						if (g_strcmp0(envvar, combined) == 0) {
							found = true;
						}
						g_free(combined);
					} else {
						found = true;
					}
				}
			}

			if (!found) {
				gchar * envstr = g_variant_print(env_array, FALSE);
				g_warning("Unable to find '%s' with value '%s' in '%s'", var, value, envstr);
				g_free(envstr);
			}

			return found;
		}

		static gboolean pause_helper (gpointer pmainloop) {
			g_main_loop_quit(static_cast<GMainLoop *>(pmainloop));
			return G_SOURCE_REMOVE;
		}

		void pause (guint time) {
			if (time > 0) {
				GMainLoop * mainloop = g_main_loop_new(NULL, FALSE);
				g_timeout_add(time, pause_helper, mainloop);

				g_main_loop_run(mainloop);

				g_main_loop_unref(mainloop);
			}

			while (g_main_pending()) {
				g_main_iteration(TRUE);
			}
		}
};

TEST_F(LibUAL, StartApplication)
{
	DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/com/test/application_click", "com.ubuntu.Upstart0_6.Job", NULL);

	/* Basic make sure we can send the event */
	ASSERT_TRUE(ubuntu_app_launch_start_application("foolike", NULL));
	EXPECT_EQ(1, dbus_test_dbus_mock_object_check_method_call(mock, obj, "Start", NULL, NULL));

	ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(mock, obj, NULL));

	/* Now look at the details of the call */
	ASSERT_TRUE(ubuntu_app_launch_start_application("foolike", NULL));

	guint len = 0;
	const DbusTestDbusMockCall * calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Start", &len, NULL);
	EXPECT_NE(nullptr, calls);
	EXPECT_EQ(1, len);

	EXPECT_STREQ("Start", calls->name);
	EXPECT_EQ(2, g_variant_n_children(calls->params));

	GVariant * block = g_variant_get_child_value(calls->params, 1);
	EXPECT_TRUE(g_variant_get_boolean(block));
	g_variant_unref(block);

	GVariant * env = g_variant_get_child_value(calls->params, 0);
	EXPECT_TRUE(check_env(env, "APP_ID", "foolike"));
	g_variant_unref(env);

	ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(mock, obj, NULL));

	/* Let's pass some URLs */
	const gchar * urls[] = {
		"http://ubuntu.com/",
		"https://ubuntu.com/",
		"file:///home/phablet/test.txt",
		NULL
	};
	ASSERT_TRUE(ubuntu_app_launch_start_application("foolike", urls));

	len = 0;
	calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Start", &len, NULL);
	EXPECT_NE(nullptr, calls);
	EXPECT_EQ(1, len);

	env = g_variant_get_child_value(calls->params, 0);
	EXPECT_TRUE(check_env(env, "APP_ID", "foolike"));
	EXPECT_TRUE(check_env(env, "APP_URIS", "'http://ubuntu.com/' 'https://ubuntu.com/' 'file:///home/phablet/test.txt'"));
	g_variant_unref(env);

	return;
}

TEST_F(LibUAL, StartApplicationTest)
{
	DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/com/test/application_click", "com.ubuntu.Upstart0_6.Job", NULL);

	ASSERT_TRUE(ubuntu_app_launch_start_application_test("foolike", NULL));

	guint len = 0;
	const DbusTestDbusMockCall * calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Start", &len, NULL);
	EXPECT_NE(nullptr, calls);
	EXPECT_EQ(1, len);

	EXPECT_STREQ("Start", calls->name);
	EXPECT_EQ(2, g_variant_n_children(calls->params));

	GVariant * block = g_variant_get_child_value(calls->params, 1);
	EXPECT_TRUE(g_variant_get_boolean(block));
	g_variant_unref(block);

	GVariant * env = g_variant_get_child_value(calls->params, 0);
	EXPECT_TRUE(check_env(env, "APP_ID", "foolike"));
	EXPECT_TRUE(check_env(env, "QT_LOAD_TESTABILITY", "1"));
	g_variant_unref(env);
}

TEST_F(LibUAL, StopApplication)
{
	DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/com/test/application_click", "com.ubuntu.Upstart0_6.Job", NULL);

	ASSERT_TRUE(ubuntu_app_launch_stop_application("foo"));

	ASSERT_EQ(dbus_test_dbus_mock_object_check_method_call(mock, obj, "Stop", NULL, NULL), 1);

}

TEST_F(LibUAL, ApplicationLog)
{
	gchar * click_log = ubuntu_app_launch_application_log_path("foo");
	EXPECT_STREQ(CMAKE_SOURCE_DIR "/upstart/application-click-foo.log", click_log);
	g_free(click_log);

	gchar * legacy_single = ubuntu_app_launch_application_log_path("single");
	EXPECT_STREQ(CMAKE_SOURCE_DIR "/upstart/application-legacy-single-.log", legacy_single);
	g_free(legacy_single);

	gchar * legacy_multiple = ubuntu_app_launch_application_log_path("bar");
	EXPECT_STREQ(CMAKE_SOURCE_DIR "/upstart/application-legacy-bar-2342345.log", legacy_multiple);
	g_free(legacy_multiple);
}

TEST_F(LibUAL, ApplicationPid)
{
	EXPECT_EQ(ubuntu_app_launch_get_primary_pid("foo"), getpid());
	EXPECT_EQ(ubuntu_app_launch_get_primary_pid("bar"), 5678);
	EXPECT_TRUE(ubuntu_app_launch_pid_in_app_id(100, "foo"));
	EXPECT_FALSE(ubuntu_app_launch_pid_in_app_id(101, "foo"));
}

TEST_F(LibUAL, ApplicationId)
{
	g_setenv("TEST_CLICK_DB", "click-db-dir", TRUE);
	g_setenv("TEST_CLICK_USER", "test-user", TRUE);

	/* Test with current-user-version, should return the version in the manifest */
	EXPECT_STREQ("com.test.good_application_1.2.3", ubuntu_app_launch_triplet_to_app_id("com.test.good", "application", "current-user-version"));

	/* Test with version specified, shouldn't even read the manifest */
	EXPECT_STREQ("com.test.good_application_1.2.4", ubuntu_app_launch_triplet_to_app_id("com.test.good", "application", "1.2.4"));

	/* Test with out a version or app, should return the version in the manifest */
	EXPECT_STREQ("com.test.good_application_1.2.3", ubuntu_app_launch_triplet_to_app_id("com.test.good", "first-listed-app", "current-user-version"));

	/* Test with a version or but wildcard app, should return the version in the manifest */
	EXPECT_STREQ("com.test.good_application_1.2.4", ubuntu_app_launch_triplet_to_app_id("com.test.good", "last-listed-app", "1.2.4"));

	/* Make sure we can select the app from a list correctly */
	EXPECT_STREQ("com.test.multiple_first_1.2.3", ubuntu_app_launch_triplet_to_app_id("com.test.multiple", "first-listed-app", NULL));
	EXPECT_STREQ("com.test.multiple_first_1.2.3", ubuntu_app_launch_triplet_to_app_id("com.test.multiple", NULL, NULL));
	EXPECT_STREQ("com.test.multiple_fifth_1.2.3", ubuntu_app_launch_triplet_to_app_id("com.test.multiple", "last-listed-app", NULL));
	EXPECT_EQ(nullptr, ubuntu_app_launch_triplet_to_app_id("com.test.multiple", "only-listed-app", NULL));
	EXPECT_STREQ("com.test.good_application_1.2.3", ubuntu_app_launch_triplet_to_app_id("com.test.good", "only-listed-app", NULL));

	/* A bunch that should be NULL */
	EXPECT_EQ(nullptr, ubuntu_app_launch_triplet_to_app_id("com.test.no-hooks", NULL, NULL));
	EXPECT_EQ(nullptr, ubuntu_app_launch_triplet_to_app_id("com.test.no-json", NULL, NULL));
	EXPECT_EQ(nullptr, ubuntu_app_launch_triplet_to_app_id("com.test.no-object", NULL, NULL));
	EXPECT_EQ(nullptr, ubuntu_app_launch_triplet_to_app_id("com.test.no-version", NULL, NULL));
}

TEST_F(LibUAL, AppIdParse)
{
	EXPECT_TRUE(ubuntu_app_launch_app_id_parse("com.ubuntu.test_test_123", NULL, NULL, NULL));
	EXPECT_FALSE(ubuntu_app_launch_app_id_parse("inkscape", NULL, NULL, NULL));
	EXPECT_FALSE(ubuntu_app_launch_app_id_parse("music-app", NULL, NULL, NULL));

	gchar * pkg;
	gchar * app;
	gchar * version;

	ASSERT_TRUE(ubuntu_app_launch_app_id_parse("com.ubuntu.test_test_123", &pkg, &app, &version));
	EXPECT_STREQ("com.ubuntu.test", pkg);
	EXPECT_STREQ("test", app);
	EXPECT_STREQ("123", version);

	g_free(pkg);
	g_free(app);
	g_free(version);

	return;
}

TEST_F(LibUAL, ApplicationList)
{
	gchar ** apps = ubuntu_app_launch_list_running_apps();

	ASSERT_NE(apps, nullptr);
	ASSERT_EQ(g_strv_length(apps), 2);

	/* Not enforcing order, but wanting to use the GTest functions
	   for "actually testing" so the errors look right. */
	if (g_strcmp0(apps[0], "foo") == 0) {
		ASSERT_STREQ(apps[0], "foo");
		ASSERT_STREQ(apps[1], "bar");
	} else {
		ASSERT_STREQ(apps[0], "bar");
		ASSERT_STREQ(apps[1], "foo");
	}

	g_strfreev(apps);
}

typedef struct {
	unsigned int count;
	const gchar * name;
} observer_data_t;

static void
observer_cb (const gchar * appid, gpointer user_data)
{
	observer_data_t * data = (observer_data_t *)user_data;

	if (data->name == NULL) {
		data->count++;
	} else if (g_strcmp0(data->name, appid) == 0) {
		data->count++;
	}
}

TEST_F(LibUAL, StartStopObserver)
{
	observer_data_t start_data = {
		.count = 0,
		.name = nullptr
	};
	observer_data_t stop_data = {
		.count = 0,
		.name = nullptr
	};

	ASSERT_TRUE(ubuntu_app_launch_observer_add_app_started(observer_cb, &start_data));
	ASSERT_TRUE(ubuntu_app_launch_observer_add_app_stop(observer_cb, &stop_data));

	DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/com/ubuntu/Upstart", "com.ubuntu.Upstart0_6", NULL);

	/* Basic start */
	dbus_test_dbus_mock_object_emit_signal(mock, obj,
		"EventEmitted",
		G_VARIANT_TYPE("(sas)"),
		g_variant_new_parsed("('started', ['JOB=application-click', 'INSTANCE=foo'])"),
		NULL
	);

	g_usleep(100000);
	while (g_main_pending())
		g_main_iteration(TRUE);

	ASSERT_EQ(start_data.count, 1);

	/* Basic stop */
	dbus_test_dbus_mock_object_emit_signal(mock, obj,
		"EventEmitted",
		G_VARIANT_TYPE("(sas)"),
		g_variant_new_parsed("('stopped', ['JOB=application-click', 'INSTANCE=foo'])"),
		NULL
	);

	g_usleep(100000);
	while (g_main_pending())
		g_main_iteration(TRUE);

	ASSERT_EQ(stop_data.count, 1);

	/* Start legacy */
	start_data.count = 0;
	start_data.name = "bar";

	dbus_test_dbus_mock_object_emit_signal(mock, obj,
		"EventEmitted",
		G_VARIANT_TYPE("(sas)"),
		g_variant_new_parsed("('started', ['JOB=application-legacy', 'INSTANCE=bar-234235'])"),
		NULL
	);

	g_usleep(100000);
	while (g_main_pending())
		g_main_iteration(TRUE);

	ASSERT_EQ(start_data.count, 1);

	/* Legacy stop */
	stop_data.count = 0;
	stop_data.name = "bar";

	dbus_test_dbus_mock_object_emit_signal(mock, obj,
		"EventEmitted",
		G_VARIANT_TYPE("(sas)"),
		g_variant_new_parsed("('stopped', ['JOB=application-legacy', 'INSTANCE=bar-9344321'])"),
		NULL
	);

	g_usleep(100000);
	while (g_main_pending())
		g_main_iteration(TRUE);

	ASSERT_EQ(stop_data.count, 1);

	/* Test Noise Start */
	start_data.count = 0;
	start_data.name = "foo";
	stop_data.count = 0;
	stop_data.name = "foo";

	/* A full lifecycle */
	dbus_test_dbus_mock_object_emit_signal(mock, obj,
		"EventEmitted",
		G_VARIANT_TYPE("(sas)"),
		g_variant_new_parsed("('starting', ['JOB=application-click', 'INSTANCE=foo'])"),
		NULL
	);
	dbus_test_dbus_mock_object_emit_signal(mock, obj,
		"EventEmitted",
		G_VARIANT_TYPE("(sas)"),
		g_variant_new_parsed("('started', ['JOB=application-click', 'INSTANCE=foo'])"),
		NULL
	);
	dbus_test_dbus_mock_object_emit_signal(mock, obj,
		"EventEmitted",
		G_VARIANT_TYPE("(sas)"),
		g_variant_new_parsed("('stopping', ['JOB=application-click', 'INSTANCE=foo'])"),
		NULL
	);
	dbus_test_dbus_mock_object_emit_signal(mock, obj,
		"EventEmitted",
		G_VARIANT_TYPE("(sas)"),
		g_variant_new_parsed("('stopped', ['JOB=application-click', 'INSTANCE=foo'])"),
		NULL
	);

	g_usleep(100000);
	while (g_main_pending())
		g_main_iteration(TRUE);

	/* Ensure we just signaled once for each */
	ASSERT_EQ(start_data.count, 1);
	ASSERT_EQ(stop_data.count, 1);


	/* Remove */
	ASSERT_TRUE(ubuntu_app_launch_observer_delete_app_started(observer_cb, &start_data));
	ASSERT_TRUE(ubuntu_app_launch_observer_delete_app_stop(observer_cb, &stop_data));
}

static GDBusMessage *
filter_starting (GDBusConnection * conn, GDBusMessage * message, gboolean incomming, gpointer user_data)
{
	if (g_strcmp0(g_dbus_message_get_member(message), "UnityStartingSignal") == 0) {
		unsigned int * count = static_cast<unsigned int *>(user_data);
		(*count)++;
		g_object_unref(message);
		return NULL;
	}

	return message;
}

static void
starting_observer (const gchar * appid, gpointer user_data)
{
	std::string * last = static_cast<std::string *>(user_data);
	*last = appid;
	return;
}

TEST_F(LibUAL, StartingResponses)
{
	std::string last_observer;
	unsigned int starting_count = 0;
	GDBusConnection * session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	guint filter = g_dbus_connection_add_filter(session,
		filter_starting,
		&starting_count,
		NULL);

	EXPECT_TRUE(ubuntu_app_launch_observer_add_app_starting(starting_observer, &last_observer));

	g_dbus_connection_emit_signal(session,
		NULL, /* destination */
		"/", /* path */
		"com.canonical.UbuntuAppLaunch", /* interface */
		"UnityStartingBroadcast", /* signal */
		g_variant_new("(s)", "foo"), /* params, the same */
		NULL);

	pause(100);

	EXPECT_EQ("foo", last_observer);
	EXPECT_EQ(1, starting_count);

	EXPECT_TRUE(ubuntu_app_launch_observer_delete_app_starting(starting_observer, &last_observer));

	g_dbus_connection_remove_filter(session, filter);
	g_object_unref(session);
}

TEST_F(LibUAL, AppIdTest)
{
	ASSERT_TRUE(ubuntu_app_launch_start_application("foo", NULL));
	pause(50); /* Ensure all the events come through */
	EXPECT_EQ("foo", this->last_focus_appid);
	EXPECT_EQ("foo", this->last_resume_appid);
}

GDBusMessage *
filter_func_good (GDBusConnection * conn, GDBusMessage * message, gboolean incomming, gpointer user_data)
{
	if (!incomming) {
		return message;
	}

	if (g_strcmp0(g_dbus_message_get_path(message), (gchar *)user_data) == 0) {
		GDBusMessage * reply = g_dbus_message_new_method_reply(message);
		g_dbus_connection_send_message(conn, reply, G_DBUS_SEND_MESSAGE_FLAGS_NONE, NULL, NULL);
		g_object_unref(message);
		return NULL;
	}

	return message;
}

TEST_F(LibUAL, UrlSendTest)
{
	GDBusConnection * session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	guint filter = g_dbus_connection_add_filter(session,
		filter_func_good,
		(gpointer)"/foo",
		NULL);

	const gchar * uris[] = {
		"http://www.test.com",
		NULL
	};
	ASSERT_TRUE(ubuntu_app_launch_start_application("foo", uris));
	pause(100); /* Ensure all the events come through */

	EXPECT_EQ("foo", this->last_focus_appid);
	EXPECT_EQ("foo", this->last_resume_appid);

	g_dbus_connection_remove_filter(session, filter);

	/* Send multiple resume responses to ensure we unsubscribe */
	/* Multiple to increase our chance of hitting a bad free in the middle,
	   fun with async! */
	int i;
	for (i = 0; i < 5; i++) {
		g_dbus_connection_emit_signal(session,
			NULL, /* destination */
			"/", /* path */
			"com.canonical.UbuntuAppLaunch", /* interface */
			"UnityResumeResponse", /* signal */
			g_variant_new("(s)", "foo"), /* params, the same */
			NULL);

		pause(50); /* Ensure all the events come through */
	}

	g_object_unref(session);
}

TEST_F(LibUAL, UrlSendNoObjectTest)
{
	const gchar * uris[] = {
		"http://www.test.com",
		NULL
	};

	ASSERT_TRUE(ubuntu_app_launch_start_application("foo", uris));
	pause(100); /* Ensure all the events come through */

	EXPECT_EQ("foo", this->last_focus_appid);
	EXPECT_EQ("foo", this->last_resume_appid);
}

TEST_F(LibUAL, UnityTimeoutTest)
{
	this->resume_timeout = 100;

	ASSERT_TRUE(ubuntu_app_launch_start_application("foo", NULL));
	pause(1000); /* Ensure all the events come through */
	EXPECT_EQ("foo", this->last_focus_appid);
	EXPECT_EQ("foo", this->last_resume_appid);
}

TEST_F(LibUAL, UnityTimeoutUriTest)
{
	this->resume_timeout = 200;

	const gchar * uris[] = {
		"http://www.test.com",
		NULL
	};

	ASSERT_TRUE(ubuntu_app_launch_start_application("foo", uris));
	pause(1000); /* Ensure all the events come through */
	EXPECT_EQ("foo", this->last_focus_appid);
	EXPECT_EQ("foo", this->last_resume_appid);
}

GDBusMessage *
filter_respawn (GDBusConnection * conn, GDBusMessage * message, gboolean incomming, gpointer user_data)
{
	if (g_strcmp0(g_dbus_message_get_member(message), "UnityResumeResponse") == 0) {
		g_object_unref(message);
		return NULL;
	}

	return message;
}

TEST_F(LibUAL, UnityLostTest)
{
	GDBusConnection * session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	guint filter = g_dbus_connection_add_filter(session,
		filter_respawn,
		NULL,
		NULL);

	guint start = g_get_monotonic_time();

	const gchar * uris[] = {
		"http://www.test.com",
		NULL
	};

	ASSERT_TRUE(ubuntu_app_launch_start_application("foo", uris));

	guint end = g_get_monotonic_time();

	EXPECT_LT(end - start, 600 * 1000);

	pause(1000); /* Ensure all the events come through */

	EXPECT_EQ("foo", this->last_focus_appid);
	EXPECT_EQ("foo", this->last_resume_appid);

	g_dbus_connection_remove_filter(session, filter);
	g_object_unref(session);
}


TEST_F(LibUAL, LegacySingleInstance)
{
	DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/com/test/application_legacy", "com.ubuntu.Upstart0_6.Job", NULL);

	/* Check for a single-instance app */
	ASSERT_TRUE(ubuntu_app_launch_start_application("single", NULL));

	guint len = 0;
	const DbusTestDbusMockCall * calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Start", &len, NULL);
	EXPECT_NE(nullptr, calls);
	EXPECT_EQ(1, len);

	EXPECT_STREQ("Start", calls->name);
	EXPECT_EQ(2, g_variant_n_children(calls->params));

	GVariant * block = g_variant_get_child_value(calls->params, 1);
	EXPECT_TRUE(g_variant_get_boolean(block));
	g_variant_unref(block);

	GVariant * env = g_variant_get_child_value(calls->params, 0);
	EXPECT_TRUE(check_env(env, "APP_ID", "single"));
	EXPECT_TRUE(check_env(env, "INSTANCE_ID", ""));
	g_variant_unref(env);

	ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(mock, obj, NULL));

	/* Check for a multi-instance app */
	ASSERT_TRUE(ubuntu_app_launch_start_application("multiple", NULL));

	len = 0;
	calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Start", &len, NULL);
	EXPECT_NE(nullptr, calls);
	EXPECT_EQ(1, len);

	EXPECT_STREQ("Start", calls->name);
	EXPECT_EQ(2, g_variant_n_children(calls->params));

	block = g_variant_get_child_value(calls->params, 1);
	EXPECT_TRUE(g_variant_get_boolean(block));
	g_variant_unref(block);

	env = g_variant_get_child_value(calls->params, 0);
	EXPECT_TRUE(check_env(env, "APP_ID", "multiple"));
	EXPECT_FALSE(check_env(env, "INSTANCE_ID", ""));
	g_variant_unref(env);
}

static void
failed_observer (const gchar * appid, UbuntuAppLaunchAppFailed reason, gpointer user_data)
{
	if (reason == UBUNTU_APP_LAUNCH_APP_FAILED_CRASH) {
		std::string * last = static_cast<std::string *>(user_data);
		*last = appid;
	}
	return;
}

TEST_F(LibUAL, FailingObserver)
{
	std::string last_observer;
	GDBusConnection * session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);

	EXPECT_TRUE(ubuntu_app_launch_observer_add_app_failed(failed_observer, &last_observer));

	g_dbus_connection_emit_signal(session,
		NULL, /* destination */
		"/", /* path */
		"com.canonical.UbuntuAppLaunch", /* interface */
		"ApplicationFailed", /* signal */
		g_variant_new("(ss)", "foo", "crash"), /* params, the same */
		NULL);

	pause(100);

	EXPECT_EQ("foo", last_observer);

	last_observer.clear();

	g_dbus_connection_emit_signal(session,
		NULL, /* destination */
		"/", /* path */
		"com.canonical.UbuntuAppLaunch", /* interface */
		"ApplicationFailed", /* signal */
		g_variant_new("(ss)", "foo", "blahblah"), /* params, the same */
		NULL);

	pause(100);

	EXPECT_EQ("foo", last_observer);

	last_observer.clear();

	g_dbus_connection_emit_signal(session,
		NULL, /* destination */
		"/", /* path */
		"com.canonical.UbuntuAppLaunch", /* interface */
		"ApplicationFailed", /* signal */
		g_variant_new("(ss)", "foo", "start-failure"), /* params, the same */
		NULL);

	pause(100);

	EXPECT_TRUE(last_observer.empty());

	EXPECT_TRUE(ubuntu_app_launch_observer_delete_app_failed(failed_observer, &last_observer));

	g_object_unref(session);
}

TEST_F(LibUAL, StartHelper)
{
	DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/com/test/untrusted/helper", "com.ubuntu.Upstart0_6.Job", NULL);

	/* Basic make sure we can send the event */
	ASSERT_TRUE(ubuntu_app_launch_start_helper("untrusted-type", "foolike", NULL));
	EXPECT_EQ(1, dbus_test_dbus_mock_object_check_method_call(mock, obj, "Start", NULL, NULL));

	ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(mock, obj, NULL));

	/* Now look at the details of the call */
	ASSERT_TRUE(ubuntu_app_launch_start_helper("untrusted-type", "foolike", NULL));

	guint len = 0;
	const DbusTestDbusMockCall * calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Start", &len, NULL);
	EXPECT_NE(nullptr, calls);
	EXPECT_EQ(1, len);

	EXPECT_STREQ("Start", calls->name);
	EXPECT_EQ(2, g_variant_n_children(calls->params));

	GVariant * block = g_variant_get_child_value(calls->params, 1);
	EXPECT_TRUE(g_variant_get_boolean(block));
	g_variant_unref(block);

	GVariant * env = g_variant_get_child_value(calls->params, 0);
	EXPECT_TRUE(check_env(env, "APP_ID", "foolike"));
	EXPECT_TRUE(check_env(env, "HELPER_TYPE", "untrusted-type"));
	EXPECT_FALSE(check_env(env, "INSTANCE_ID", NULL));
	g_variant_unref(env);

	ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(mock, obj, NULL));

	/* Now check a multi out */ 
	gchar * instance_id = ubuntu_app_launch_start_multiple_helper("untrusted-type", "foolike", NULL);
	ASSERT_NE(nullptr, instance_id);
	g_debug("Multi-instance ID: %s", instance_id);

	len = 0;
	calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Start", &len, NULL);
	EXPECT_NE(nullptr, calls);
	EXPECT_EQ(1, len);

	EXPECT_STREQ("Start", calls->name);
	EXPECT_EQ(2, g_variant_n_children(calls->params));

	block = g_variant_get_child_value(calls->params, 1);
	EXPECT_TRUE(g_variant_get_boolean(block));
	g_variant_unref(block);

	env = g_variant_get_child_value(calls->params, 0);
	EXPECT_TRUE(check_env(env, "APP_ID", "foolike"));
	EXPECT_TRUE(check_env(env, "HELPER_TYPE", "untrusted-type"));
	EXPECT_TRUE(check_env(env, "INSTANCE_ID", instance_id));
	g_variant_unref(env);
	g_free(instance_id);

	ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(mock, obj, NULL));

	/* Let's pass some URLs */
	const gchar * urls[] = {
		"http://ubuntu.com/",
		"https://ubuntu.com/",
		"file:///home/phablet/test.txt",
		NULL
	};
	ASSERT_TRUE(ubuntu_app_launch_start_helper("untrusted-type", "foolike", urls));

	len = 0;
	calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Start", &len, NULL);
	EXPECT_NE(nullptr, calls);
	EXPECT_EQ(1, len);

	env = g_variant_get_child_value(calls->params, 0);
	EXPECT_TRUE(check_env(env, "APP_ID", "foolike"));
	EXPECT_TRUE(check_env(env, "APP_URIS", "'http://ubuntu.com/' 'https://ubuntu.com/' 'file:///home/phablet/test.txt'"));
	EXPECT_TRUE(check_env(env, "HELPER_TYPE", "untrusted-type"));
	EXPECT_FALSE(check_env(env, "INSTANCE_ID", NULL));
	g_variant_unref(env);

	return;
}

TEST_F(LibUAL, StopHelper)
{
	DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/com/test/untrusted/helper", "com.ubuntu.Upstart0_6.Job", NULL);

	/* Basic helper */
	ASSERT_TRUE(ubuntu_app_launch_stop_helper("untrusted-type", "foo"));

	ASSERT_EQ(dbus_test_dbus_mock_object_check_method_call(mock, obj, "Stop", NULL, NULL), 1);

	guint len = 0;
	const DbusTestDbusMockCall * calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Stop", &len, NULL);
	EXPECT_NE(nullptr, calls);
	EXPECT_EQ(1, len);

	EXPECT_STREQ("Stop", calls->name);
	EXPECT_EQ(2, g_variant_n_children(calls->params));

	GVariant * block = g_variant_get_child_value(calls->params, 1);
	EXPECT_TRUE(g_variant_get_boolean(block));
	g_variant_unref(block);

	GVariant * env = g_variant_get_child_value(calls->params, 0);
	EXPECT_TRUE(check_env(env, "APP_ID", "foo"));
	EXPECT_TRUE(check_env(env, "HELPER_TYPE", "untrusted-type"));
	EXPECT_FALSE(check_env(env, "INSTANCE_ID", NULL));
	g_variant_unref(env);

	ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(mock, obj, NULL));

	/* Multi helper */
	ASSERT_TRUE(ubuntu_app_launch_stop_multiple_helper("untrusted-type", "foo", "instance-me"));

	ASSERT_EQ(dbus_test_dbus_mock_object_check_method_call(mock, obj, "Stop", NULL, NULL), 1);

	len = 0;
	calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Stop", &len, NULL);
	EXPECT_NE(nullptr, calls);
	EXPECT_EQ(1, len);

	EXPECT_STREQ("Stop", calls->name);
	EXPECT_EQ(2, g_variant_n_children(calls->params));

	block = g_variant_get_child_value(calls->params, 1);
	EXPECT_TRUE(g_variant_get_boolean(block));
	g_variant_unref(block);

	env = g_variant_get_child_value(calls->params, 0);
	EXPECT_TRUE(check_env(env, "APP_ID", "foo"));
	EXPECT_TRUE(check_env(env, "HELPER_TYPE", "untrusted-type"));
	EXPECT_TRUE(check_env(env, "INSTANCE_ID", "instance-me"));
	g_variant_unref(env);

	ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(mock, obj, NULL));

	return;
}

TEST_F(LibUAL, HelperList)
{
	gchar ** blanktype = ubuntu_app_launch_list_helpers("not-a-type");

	EXPECT_NE(nullptr, blanktype);
	EXPECT_EQ(0, g_strv_length(blanktype));

	g_strfreev(blanktype);

	gchar ** goodtype = ubuntu_app_launch_list_helpers("untrusted-type");

	EXPECT_NE(nullptr, goodtype);
	EXPECT_EQ(2, g_strv_length(goodtype));

	if (g_strcmp0(goodtype[0], "com.foo_bar_43.23.12") == 0) {
		EXPECT_STREQ("com.foo_bar_43.23.12", goodtype[0]);
		EXPECT_STREQ("com.bar_foo_8432.13.1", goodtype[1]);
	} else {
		EXPECT_STREQ("com.foo_bar_43.23.12", goodtype[1]);
		EXPECT_STREQ("com.bar_foo_8432.13.1", goodtype[0]);
	}

	g_strfreev(goodtype);
}

TEST_F(LibUAL, HelperInstanceList)
{
	gchar ** blanktype = ubuntu_app_launch_list_helper_instances("not-a-type", "com.bar_foo_8432.13.1");

	EXPECT_NE(nullptr, blanktype);
	EXPECT_EQ(0, g_strv_length(blanktype));

	g_strfreev(blanktype);

	gchar ** goodtype = ubuntu_app_launch_list_helper_instances("untrusted-type", "com.bar_foo_8432.13.1");

	EXPECT_NE(nullptr, goodtype);
	EXPECT_EQ(1, g_strv_length(goodtype));
	EXPECT_STREQ("24034582324132", goodtype[0]);

	g_strfreev(goodtype);
}


typedef struct {
	unsigned int count;
	const gchar * appid;
	const gchar * type;
	const gchar * instance;
} helper_observer_data_t;

static void
helper_observer_cb (const gchar * appid, const gchar * instance, const gchar * type, gpointer user_data)
{
	helper_observer_data_t * data = (helper_observer_data_t *)user_data;

	if (g_strcmp0(data->appid, appid) == 0 &&
		g_strcmp0(data->type, type) == 0 &&
		g_strcmp0(data->instance, instance) == 0) {
		data->count++;
	}
}

TEST_F(LibUAL, StartStopHelperObserver)
{
	helper_observer_data_t start_data = {
		.count = 0,
		.appid = "com.foo_foo_1.2.3",
		.type = "my-type-is-scorpio",
		.instance = nullptr
	};
	helper_observer_data_t stop_data = {
		.count = 0,
		.appid = "com.bar_bar_44.32",
		.type = "my-type-is-libra",
		.instance = "1234"
	};

	ASSERT_TRUE(ubuntu_app_launch_observer_add_helper_started(helper_observer_cb, "my-type-is-scorpio", &start_data));
	ASSERT_TRUE(ubuntu_app_launch_observer_add_helper_stop(helper_observer_cb, "my-type-is-libra", &stop_data));

	DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/com/ubuntu/Upstart", "com.ubuntu.Upstart0_6", NULL);

	/* Basic start */
	dbus_test_dbus_mock_object_emit_signal(mock, obj,
		"EventEmitted",
		G_VARIANT_TYPE("(sas)"),
		g_variant_new_parsed("('started', ['JOB=untrusted-helper', 'INSTANCE=my-type-is-scorpio::com.foo_foo_1.2.3'])"),
		NULL
	);

	g_usleep(100000);
	while (g_main_pending())
		g_main_iteration(TRUE);

	ASSERT_EQ(start_data.count, 1);

	/* Basic stop */
	dbus_test_dbus_mock_object_emit_signal(mock, obj,
		"EventEmitted",
		G_VARIANT_TYPE("(sas)"),
		g_variant_new_parsed("('stopped', ['JOB=untrusted-helper', 'INSTANCE=my-type-is-libra:1234:com.bar_bar_44.32'])"),
		NULL
	);

	g_usleep(100000);
	while (g_main_pending())
		g_main_iteration(TRUE);

	ASSERT_EQ(stop_data.count, 1);

	/* Remove */
	ASSERT_TRUE(ubuntu_app_launch_observer_delete_helper_started(helper_observer_cb, "my-type-is-scorpio", &start_data));
	ASSERT_TRUE(ubuntu_app_launch_observer_delete_helper_stop(helper_observer_cb, "my-type-is-libra", &stop_data));
}
