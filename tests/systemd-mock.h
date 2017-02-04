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

    void throwError(GError* error)
    {
        if (error == nullptr)
        {
            return;
        }

        auto message = std::string{"Error in systemd mock: "} + error->message;
        g_error_free(error);
        throw std::runtime_error{message};
    }

public:
    SystemdMock(const std::list<Instance>& instances, const std::string& controlGroupPath)
    {
        GError* error = nullptr;
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
            &error);
        throwError(error);

        dbus_test_dbus_mock_object_add_method(
            mock, managerobj, "GetUnit", G_VARIANT_TYPE_STRING, G_VARIANT_TYPE_OBJECT_PATH, /* ret type */
            ("ret = '/'\n" + std::accumulate(instances.begin(), instances.end(), std::string{},
                                             [](const std::string accum, const Instance& inst) {
                                                 std::string retval = accum;

                                                 retval += "if args[0] == '" + instanceName(inst) + "':\n";
                                                 retval += "\tret = '" + instancePath(inst) + "'\n";

                                                 return retval;
                                             }))
                .c_str(),
            &error);
        throwError(error);

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
            &error);
        throwError(error);

        dbus_test_dbus_mock_object_add_method(mock, managerobj, "StartTransientUnit",
                                              G_VARIANT_TYPE("(ssa(sv)a(sa(sv)))"),
                                              G_VARIANT_TYPE_OBJECT_PATH, /* ret type */
                                              "ret = '/'", &error);
        throwError(error);

        for (auto& instance : instances)
        {
            auto obj = dbus_test_dbus_mock_get_object(mock, instancePath(instance).c_str(),
                                                      "org.freedesktop.systemd1.Service", &error);
            throwError(error);
            dbus_test_dbus_mock_object_add_property(mock, obj, "MainPID", G_VARIANT_TYPE_UINT32,
                                                    g_variant_new_uint32(instance.primaryPid), &error);
            throwError(error);
            dbus_test_dbus_mock_object_add_property(mock, obj, "Result", G_VARIANT_TYPE_STRING,
                                                    g_variant_new_string("success"), &error);
            throwError(error);

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
                                -1, &error);
            throwError(error);

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

    struct TransientUnit
    {
        std::string name;
        std::set<std::string> environment;
        std::string execpath;
        std::list<std::string> execline;
    };

    std::list<TransientUnit> unitCalls()
    {
        guint len = 0;
        GError* error = nullptr;

        auto calls = dbus_test_dbus_mock_object_get_method_calls(mock,                 /* mock */
                                                                 managerobj,           /* manager */
                                                                 "StartTransientUnit", /* function */
                                                                 &len,                 /* number */
                                                                 &error                /* error */
                                                                 );

        if (error != nullptr)
        {
            g_warning("Unable to get 'StartTransientUnit' calls from systemd mock: %s", error->message);
            g_error_free(error);
            throw std::runtime_error{"Mock disfunctional"};
        }

        std::list<TransientUnit> retval;

        for (unsigned int i = 0; i < len; i++)
        {
            auto& call = calls[i];
            gchar* name = nullptr;

            g_variant_get_child(call.params, 0, "&s", &name);

            if (name == nullptr)
            {
                g_warning("Invalid 'name' on 'StartTransientUnit' call");
                continue;
            }

            TransientUnit unit;
            unit.name = name;

            auto paramarray = g_variant_get_child_value(call.params, 2);
            gchar* ckey;
            GVariant* var;
            GVariantIter iter;
            g_variant_iter_init(&iter, paramarray);
            while (g_variant_iter_loop(&iter, "(sv)", &ckey, &var))
            {
                g_debug("Looking at parameter: %s", ckey);
                std::string key{ckey};

                if (key == "Environment")
                {
                    GVariantIter array;
                    gchar* envvar;
                    g_variant_iter_init(&array, var);

                    while (g_variant_iter_loop(&array, "&s", &envvar))
                    {
                        unit.environment.emplace(envvar);
                    }
                }
                else if (key == "ExecStart")
                {
                    /* a(sasb) */
                    if (g_variant_n_children(var) > 1)
                    {
                        g_warning("'ExecStart' has more than one entry, only processing the first");
                    }

                    auto tuple = g_variant_get_child_value(var, 0);

                    const gchar* cpath = nullptr;
                    g_variant_get_child(tuple, 0, "&s", &cpath);

                    if (cpath != nullptr)
                    {
                        unit.execpath = cpath;
                    }
                    else
                    {
                        g_warning("'ExecStart[0][0]' isn't a string?");
                    }

                    auto vexecarray = g_variant_get_child_value(tuple, 1);
                    GVariantIter execarray;
                    g_variant_iter_init(&execarray, vexecarray);
                    const gchar* execentry;

                    while (g_variant_iter_loop(&execarray, "&s", &execentry))
                    {
                        unit.execline.emplace_back(execentry);
                    }

                    g_clear_pointer(&vexecarray, g_variant_unref);
                    g_clear_pointer(&tuple, g_variant_unref);
                }
            }
            g_variant_unref(paramarray);

            retval.emplace_back(unit);
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

    void managerEmitNew(const std::string& name, const std::string& path)
    {
        GError* error = nullptr;

        dbus_test_dbus_mock_object_emit_signal(mock, managerobj, "UnitNew", G_VARIANT_TYPE("(so)"),
                                               g_variant_new("(so)", name.c_str(), path.c_str()), &error);

        if (error != nullptr)
        {
            g_warning("Unable to emit 'UnitNew': %s", error->message);
            g_error_free(error);
            throw std::runtime_error{"Mock disfunctional"};
        }
    }

    void managerEmitRemoved(const std::string& name, const std::string& path)
    {
        GError* error = nullptr;

        dbus_test_dbus_mock_object_emit_signal(mock, managerobj, "UnitRemoved", G_VARIANT_TYPE("(so)"),
                                               g_variant_new("(so)", name.c_str(), path.c_str()), &error);

        if (error != nullptr)
        {
            g_warning("Unable to emit 'UnitRemoved': %s", error->message);
            g_error_free(error);
            throw std::runtime_error{"Mock disfunctional"};
        }
    }

    void managerEmitFailed(const Instance& inst)
    {
        auto instobj =
            std::find_if(insts.begin(), insts.end(), [inst](const std::pair<Instance, DbusTestDbusMockObject*>& item) {
                return item.first.job == inst.job && item.first.appid == inst.appid &&
                       item.first.instanceid == inst.instanceid;
            });

        if (instobj == insts.end())
        {
            throw std::runtime_error{"Unable to find instance"};
        }

        GError* error = nullptr;
        dbus_test_dbus_mock_object_update_property(mock, instobj->second, "Result", g_variant_new_string("fail"),
                                                   &error);

        if (error != nullptr)
        {
            g_warning("Unable to set result to 'fail': %s", error->message);
            g_error_free(error);
            throw std::runtime_error{"Mock disfunctional"};
        }
    }
};
