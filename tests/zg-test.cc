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

#include <gio/gio.h>
#include <gtest/gtest.h>
#include <libdbustest/dbus-test.h>

#include "eventually-fixture.h"

class ZGEvent : public EventuallyFixture
{
    GDBusConnection* bus = NULL;

protected:
    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
        g_object_unref(bus);

        EXPECT_EVENTUALLY_EQ(nullptr, bus);
    }

    void grabBus()
    {
        bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
        g_dbus_connection_set_exit_on_close(bus, FALSE);
        g_object_add_weak_pointer(G_OBJECT(bus), (gpointer*)&bus);
    }
};

static void zg_state_changed(DbusTestTask* task, DbusTestTaskState state, gpointer user_data)
{
    auto outstate = reinterpret_cast<DbusTestTaskState*>(user_data);
    *outstate = state;
}

TEST_F(ZGEvent, OpenTest)
{
    DbusTestService* service = dbus_test_service_new(NULL);

    DbusTestDbusMock* mock = dbus_test_dbus_mock_new("org.gnome.zeitgeist.Engine");
    DbusTestDbusMockObject* obj =
        dbus_test_dbus_mock_get_object(mock, "/org/gnome/zeitgeist/log/activity", "org.gnome.zeitgeist.Log", NULL);

    dbus_test_dbus_mock_object_add_method(mock, obj, "InsertEvents", G_VARIANT_TYPE("a(asaasay)"), G_VARIANT_TYPE("au"),
                                          "ret = [ 0 ]", NULL);

    dbus_test_service_add_task(service, DBUS_TEST_TASK(mock));

    DbusTestProcess* zgevent = dbus_test_process_new(ZG_EVENT_TOOL);
    dbus_test_process_append_param(zgevent, "open");
    g_setenv("APP_ID", "foo", TRUE);
    dbus_test_task_set_wait_for(DBUS_TEST_TASK(zgevent), "org.gnome.zeitgeist.Engine");
    dbus_test_task_set_name(DBUS_TEST_TASK(zgevent), "ZGEvent");
    DbusTestTaskState zgevent_state = DBUS_TEST_TASK_STATE_INIT;
    g_signal_connect(G_OBJECT(zgevent), DBUS_TEST_TASK_SIGNAL_STATE_CHANGED, G_CALLBACK(zg_state_changed),
                     &zgevent_state);

    dbus_test_service_add_task(service, DBUS_TEST_TASK(zgevent));

    dbus_test_service_start_tasks(service);
    grabBus();

    EXPECT_EVENTUALLY_EQ(DBUS_TEST_TASK_STATE_FINISHED, zgevent_state);
    ASSERT_TRUE(dbus_test_task_passed(DBUS_TEST_TASK(zgevent)));

    guint numcalls = 0;
    const DbusTestDbusMockCall* calls =
        dbus_test_dbus_mock_object_get_method_calls(mock, obj, "InsertEvents", &numcalls, NULL);

    ASSERT_NE(nullptr, calls);
    ASSERT_EQ(1, numcalls);

    g_object_unref(zgevent);
    g_object_unref(mock);
    g_object_unref(service);
}

TEST_F(ZGEvent, TimeoutTest)
{
    DbusTestService* service = dbus_test_service_new(NULL);

    DbusTestDbusMock* mock = dbus_test_dbus_mock_new("org.gnome.zeitgeist.Engine");
    DbusTestDbusMockObject* obj =
        dbus_test_dbus_mock_get_object(mock, "/org/gnome/zeitgeist/log/activity", "org.gnome.zeitgeist.Log", NULL);

    dbus_test_dbus_mock_object_add_method(mock, obj, "InsertEvents", G_VARIANT_TYPE("a(asaasay)"), G_VARIANT_TYPE("au"),
                                          "time.sleep(6)\n"
                                          "ret = [ 0 ]",
                                          NULL);

    dbus_test_service_add_task(service, DBUS_TEST_TASK(mock));

    DbusTestProcess* zgevent = dbus_test_process_new(ZG_EVENT_TOOL);
    dbus_test_process_append_param(zgevent, "close");
    g_setenv("APP_ID", "foo", TRUE);
    dbus_test_task_set_wait_for(DBUS_TEST_TASK(zgevent), "org.gnome.zeitgeist.Engine");
    dbus_test_task_set_name(DBUS_TEST_TASK(zgevent), "ZGEvent");
    DbusTestTaskState zgevent_state = DBUS_TEST_TASK_STATE_INIT;
    g_signal_connect(G_OBJECT(zgevent), DBUS_TEST_TASK_SIGNAL_STATE_CHANGED, G_CALLBACK(zg_state_changed),
                     &zgevent_state);

    dbus_test_service_add_task(service, DBUS_TEST_TASK(zgevent));

    dbus_test_service_start_tasks(service);
    grabBus();

    EXPECT_EVENTUALLY_EQ(DBUS_TEST_TASK_STATE_FINISHED, zgevent_state);

    g_object_unref(zgevent);
    g_object_unref(mock);
    g_object_unref(service);
}
