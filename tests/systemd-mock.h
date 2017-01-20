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
        pid_t primaryPid;
        std::vector<pid_t> pids;
    };

private:
    DbusTestDbusMock* mock = nullptr;
    DbusTestDbusMockObject* managerobj = nullptr;
    GLib::ContextThread thread;
    std::list<std::pair<Instance, DbusTestDbusMockObject*>> insts;

public:
    SystemdMock(const std::list<Instance>& instances, const std::string& controlGroupPath)
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
                                          [](const std::string accum, const Instance& inst) {
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
                            [](const std::string accum, const Instance& inst) {
                                std::string retval = accum;

                                retval += "if args[0] == '" + instanceName(inst) + "':\n";
                                retval += "\tret = '" + instancePath(inst) + "'\n";

                                return retval;
                            })
                .c_str(),
            nullptr);
        dbus_test_dbus_mock_object_add_method(
            mock, managerobj, "StopUnit", G_VARIANT_TYPE("(ss)"), G_VARIANT_TYPE_OBJECT_PATH, /* ret type */
            std::accumulate(instances.begin(), instances.end(), std::string{},
                            [](const std::string accum, const Instance& inst) {
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
                                                      "org.freedesktop.systemd1.Service", nullptr);
            dbus_test_dbus_mock_object_add_property(mock, obj, "MainPID", G_VARIANT_TYPE_UINT32,
                                                    g_variant_new_uint32(instance.primaryPid), nullptr);

            /* Control Group */
            auto dir = g_build_filename(controlGroupPath.c_str(), instancePath(instance).c_str(), nullptr);
            auto tasks = g_build_filename(dir, "tasks", nullptr);

            g_mkdir_with_parents(dir, 0777);

            g_file_set_contents(tasks, std::accumulate(instance.pids.begin(), instance.pids.end(), std::string{},
                                                       [](const std::string& accum, pid_t pid) {
                                                           if (accum.empty())
                                                           {
                                                               return std::to_string(pid);
                                                           }
                                                           else
                                                           {
                                                               return accum + "\n" + std::to_string(pid);
                                                           }
                                                       })
                                           .c_str(),
                                -1, nullptr);

            g_free(tasks);
            g_free(dir);

            dbus_test_dbus_mock_object_add_property(mock, obj, "ControlGroup", G_VARIANT_TYPE_STRING,
                                                    g_variant_new_string(instancePath(instance).c_str()), nullptr);

            insts.emplace_back(std::make_pair(instance, obj));
        }
    }

    ~SystemdMock()
    {
        g_debug("Destroying the Systemd Mock");
        g_clear_object(&mock);
    }

    static std::string dbusSafe(const std::string& input)
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

    static std::string instancePath(const Instance& inst)
    {
        std::string retval = std::string{"/"} + dbusSafe(inst.job) + "/" + dbusSafe(inst.appid);

        if (!inst.instanceid.empty())
        {
            retval += "/" + dbusSafe(inst.instanceid);
        }

        return retval;
    }

    static std::string instanceName(const Instance& inst)
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

    std::list<std::string> stopCalls()
    {
        guint len = 0;
        GError* error = nullptr;

        auto calls = dbus_test_dbus_mock_object_get_method_calls(mock,       /* mock */
                                                                 managerobj, /* manager */
                                                                 "StopUnit", /* function */
                                                                 &len,       /* number */
                                                                 &error      /* error */
                                                                 );

        if (error != nullptr)
        {
            g_warning("Unable to get 'StopUnit' calls from systemd mock: %s", error->message);
            g_error_free(error);
            throw std::runtime_error{"Mock disfunctional"};
        }

        std::list<std::string> retval;

        for (unsigned int i = 0; i < len; i++)
        {
            auto& call = calls[i];
            gchar* name = nullptr;
            gchar* inst = nullptr;

            g_variant_get(call.params, "(&s&s)", &name, &inst);

            if (name == nullptr)
            {
                g_warning("Invalid 'name' on 'StopUnit' call");
                continue;
            }

            retval.emplace_back(name);
        }

        return retval;
    }

    void managerClear()
    {
        GError* error = nullptr;

        dbus_test_dbus_mock_object_clear_method_calls(mock,       /* mock */
                                                      managerobj, /* manager */
                                                      &error      /* error */
                                                      );

        if (error != nullptr)
        {
            g_warning("Unable to clear manager calls: %s", error->message);
            g_error_free(error);
            throw std::runtime_error{"Mock disfunctional"};
        }
    }
};
