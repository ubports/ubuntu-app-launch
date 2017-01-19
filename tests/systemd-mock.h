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

#include <algorithm>
#include <future>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <type_traits>

#include <gio/gio.h>

#include <libdbustest/dbus-test.h>

#include "glib-thread.h"

class SystemdMock
{
public:
    struct Instance
    {
        std::string job;
        std::string appid;
        std::string instanceid;
    };

private:
    DbusTestDbusMock* mock = nullptr;
    DbusTestDbusMockObject* managerobj = nullptr;
    GLib::ContextThread thread;
    std::list<std::pair<Instance, DbusTestDbusMockObject*>> insts;

public:
    SystemdMock(std::list<Instance> instances)
    {
        mock = dbus_test_dbus_mock_new("org.freedesktop.systemd1");
        dbus_test_task_set_bus(DBUS_TEST_TASK(mock), DBUS_TEST_SERVICE_BUS_SESSION);
        dbus_test_task_set_name(DBUS_TEST_TASK(mock), "systemd");

        managerobj = dbus_test_dbus_mock_get_object(mock, "/org/freedesktop/systemd1",
                                                    "org.freedesktop.systemd1.Manager", nullptr);

        dbus_test_dbus_mock_object_add_method(mock, managerobj, "Subscribe", nullptr, nullptr, "", nullptr);
        dbus_test_dbus_mock_object_add_method(
            mock, managerobj, "ListUnits", nullptr, G_VARIANT_TYPE("(a(ssssssouso))"), /* ret type */
            ("ret = [ " + std::accumulate(instances.begin(), instances.end(), std::string{},
                                          [](const std::string accum, Instance& inst) {
                                              std::string retval = accum;

                                              if (!retval.empty())
                                              {
                                                  retval += ", ";
                                              }

                                              retval += std::string{"("} +                 /* start tuple */
                                                        "'" + instanceName(inst) + "', " + /* id */
                                                        "'unused', " +                     /* description */
                                                        "'unused', " +                     /* load state */
                                                        "'unused', " +                     /* active state */
                                                        "'unused', " +                     /* substate */
                                                        "'unused', " +                     /* following */
                                                        "'/unused', " +                    /* path */
                                                        "5, " +                            /* jobId */
                                                        "'unused', " +                     /* jobType */
                                                        "'" + instancePath(inst) + "'" +   /* jobPath */
                                                        ")";                               /* finish tuple */

                                              return retval;
                                          }) +
             "]")
                .c_str(),
            nullptr);

        dbus_test_dbus_mock_object_add_method(
            mock, managerobj, "GetUnit", G_VARIANT_TYPE_STRING, G_VARIANT_TYPE_OBJECT_PATH, /* ret type */
            std::accumulate(instances.begin(), instances.end(), std::string{},
                            [](const std::string accum, Instance& inst) {
                                std::string retval = accum;

                                retval += "if args[0] == '" + instanceName(inst) + "':\n";
                                retval += "\tret = '" + instancePath(inst) + "'\n";

                                return retval;
                            })
                .c_str(),
            nullptr);

        for (auto& instance : instances)
        {
            auto obj = dbus_test_dbus_mock_get_object(mock, instancePath(instance).c_str(),
                                                      "org.freedesktop.systemd1.Unit", nullptr);
            insts.emplace_back(std::make_pair(instance, obj));
        }
    }

    ~SystemdMock()
    {
        g_debug("Destroying the Systemd Mock");
        g_clear_object(&mock);
    }

    static std::string dbusSafe(std::string& input)
    {
        std::string output = input;
        std::transform(output.begin(), output.end(), output.begin(), [](char in) {
            if (std::isalpha(in) || std::isdigit(in))
            {
                return in;
            }
            else
            {
                return '_';
            }

        });
        return output;
    }

    static std::string instancePath(Instance& inst)
    {
        return std::string{"/"} + dbusSafe(inst.job) + "/" + dbusSafe(inst.appid) + "/" + dbusSafe(inst.instanceid);
    }

    static std::string instanceName(Instance& inst)
    {
        return std::string{"ubuntu-app-launch-"} + inst.job + "-" + inst.appid + "-" + inst.instanceid + ".service";
    }

    operator std::shared_ptr<DbusTestTask>()
    {
        std::shared_ptr<DbusTestTask> retval(DBUS_TEST_TASK(g_object_ref(mock)),
                                             [](DbusTestTask* task) { g_clear_object(&task); });
        return retval;
    }

    operator DbusTestTask*()
    {
        return DBUS_TEST_TASK(mock);
    }

    operator DbusTestDbusMock*()
    {
        return mock;
    }

    unsigned int subscribeCallsCnt()
    {
        guint len = 0;
        GError* error = nullptr;

        dbus_test_dbus_mock_object_get_method_calls(mock,        /* mock */
                                                    managerobj,  /* manager */
                                                    "Subscribe", /* function */
                                                    &len,        /* number */
                                                    &error       /* error */
                                                    );

        if (error != nullptr)
        {
            g_warning("Unable to get 'Subscribe' calls from systemd mock: %s", error->message);
            g_error_free(error);
            throw std::runtime_error{"Mock disfunctional"};
        }

        return len;
    }

    unsigned int listCallsCnt()
    {
        guint len = 0;
        GError* error = nullptr;

        dbus_test_dbus_mock_object_get_method_calls(mock,        /* mock */
                                                    managerobj,  /* manager */
                                                    "ListUnits", /* function */
                                                    &len,        /* number */
                                                    &error       /* error */
                                                    );

        if (error != nullptr)
        {
            g_warning("Unable to get 'Subscribe' calls from systemd mock: %s", error->message);
            g_error_free(error);
            throw std::runtime_error{"Mock disfunctional"};
        }

        return len;
    }
};
