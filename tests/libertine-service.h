/*
 * Copyright © 2017 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Ted Gould <ted@canonical.com>
 */

#include <gio/gio.h>

#include <libdbustest/dbus-test.h>

class LibertineService
{
private:
    DbusTestProcess* process = nullptr;
    DbusTestTask* wait = nullptr;

public:
    LibertineService()
    {
        process = dbus_test_process_new("/usr/bin/libertined");
        dbus_test_process_append_param(process, "--debug");

        dbus_test_task_set_bus(DBUS_TEST_TASK(process), DBUS_TEST_SERVICE_BUS_SESSION);
        dbus_test_task_set_name(DBUS_TEST_TASK(process), "libertine");
        dbus_test_task_set_return(DBUS_TEST_TASK(process), DBUS_TEST_TASK_RETURN_IGNORE);
        dbus_test_task_set_wait_finished(DBUS_TEST_TASK(process), FALSE);

        wait = dbus_test_task_new();
        dbus_test_task_set_wait_for(wait, "com.canonical.libertine.Service");
    }

    ~LibertineService()
    {
        g_debug("Destroying the Libertined Task");
        g_clear_object(&process);
        g_clear_object(&wait);
    }

    DbusTestTask* waitTask()
    {
        return wait;
    }

    operator std::shared_ptr<DbusTestTask>()
    {
        std::shared_ptr<DbusTestTask> retval(DBUS_TEST_TASK(g_object_ref(process)),
                                             [](DbusTestTask* process) { g_clear_object(&process); });
        return retval;
    }

    operator DbusTestTask*()
    {
        return DBUS_TEST_TASK(process);
    }
};
