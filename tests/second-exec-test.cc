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
#include "../second-exec-core.h"
#include "upstart-app-launch.h"
#include "upstart-app-launch-mock.h"
}

class SecondExecTest : public ::testing::Test
{
	private:
		GTestDBus * testbus = NULL;

	protected:
		std::string last_focus_appid;
		std::string last_resume_appid;
		guint resume_timeout = 0;

	private:
		static void focus_cb (const gchar * appid, gpointer user_data) {
			SecondExecTest * _this = static_cast<SecondExecTest *>(user_data);
			_this->last_focus_appid = appid;
		}

		static void resume_cb (const gchar * appid, gpointer user_data) {
			SecondExecTest * _this = static_cast<SecondExecTest *>(user_data);
			_this->last_resume_appid = appid;

			if (_this->resume_timeout > 0) {
				_this->pause(_this->resume_timeout);
			}
		}

	protected:
		virtual void SetUp() {
			testbus = g_test_dbus_new(G_TEST_DBUS_NONE);
			g_test_dbus_up(testbus);

			upstart_app_launch_observer_add_app_focus(focus_cb, this);
			upstart_app_launch_observer_add_app_resume(resume_cb, this);

			return;
		}
		virtual void TearDown() {
			upstart_app_launch_observer_delete_app_focus(focus_cb, this);
			upstart_app_launch_observer_delete_app_resume(resume_cb, this);

			g_test_dbus_down(testbus);
			g_object_unref(testbus);

			return;
		}

		static gboolean pause_helper (gpointer pmainloop) {
			g_main_loop_quit((GMainLoop *)pmainloop);
			return G_SOURCE_REMOVE;
		}

		void pause (guint time) {
			if (time > 0) {
				GMainLoop * mainloop = g_main_loop_new(NULL, FALSE);
				guint timer = g_timeout_add(time, pause_helper, mainloop);

				g_main_loop_run(mainloop);

				g_source_remove(timer);
				g_main_loop_unref(mainloop);
			}

			while (g_main_pending()) {
				g_main_iteration(TRUE);
			}

			return;
		}
};

TEST_F(SecondExecTest, AppIdTest)
{
	ASSERT_TRUE(second_exec("foo", NULL));
	pause(0); /* Ensure all the events come through */
	ASSERT_STREQ(this->last_focus_appid.c_str(), "foo");
	ASSERT_STREQ(this->last_resume_appid.c_str(), "foo");
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

TEST_F(SecondExecTest, UrlSendTest)
{
	upstart_app_launch_mock_set_primary_pid(getpid());

	GDBusConnection * session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	guint filter = g_dbus_connection_add_filter(session,
		filter_func_good,
		(gpointer)"/foo",
		NULL);

	ASSERT_TRUE(second_exec("foo", "http://www.test.com"));
	pause(100); /* Ensure all the events come through */

	ASSERT_STREQ(this->last_focus_appid.c_str(), "foo");
	ASSERT_STREQ(this->last_resume_appid.c_str(), "foo");

	g_dbus_connection_remove_filter(session, filter);
	g_object_unref(session);
}

TEST_F(SecondExecTest, UrlSendNoObjectTest)
{
	upstart_app_launch_mock_set_primary_pid(getpid());

	ASSERT_TRUE(second_exec("foo", "http://www.test.com"));
	pause(100); /* Ensure all the events come through */

	ASSERT_STREQ(this->last_focus_appid.c_str(), "foo");
	ASSERT_STREQ(this->last_resume_appid.c_str(), "foo");
}

TEST_F(SecondExecTest, UnityTimeoutTest)
{
	this->resume_timeout = 100;

	ASSERT_TRUE(second_exec("foo", NULL));
	pause(100); /* Ensure all the events come through */
	ASSERT_STREQ(this->last_focus_appid.c_str(), "foo");
	ASSERT_STREQ(this->last_resume_appid.c_str(), "foo");
}

TEST_F(SecondExecTest, UnityTimeoutUriTest)
{
	this->resume_timeout = 200;

	ASSERT_TRUE(second_exec("foo", "http://www.test.com"));
	pause(100); /* Ensure all the events come through */
	ASSERT_STREQ(this->last_focus_appid.c_str(), "foo");
	ASSERT_STREQ(this->last_resume_appid.c_str(), "foo");
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

TEST_F(SecondExecTest, UnityLostTest)
{
	upstart_app_launch_mock_set_primary_pid(getpid());

	GDBusConnection * session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	guint filter = g_dbus_connection_add_filter(session,
		filter_respawn,
		NULL,
		NULL);

	guint start = g_get_monotonic_time();

	ASSERT_TRUE(second_exec("foo", "http://www.test.com"));

	guint end = g_get_monotonic_time();

	ASSERT_LT(end - start, 600 * 1000);

	pause(100); /* Ensure all the events come through */
	ASSERT_STREQ(this->last_focus_appid.c_str(), "foo");
	ASSERT_STREQ(this->last_resume_appid.c_str(), "foo");

	g_dbus_connection_remove_filter(session, filter);
	g_object_unref(session);
}


