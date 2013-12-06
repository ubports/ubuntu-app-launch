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
		DbusTestService * service = NULL;
		DbusTestDbusMock * mock = NULL;
		GDBusConnection * bus = NULL;

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
			g_setenv("UPSTART_APP_LAUNCH_USE_SESSION", "1", TRUE);

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
				"else:\n"
				"	ret = dbus.ObjectPath('/com/test/application_legacy')\n",
				NULL);

			DbusTestDbusMockObject * jobobj = dbus_test_dbus_mock_get_object(mock, "/com/test/application_click", "com.ubuntu.Upstart0_6.Job", NULL);

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
			dbus_test_dbus_mock_object_add_property(mock, instobj,
				"processes",
				G_VARIANT_TYPE("a(si)"),
				g_variant_new_parsed("[('main', 1234)]"),
				NULL);

			DbusTestDbusMockObject * ljobobj = dbus_test_dbus_mock_get_object(mock, "/com/test/application_legacy", "com.ubuntu.Upstart0_6.Job", NULL);

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

			if (!found) {
				gchar * envstr = g_variant_print(env_array, FALSE);
				g_warning("Unable to find '%s' with value '%s' in '%s'", var, value, envstr);
				g_free(envstr);
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
	ASSERT_TRUE(check_env(env, "APP_URIS", "'http://ubuntu.com/' 'https://ubuntu.com/' 'file:///home/phablet/test.txt'"));
	g_variant_unref(env);

	return;
}

TEST_F(LibUAL, StopApplication)
{
	DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/com/test/application_click", "com.ubuntu.Upstart0_6.Job", NULL);

	ASSERT_TRUE(upstart_app_launch_stop_application("foo"));

	ASSERT_EQ(dbus_test_dbus_mock_object_check_method_call(mock, obj, "Stop", NULL, NULL), 1);

}

TEST_F(LibUAL, ApplicationPid)
{
	ASSERT_EQ(upstart_app_launch_get_primary_pid("foo"), 1234);
	ASSERT_EQ(upstart_app_launch_get_primary_pid("bar"), 5678);
	ASSERT_TRUE(upstart_app_launch_pid_in_app_id(1234, "foo"));
	ASSERT_FALSE(upstart_app_launch_pid_in_app_id(5678, "foo"));
}

TEST_F(LibUAL, ApplicationList)
{
	gchar ** apps = upstart_app_launch_list_running_apps();

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

	ASSERT_TRUE(upstart_app_launch_observer_add_app_started(observer_cb, &start_data));
	ASSERT_TRUE(upstart_app_launch_observer_add_app_stop(observer_cb, &stop_data));
	ASSERT_FALSE(upstart_app_launch_observer_add_app_failed(NULL, NULL)); /* Not yet implemented */

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
	ASSERT_TRUE(upstart_app_launch_observer_delete_app_started(observer_cb, &start_data));
	ASSERT_TRUE(upstart_app_launch_observer_delete_app_stop(observer_cb, &stop_data));
	ASSERT_FALSE(upstart_app_launch_observer_delete_app_failed(NULL, NULL)); /* Not yet implemented */
}

static GDBusMessage *
filter_starting (GDBusConnection * conn, GDBusMessage * message, gboolean incomming, gpointer user_data)
{
	if (g_strcmp0(g_dbus_message_get_member(message), "UnityStartingSignal") == 0) {
		unsigned int * count = static_cast<unsigned int *>(user_data);
		*count++;
		g_object_unref(message);
		return NULL;
	}

	return message;
}

static void
starting_observer (const gchar * appid, gpointer user_data)
{
	return;
}

TEST_F(LibUAL, StartingResponses)
{
	unsigned int starting_count = 0;
	GDBusConnection * session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	guint filter = g_dbus_connection_add_filter(session,
		filter_starting,
		&starting_count,
		NULL);

	ASSERT_TRUE(upstart_app_launch_observer_add_app_starting(starting_observer, NULL));


	ASSERT_TRUE(upstart_app_launch_observer_delete_app_starting(starting_observer, NULL));

	g_object_unref(session);
}
