/*
 * Copyright 2014 Canonical Ltd.
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

class CGroupReap : public ::testing::Test
{
	protected:
		DbusTestService * service = NULL;
		DbusTestDbusMock * cgmock = NULL;
		GDBusConnection * bus = NULL;

		virtual void SetUp() {
			service = dbus_test_service_new(NULL);

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
			dbus_test_service_add_task(service, DBUS_TEST_TASK(cgmock));
			dbus_test_service_start_tasks(service);

			bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
			g_dbus_connection_set_exit_on_close(bus, FALSE);
			g_object_add_weak_pointer(G_OBJECT(bus), (gpointer *)&bus);

			/* Make sure we pretend the CG manager is just on our bus */
			g_setenv("UBUNTU_APP_LAUNCH_CG_MANAGER_SESSION_BUS", "YES", TRUE);
		}

		virtual void TearDown() {
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

TEST_F(CGroupReap, Test)
{

}
