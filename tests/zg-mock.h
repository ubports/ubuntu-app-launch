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

class ZeitgeistMock
{
private:
    DbusTestDbusMock* zgmock = nullptr;
    DbusTestDbusMockObject* zgobj = nullptr;

public:
    ZeitgeistMock()
    {
        DbusTestDbusMock* zgmock = dbus_test_dbus_mock_new("org.gnome.zeitgeist.Engine");
        dbus_test_task_set_name(DBUS_TEST_TASK(zgmock), "Zeitgeist");

        DbusTestDbusMockObject* zgobj = dbus_test_dbus_mock_get_object(zgmock, "/org/gnome/zeitgeist/log/activity",
                                                                       "org.gnome.zeitgeist.Log", NULL);

        dbus_test_dbus_mock_object_add_method(zgmock, zgobj, "InsertEvents", G_VARIANT_TYPE("a(asaasay)"),
                                              G_VARIANT_TYPE("au"), "ret = [ 0 ]", NULL);
    }

    ~ZeitgeistMock()
    {
        g_debug("Destroying the Zeitgeist Mock");
        g_clear_object(&zgmock);
    }

    operator std::shared_ptr<DbusTestTask>()
    {
        std::shared_ptr<DbusTestTask> retval(DBUS_TEST_TASK(g_object_ref(zgmock)),
                                             [](DbusTestTask* task) { g_clear_object(&task); });
        return retval;
    }

    operator DbusTestTask*()
    {
        return DBUS_TEST_TASK(zgmock);
    }

    operator DbusTestDbusMock*()
    {
        return zgmock;
    }

    std::function<DbusTestTaskState()> stateFunc()
    {
        return [this] { return dbus_test_task_get_state(DBUS_TEST_TASK(zgmock)); };
    }

    std::list<bool> insertCalls()
    {
        guint len = 0;
        GError* error = nullptr;

        dbus_test_dbus_mock_object_get_method_calls(zgmock,         /* mock */
                                                    zgobj,          /* manager */
                                                    "InsertEvents", /* function */
                                                    &len,           /* number */
                                                    &error          /* error */
                                                    );

        if (error != nullptr)
        {
            g_warning("Unable to get 'InsertEvents' calls from zeitgeist mock: %s", error->message);
            g_error_free(error);
            throw std::runtime_error{"Mock disfunctional"};
        }

        std::list<bool> retval;

        for (unsigned int i = 0; i < len; i++)
        {
            /* We're not using the params for anything */
            retval.emplace_back(true);
        }

        return retval;
    }

    void clear()
    {
        GError* error = nullptr;

        dbus_test_dbus_mock_object_clear_method_calls(zgmock, /* mock */
                                                      zgobj,  /* manager */
                                                      &error  /* error */
                                                      );

        if (error != nullptr)
        {
            g_warning("Unable to clear ZG calls: %s", error->message);
            g_error_free(error);
            throw std::runtime_error{"Mock disfunctional"};
        }
    }
};
