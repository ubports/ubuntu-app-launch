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

class FailureTest : public ::testing::Test
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
			g_clear_object(&testbus);
			g_main_loop_unref(mainloop);
			mainloop = NULL;
			return;
		}
};

TEST_F(FailureTest, DummyTest)
{

	return;
}
