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

extern "C" {
#include "../helpers.h"
}

class HelperHandshakeTest : public ::testing::Test
{
	private:
		GTestDBus * testbus = NULL;

	protected:
		GMainLoop * mainloop = NULL;

		virtual void SetUp() {
			mainloop = g_main_loop_new(NULL, FALSE);
			testbus = g_test_dbus_new(G_TEST_DBUS_NONE);
			g_test_dbus_up(testbus);
		}

		virtual void TearDown() {
			g_test_dbus_down(testbus);
			g_object_unref(testbus);
			return;
		}

	public:
		GDBusMessage * FilterFunc (GDBusConnection * conn, GDBusMessage * message, gboolean incomming) {
			if (g_strcmp0(g_dbus_message_get_member(message), "UnityStartingBroadcast") == 0) {
				GVariant * body = g_dbus_message_get_body(message);
				GVariant * correct_body = g_variant_new("(s)", "fooapp");
				g_variant_ref_sink(correct_body);

				[body, correct_body]() {
					ASSERT_TRUE(g_variant_equal(body, correct_body));
				}();

				g_variant_unref(correct_body);
				g_main_loop_quit(mainloop);
			}

			return message;
		}

};

static GDBusMessage *
filter_func (GDBusConnection * conn, GDBusMessage * message, gboolean incomming, gpointer user_data) {
	return static_cast<HelperHandshakeTest *>(user_data)->FilterFunc(conn, message, incomming);
}

TEST_F(HelperHandshakeTest, BaseHandshake)
{
	GDBusConnection * con = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	guint filter = g_dbus_connection_add_filter(con, filter_func, this, NULL);

	handshake_t * handshake = starting_handshake_start("fooapp");

	g_main_loop_run(mainloop);

	g_dbus_connection_remove_filter(con, filter);
	g_main_loop_unref(mainloop);

	g_dbus_connection_emit_signal(con,
		g_dbus_connection_get_unique_name(con), /* destination */
		"/", /* path */
		"com.canonical.UpstartAppLaunch", /* interface */
		"UnityStartingSignal", /* signal */
		g_variant_new("(s)", "fooapp"), /* params, the same */
		NULL);

	starting_handshake_wait(handshake);

	g_object_unref(con);

	return;
}
