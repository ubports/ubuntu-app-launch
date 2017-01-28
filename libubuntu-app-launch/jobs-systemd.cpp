/*
 * Copyright © 2016 Canonical Ltd.
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

#include "jobs-systemd.h"
#include "application-impl-base.h"
#include "helpers.h"
#include "registry-impl.h"
#include "second-exec-core.h"

extern "C" {
#include "ubuntu-app-launch-trace.h"
}

#include <gio/gio.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <numeric>
#include <regex>

namespace ubuntu
{
namespace app_launch
{
namespace jobs
{
namespace instance
{

class SystemD : public instance::Base
{
    friend class manager::SystemD;

public:
    explicit SystemD(const AppID& appId,
                     const std::string& job,
                     const std::string& instance,
                     const std::vector<Application::URL>& urls,
                     const std::shared_ptr<Registry>& registry);
    virtual ~SystemD()
    {
        g_debug("Destroying a SystemD for '%s' instance '%s'", std::string(appId_).c_str(), instance_.c_str());
    }

    /* Query lifecycle */
    pid_t primaryPid() override;
    std::string logPath() override;
    std::vector<pid_t> pids() override;

    /* Manage lifecycle */
    void stop() override;

};  // class SystemD

SystemD::SystemD(const AppID& appId,
                 const std::string& job,
                 const std::string& instance,
                 const std::vector<Application::URL>& urls,
                 const std::shared_ptr<Registry>& registry)
    : Base(appId, job, instance, urls, registry)
{
    g_debug("Creating a new SystemD for '%s' instance '%s'", std::string(appId).c_str(), instance.c_str());
}

pid_t SystemD::primaryPid()
{
    auto manager = std::dynamic_pointer_cast<manager::SystemD>(registry_->impl->jobs);
    return manager->unitPrimaryPid(appId_, job_, instance_);
}

std::string SystemD::logPath()
{
    /* NOTE: We can never get this for systemd */
    g_warning("Log paths aren't available for systemd");
    return {};
}

std::vector<pid_t> SystemD::pids()
{
    auto manager = std::dynamic_pointer_cast<manager::SystemD>(registry_->impl->jobs);
    return manager->unitPids(appId_, job_, instance_);
}

void SystemD::stop()
{
    auto manager = std::dynamic_pointer_cast<manager::SystemD>(registry_->impl->jobs);
    manager->stopUnit(appId_, job_, instance_);
}

}  // namespace instance

namespace manager
{

static const char* SYSTEMD_DBUS_ADDRESS{"org.freedesktop.systemd1"};
static const char* SYSTEMD_DBUS_IFACE_MANAGER{"org.freedesktop.systemd1.Manager"};
static const char* SYSTEMD_DBUS_PATH_MANAGER{"/org/freedesktop/systemd1"};
// static const char * SYSTEMD_DBUS_IFACE_UNIT{"org.freedesktop.systemd1.Unit"};
static const char* SYSTEMD_DBUS_IFACE_SERVICE{"org.freedesktop.systemd1.Service"};

SystemD::SystemD(std::shared_ptr<Registry> registry)
    : Base(registry)
{
    auto gcgroup_root = getenv("UBUNTU_APP_LAUNCH_SYSTEMD_CGROUP_ROOT");
    if (gcgroup_root == nullptr)
    {
        auto cpath = g_build_filename("/sys", "fs", "cgroup", "systemd", nullptr);
        cgroup_root_ = cpath;
        g_free(cpath);
    }
    else
    {
        cgroup_root_ = gcgroup_root;
    }

    auto cancel = registry->impl->thread.getCancellable();
    userbus_ = registry->impl->thread.executeOnThread<std::shared_ptr<GDBusConnection>>([this, cancel]() {
        GError* error = nullptr;
        auto bus = std::shared_ptr<GDBusConnection>(
            [&]() -> GDBusConnection* {
                if (g_file_test(SystemD::userBusPath().c_str(), G_FILE_TEST_EXISTS))
                {
                    return g_dbus_connection_new_for_address_sync(
                        ("unix:path=" + userBusPath()).c_str(), /* path to the user bus */
                        (GDBusConnectionFlags)(
                            G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                            G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION), /* It is a message bus */
                        nullptr,                                             /* observer */
                        cancel.get(),                                        /* cancellable from the thread */
                        &error);                                             /* error */
                }
                else
                {
                    /* Fallback mostly for testing */
                    g_debug("Using session bus for systemd user bus");
                    return g_bus_get_sync(G_BUS_TYPE_SESSION, /* type */
                                          cancel.get(),       /* thread cancellable */
                                          &error);            /* error */
                }
            }(),
            [](GDBusConnection* bus) { g_clear_object(&bus); });

        if (error != nullptr)
        {
            std::string message = std::string("Unable to connect to user bus: ") + error->message;
            g_error_free(error);
            throw std::runtime_error(message);
        }

        /* If we don't subscribe, it doesn't send us signals :-( */
        g_dbus_connection_call(bus.get(),                  /* user bus */
                               SYSTEMD_DBUS_ADDRESS,       /* bus name */
                               SYSTEMD_DBUS_PATH_MANAGER,  /* path */
                               SYSTEMD_DBUS_IFACE_MANAGER, /* interface */
                               "Subscribe",                /* method */
                               nullptr,                    /* params */
                               nullptr,                    /* ret type */
                               G_DBUS_CALL_FLAGS_NONE,     /* flags */
                               -1,                         /* timeout */
                               cancel.get(),               /* cancellable */
                               [](GObject* obj, GAsyncResult* res, gpointer user_data) {
                                   GError* error{nullptr};
                                   GVariant* callt = g_dbus_connection_call_finish(G_DBUS_CONNECTION(obj), res, &error);

                                   if (error != nullptr)
                                   {
                                       if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                                       {
                                           g_warning("Unable to subscribe to SystemD: %s", error->message);
                                       }
                                       g_error_free(error);
                                       return;
                                   }

                                   g_clear_pointer(&callt, g_variant_unref);
                                   g_debug("Subscribed to Systemd");
                               },
                               nullptr);

        /* Setup Unit add/remove signals */
        handle_unitNew = g_dbus_connection_signal_subscribe(
            bus.get(),                  /* bus */
            nullptr,                    /* sender */
            SYSTEMD_DBUS_IFACE_MANAGER, /* interface */
            "UnitNew",                  /* signal */
            SYSTEMD_DBUS_PATH_MANAGER,  /* path */
            nullptr,                    /* arg0 */
            G_DBUS_SIGNAL_FLAGS_NONE,
            [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar*, GVariant* params,
               gpointer user_data) -> void {
                auto pthis = static_cast<SystemD*>(user_data);

                if (!g_variant_check_format_string(params, "(so)", FALSE))
                {
                    g_warning("Got 'UnitNew' signal with unknown parameter type: %s",
                              g_variant_get_type_string(params));
                    return;
                }

                const gchar* unitname{nullptr};
                const gchar* unitpath{nullptr};

                g_variant_get(params, "(&s&o)", &unitname, &unitpath);

                if (unitname == nullptr || unitpath == nullptr)
                {
                    g_warning("Got 'UnitNew' signal with funky params %p, %p", unitname, unitpath);
                    return;
                }

                try
                {
                    pthis->parseUnit(unitname);
                }
                catch (std::runtime_error& e)
                {
                    /* Not for UAL */
                    g_debug("Unable to parse unit: %s", unitname);
                    return;
                }

                try
                {
                    auto info = pthis->unitNew(unitname, unitpath, pthis->userbus_);
                    pthis->emitSignal(pthis->sig_appStarted, info);
                }
                catch (std::runtime_error& e)
                {
                    g_warning("%s", e.what());
                }
            },        /* callback */
            this,     /* user data */
            nullptr); /* user data destroy */

        handle_unitRemoved = g_dbus_connection_signal_subscribe(
            bus.get(),                  /* bus */
            nullptr,                    /* sender */
            SYSTEMD_DBUS_IFACE_MANAGER, /* interface */
            "UnitRemoved",              /* signal */
            SYSTEMD_DBUS_PATH_MANAGER,  /* path */
            nullptr,                    /* arg0 */
            G_DBUS_SIGNAL_FLAGS_NONE,
            [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar*, GVariant* params,
               gpointer user_data) -> void {
                auto pthis = static_cast<SystemD*>(user_data);

                if (!g_variant_check_format_string(params, "(so)", FALSE))
                {
                    g_warning("Got 'UnitRemoved' signal with unknown parameter type: %s",
                              g_variant_get_type_string(params));
                    return;
                }

                const gchar* unitname{nullptr};
                const gchar* unitpath{nullptr};

                g_variant_get(params, "(&s&o)", &unitname, &unitpath);

                if (unitname == nullptr || unitpath == nullptr)
                {
                    g_warning("Got 'UnitRemoved' signal with funky params %p, %p", unitname, unitpath);
                    return;
                }

                try
                {
                    pthis->parseUnit(unitname);
                }
                catch (std::runtime_error& e)
                {
                    /* Not for UAL */
                    g_debug("Unable to parse unit: %s", unitname);
                    return;
                }

                pthis->unitRemoved(unitname, unitpath);
            },        /* callback */
            this,     /* user data */
            nullptr); /* user data destroy */

        getInitialUnits(bus, cancel);

        return bus;
    });
}

SystemD::~SystemD()
{
    auto unsub = [&](guint& handle) {
        if (handle != 0)
        {
            g_dbus_connection_signal_unsubscribe(dbus_.get(), handle);
            handle = 0;
        }
    };

    unsub(handle_unitNew);
    unsub(handle_unitRemoved);
    unsub(handle_appFailed);
}

void SystemD::getInitialUnits(const std::shared_ptr<GDBusConnection>& bus, const std::shared_ptr<GCancellable>& cancel)
{
    GError* error = nullptr;

    auto callt = g_dbus_connection_call_sync(bus.get(),                         /* user bus */
                                             SYSTEMD_DBUS_ADDRESS,              /* bus name */
                                             SYSTEMD_DBUS_PATH_MANAGER,         /* path */
                                             SYSTEMD_DBUS_IFACE_MANAGER,        /* interface */
                                             "ListUnits",                       /* method */
                                             nullptr,                           /* params */
                                             G_VARIANT_TYPE("(a(ssssssouso))"), /* ret type */
                                             G_DBUS_CALL_FLAGS_NONE,            /* flags */
                                             -1,                                /* timeout */
                                             cancel.get(),                      /* cancellable */
                                             &error);

    if (error != nullptr)
    {
        if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
            g_warning("Unable to list SystemD units: %s", error->message);
        }
        g_error_free(error);
        return;
    }

    GVariant* call = g_variant_get_child_value(callt, 0);
    g_variant_unref(callt);

    const gchar* id;
    const gchar* description;
    const gchar* loadState;
    const gchar* activeState;
    const gchar* subState;
    const gchar* following;
    const gchar* path;
    guint32 jobId;
    const gchar* jobType;
    const gchar* jobPath;
    auto iter = g_variant_iter_new(call);
    while (g_variant_iter_loop(iter, "(&s&s&s&s&s&s&ou&s&o)", &id, &description, &loadState, &activeState, &subState,
                               &following, &path, &jobId, &jobType, &jobPath))
    {
        g_debug("List Units: %s", id);
        try
        {
            unitNew(id, jobPath, bus);
        }
        catch (std::runtime_error& e)
        {
            g_debug("%s", e.what());
        }
    }

    g_variant_iter_free(iter);
    g_variant_unref(call);
}

std::string SystemD::findEnv(const std::string& value, std::list<std::pair<std::string, std::string>>& env)
{
    std::string retval;
    auto entry = std::find_if(env.begin(), env.end(),
                              [&value](std::pair<std::string, std::string>& entry) { return entry.first == value; });

    if (entry != env.end())
    {
        retval = entry->second;
    }

    return retval;
}

void SystemD::removeEnv(const std::string& value, std::list<std::pair<std::string, std::string>>& env)
{
    auto entry = std::find_if(env.begin(), env.end(),
                              [&value](std::pair<std::string, std::string>& entry) { return entry.first == value; });

    if (entry != env.end())
    {
        env.erase(entry);
    }
}

int SystemD::envSize(std::list<std::pair<std::string, std::string>>& env)
{
    int len = std::string{"Environment="}.length();

    for (const auto& entry : env)
    {
        len += 3; /* two quotes, one space */
        len += entry.first.length();
        len += entry.second.length();
    }

    len -= 1; /* We account for a space each time but the first doesn't have */

    return len;
}

std::vector<std::string> SystemD::parseExec(std::list<std::pair<std::string, std::string>>& env)
{
    auto exec = findEnv("APP_EXEC", env);
    if (exec.empty())
    {
        g_warning("Application exec line is empty?!?!?");
        return {};
    }
    auto uris = findEnv("APP_URIS", env);

    g_debug("Exec line: %s", exec.c_str());
    g_debug("App URLS:  %s", uris.c_str());

    auto execarray = desktop_exec_parse(exec.c_str(), uris.c_str());

    std::vector<std::string> retval;
    for (unsigned int i = 0; ((gchar**)execarray->data)[i] != nullptr; i++)
    {
        retval.emplace_back(((gchar**)execarray->data)[i]);
    }

    g_array_set_clear_func(execarray, g_free);
    g_array_free(execarray, FALSE);

    if (retval.empty())
    {
        g_warning("After parsing 'APP_EXEC=%s' we ended up with no tokens", exec.c_str());
    }

    /* See if we need the xmir helper */
    if (findEnv("APP_XMIR_ENABLE", env) == "1" && getenv("DISPLAY") == nullptr)
    {
        retval.emplace(retval.begin(), findEnv("APP_ID", env));
        retval.emplace(retval.begin(), XMIR_HELPER);
    }

    /* See if we're doing apparmor by hand */
    auto appexecpolicy = findEnv("APP_EXEC_POLICY", env);
    if (!appexecpolicy.empty() && appexecpolicy != "unconfined")
    {
        retval.emplace(retval.begin(), appexecpolicy);
        retval.emplace(retval.begin(), "-p");
        retval.emplace(retval.begin(), "aa-exec");
    }

    return retval;
}

/** Small helper that we can new/delete to work better with C stuff */
struct StartCHelper
{
    std::shared_ptr<instance::SystemD> ptr;
    std::shared_ptr<GDBusConnection> bus;
};

void SystemD::application_start_cb(GObject* obj, GAsyncResult* res, gpointer user_data)
{
    auto data = static_cast<StartCHelper*>(user_data);

    tracepoint(ubuntu_app_launch, libual_start_message_callback, std::string(data->ptr->appId_).c_str());

    g_debug("Started Message Callback: %s", std::string(data->ptr->appId_).c_str());

    GError* error{nullptr};
    GVariant* result{nullptr};

    result = g_dbus_connection_call_finish(G_DBUS_CONNECTION(obj), res, &error);

    /* We don't care about the result but we need to make sure we don't
       have a leak. */
    g_clear_pointer(&result, g_variant_unref);

    if (error != nullptr)
    {
        if (g_dbus_error_is_remote_error(error))
        {
            gchar* remote_error = g_dbus_error_get_remote_error(error);
            g_debug("Remote error: %s", remote_error);
            if (g_strcmp0(remote_error, "org.freedesktop.systemd1.UnitExists") == 0)
            {
                auto urls = instance::SystemD::urlsToStrv(data->ptr->urls_);
                second_exec(data->bus.get(),                                           /* DBus */
                            data->ptr->registry_->impl->thread.getCancellable().get(), /* cancellable */
                            data->ptr->primaryPid(),                                   /* primary pid */
                            std::string(data->ptr->appId_).c_str(),                    /* appid */
                            data->ptr->instance_.c_str(),                              /* instance */
                            urls.get());                                               /* urls */
            }

            g_free(remote_error);
        }
        else
        {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            {
                g_warning("Unable to emit event to start application: %s", error->message);
            }
        }
        g_error_free(error);
    }

    delete data;
}

void SystemD::copyEnv(const std::string& envname, std::list<std::pair<std::string, std::string>>& env)
{
    if (!findEnv(envname, env).empty())
    {
        g_debug("Already a value set for '%s' ignoring", envname.c_str());
        return;
    }

    auto cvalue = getenv(envname.c_str());
    g_debug("Copying Environment: %s", envname.c_str());
    if (cvalue != nullptr)
    {
        std::string value{cvalue};
        env.emplace_back(std::make_pair(envname, value));
    }
    else
    {
        g_debug("Unable to copy environment '%s'", envname.c_str());
    }
}

void SystemD::copyEnvByPrefix(const std::string& prefix, std::list<std::pair<std::string, std::string>>& env)
{
    for (unsigned int i = 0; environ[i] != nullptr; i++)
    {
        if (g_str_has_prefix(environ[i], prefix.c_str()))
        {
            std::string envname = environ[i];
            envname.erase(envname.find('='));
            copyEnv(envname, env);
        }
    }
}

std::shared_ptr<Application::Instance> SystemD::launch(
    const AppID& appId,
    const std::string& job,
    const std::string& instance,
    const std::vector<Application::URL>& urls,
    launchMode mode,
    std::function<std::list<std::pair<std::string, std::string>>(void)>& getenv)
{
    if (appId.empty())
        return {};

    auto registry = registry_.lock();
    return registry->impl->thread.executeOnThread<std::shared_ptr<instance::SystemD>>(
        [&]() -> std::shared_ptr<instance::SystemD> {
            auto manager = std::dynamic_pointer_cast<manager::SystemD>(registry->impl->jobs);
            std::string appIdStr{appId};
            g_debug("Initializing params for an new instance::SystemD for: %s", appIdStr.c_str());

            tracepoint(ubuntu_app_launch, libual_start, appIdStr.c_str());

            int timeout = 1;
            if (ubuntu::app_launch::Registry::Impl::isWatchingAppStarting())
            {
                timeout = 0;
            }

            auto handshake = starting_handshake_start(appIdStr.c_str(), instance.c_str(), timeout);
            if (handshake == nullptr)
            {
                g_warning("Unable to setup starting handshake");
            }

            /* Figure out the unit name for the job */
            auto unitname = unitName(SystemD::UnitInfo{appIdStr, job, instance});

            /* Build up our environment */
            auto env = getenv();

            env.emplace_back(std::make_pair("APP_ID", appIdStr));                           /* Application ID */
            env.emplace_back(std::make_pair("APP_LAUNCHER_PID", std::to_string(getpid()))); /* Who we are, for bugs */

            copyEnv("DISPLAY", env);
            for (const auto prefix : {"DBUS_", "MIR_", "QT_", "UBUNTU_", "UNITY_", "XDG_"})
            {
                copyEnvByPrefix(prefix, env);
            }

            if (!urls.empty())
            {
                auto accumfunc = [](const std::string& prev, Application::URL thisurl) -> std::string {
                    gchar* gescaped = g_shell_quote(thisurl.value().c_str());
                    std::string escaped;
                    if (gescaped != nullptr)
                    {
                        escaped = gescaped;
                        g_free(gescaped);
                    }
                    else
                    {
                        g_warning("Unable to escape URL: %s", thisurl.value().c_str());
                        return prev;
                    }

                    if (prev.empty())
                    {
                        return escaped;
                    }
                    else
                    {
                        return prev + " " + escaped;
                    }
                };
                auto urlstring = std::accumulate(urls.begin(), urls.end(), std::string{}, accumfunc);
                env.emplace_back(std::make_pair("APP_URIS", urlstring));
            }

            if (mode == launchMode::TEST)
            {
                env.emplace_back(std::make_pair("QT_LOAD_TESTABILITY", "1"));
            }

            /* Convert to GVariant */
            GVariantBuilder builder;
            g_variant_builder_init(&builder, G_VARIANT_TYPE_TUPLE);

            g_variant_builder_add_value(&builder, g_variant_new_string(unitname.c_str()));
            g_variant_builder_add_value(&builder, g_variant_new_string("replace"));  // Job mode

            /* Parameter Array */
            g_variant_builder_open(&builder, G_VARIANT_TYPE_ARRAY);

            /* ExecStart */
            auto commands = parseExec(env);
            if (!commands.empty())
            {
                g_variant_builder_open(&builder, G_VARIANT_TYPE_TUPLE);
                g_variant_builder_add_value(&builder, g_variant_new_string("ExecStart"));
                g_variant_builder_open(&builder, G_VARIANT_TYPE_VARIANT);
                g_variant_builder_open(&builder, G_VARIANT_TYPE_ARRAY);

                g_variant_builder_open(&builder, G_VARIANT_TYPE_TUPLE);

                gchar* pathexec = g_find_program_in_path(commands[0].c_str());
                if (pathexec != nullptr)
                {
                    g_variant_builder_add_value(&builder, g_variant_new_take_string(pathexec));
                }
                else
                {
                    g_debug("Unable to find '%s' in PATH=%s", commands[0].c_str(), g_getenv("PATH"));
                    g_variant_builder_add_value(&builder, g_variant_new_string(commands[0].c_str()));
                }

                g_variant_builder_open(&builder, G_VARIANT_TYPE_ARRAY);
                for (const auto& param : commands)
                {
                    g_variant_builder_add_value(&builder, g_variant_new_string(param.c_str()));
                }
                g_variant_builder_close(&builder);

                g_variant_builder_add_value(&builder, g_variant_new_boolean(FALSE));

                g_variant_builder_close(&builder);
                g_variant_builder_close(&builder);
                g_variant_builder_close(&builder);
                g_variant_builder_close(&builder);
            }

            /* RemainAfterExit */
            g_variant_builder_open(&builder, G_VARIANT_TYPE_TUPLE);
            g_variant_builder_add_value(&builder, g_variant_new_string("RemainAfterExit"));
            g_variant_builder_open(&builder, G_VARIANT_TYPE_VARIANT);
            g_variant_builder_add_value(&builder, g_variant_new_boolean(FALSE));
            g_variant_builder_close(&builder);
            g_variant_builder_close(&builder);

            /* Type */
            g_variant_builder_open(&builder, G_VARIANT_TYPE_TUPLE);
            g_variant_builder_add_value(&builder, g_variant_new_string("Type"));
            g_variant_builder_open(&builder, G_VARIANT_TYPE_VARIANT);
            g_variant_builder_add_value(&builder, g_variant_new_string("oneshot"));
            g_variant_builder_close(&builder);
            g_variant_builder_close(&builder);

            /* Working Directory */
            if (!findEnv("APP_DIR", env).empty())
            {
                g_variant_builder_open(&builder, G_VARIANT_TYPE_TUPLE);
                g_variant_builder_add_value(&builder, g_variant_new_string("WorkingDirectory"));
                g_variant_builder_open(&builder, G_VARIANT_TYPE_VARIANT);
                g_variant_builder_add_value(&builder, g_variant_new_string(findEnv("APP_DIR", env).c_str()));
                g_variant_builder_close(&builder);
                g_variant_builder_close(&builder);
            }

            /* Clean up env before shipping it */
            for (const auto rmenv :
                 {"APP_XMIR_ENABLE", "APP_DIR", "APP_URIS", "APP_EXEC", "APP_EXEC_POLICY", "APP_LAUNCHER_PID",
                  "INSTANCE_ID", "MIR_SERVER_PLATFORM_PATH", "MIR_SERVER_PROMPT_FILE", "MIR_SERVER_HOST_SOCKET",
                  "UBUNTU_APP_LAUNCH_DEMANGLER", "UBUNTU_APP_LAUNCH_OOM_HELPER", "UBUNTU_APP_LAUNCH_LEGACY_ROOT",
                  "UBUNTU_APP_LAUNCH_XMIR_HELPER"})
            {
                removeEnv(rmenv, env);
            }

            g_debug("Environment length: %d", envSize(env));

            /* Environment */
            g_variant_builder_open(&builder, G_VARIANT_TYPE_TUPLE);
            g_variant_builder_add_value(&builder, g_variant_new_string("Environment"));
            g_variant_builder_open(&builder, G_VARIANT_TYPE_VARIANT);
            g_variant_builder_open(&builder, G_VARIANT_TYPE_ARRAY);
            for (const auto& envvar : env)
            {
                if (!envvar.first.empty() && !envvar.second.empty())
                {
                    g_variant_builder_add_value(&builder, g_variant_new_take_string(g_strdup_printf(
                                                              "%s=%s", envvar.first.c_str(), envvar.second.c_str())));
                    // g_debug("Setting environment: %s=%s", envvar.first.c_str(), envvar.second.c_str());
                }
            }

            g_variant_builder_close(&builder);
            g_variant_builder_close(&builder);
            g_variant_builder_close(&builder);

            /* Parameter Array */
            g_variant_builder_close(&builder);

            /* Dependent Units (none) */
            g_variant_builder_add_value(&builder, g_variant_new_array(G_VARIANT_TYPE("(sa(sv))"), nullptr, 0));

            auto retval = std::make_shared<instance::SystemD>(appId, job, instance, urls, registry);
            auto chelper = new StartCHelper{};
            chelper->ptr = retval;
            chelper->bus = manager->userbus_;

            tracepoint(ubuntu_app_launch, handshake_wait, appIdStr.c_str());
            starting_handshake_wait(handshake);
            tracepoint(ubuntu_app_launch, handshake_complete, appIdStr.c_str());

            /* Call the job start function */
            g_debug("Asking systemd to start task for: %s", appIdStr.c_str());
            g_dbus_connection_call(manager->userbus_.get(),                       /* bus */
                                   SYSTEMD_DBUS_ADDRESS,                          /* service name */
                                   SYSTEMD_DBUS_PATH_MANAGER,                     /* Path */
                                   SYSTEMD_DBUS_IFACE_MANAGER,                    /* interface */
                                   "StartTransientUnit",                          /* method */
                                   g_variant_builder_end(&builder),               /* params */
                                   G_VARIANT_TYPE("(o)"),                         /* return */
                                   G_DBUS_CALL_FLAGS_NONE,                        /* flags */
                                   -1,                                            /* default timeout */
                                   registry->impl->thread.getCancellable().get(), /* cancellable */
                                   application_start_cb,                          /* callback */
                                   chelper                                        /* object */
                                   );

            tracepoint(ubuntu_app_launch, libual_start_message_sent, appIdStr.c_str());

            return retval;
        });
}

std::shared_ptr<Application::Instance> SystemD::existing(const AppID& appId,
                                                         const std::string& job,
                                                         const std::string& instance,
                                                         const std::vector<Application::URL>& urls)
{
    return std::make_shared<instance::SystemD>(appId, job, instance, urls, registry_.lock());
}

std::vector<std::shared_ptr<instance::Base>> SystemD::instances(const AppID& appID, const std::string& job)
{
    std::vector<std::shared_ptr<instance::Base>> instances;
    std::vector<Application::URL> urls;
    auto registry = registry_.lock();

    if (!registry)
    {
        g_warning("Unable to list instances without a registry");
        return {};
    }

    std::string sappid{appID};
    for (const auto& unit : unitPaths)
    {
        const SystemD::UnitInfo& unitinfo = unit.first;

        if (job != unitinfo.job)
        {
            continue;
        }

        if (sappid != unitinfo.appid)
        {
            continue;
        }

        instances.emplace_back(std::make_shared<instance::SystemD>(appID, job, unitinfo.inst, urls, registry));
    }

    g_debug("Found %d instances for AppID '%s'", int(instances.size()), std::string(appID).c_str());

    return instances;
}

std::list<std::shared_ptr<Application>> SystemD::runningApps()
{
    auto registry = registry_.lock();

    if (!registry)
    {
        g_warning("Unable to list instances without a registry");
        return {};
    }

    auto allJobs = getAllJobs();
    std::set<std::string> appids;

    for (const auto& unit : unitPaths)
    {
        const SystemD::UnitInfo& unitinfo = unit.first;

        if (allJobs.find(unitinfo.job) == allJobs.end())
        {
            continue;
        }

        appids.insert(unitinfo.appid);
    }

    std::list<std::shared_ptr<Application>> apps;
    for (const auto& appid : appids)
    {
        auto id = AppID::find(registry, appid);
        if (id.empty())
        {
            g_debug("Unable to handle AppID: %s", appid.c_str());
            continue;
        }

        apps.emplace_back(Application::create(id, registry));
    }

    return apps;
}

std::string SystemD::userBusPath()
{
    auto cpath = getenv("UBUNTU_APP_LAUNCH_SYSTEMD_PATH");
    if (cpath != nullptr)
    {
        return cpath;
    }
    return std::string{"/run/user/"} + std::to_string(getuid()) + std::string{"/bus"};
}

/* TODO: Application job names */
const std::regex unitNaming{
    "^ubuntu\\-app\\-launch\\-(application\\-(?:click|legacy|snap))\\-(.*)\\-([0-9]*)\\.service$"};

SystemD::UnitInfo SystemD::parseUnit(const std::string& unit) const
{
    std::smatch match;
    if (!std::regex_match(unit, match, unitNaming))
    {
        throw std::runtime_error{"Unable to parse unit name: " + unit};
    }

    return {match[2].str(), match[1].str(), match[3].str()};
}

std::string SystemD::unitName(const SystemD::UnitInfo& info) const
{
    return std::string{"ubuntu-app-launch-"} + info.job + "-" + info.appid + "-" + info.inst + ".service";
}

std::string SystemD::unitPath(const SystemD::UnitInfo& info)
{
    auto data = unitPaths[info];

    if (!data)
    {
        return {};
    }

    auto registry = registry_.lock();

    if (!registry)
    {
        g_warning("Unable to get registry to determine path");
        return {};
    }

    /* Execute on the thread so that we're sure that we're not in
       a dbus call to get the value. No racey for you! */
    return registry->impl->thread.executeOnThread<std::string>([&data]() { return data->unitpath; });
}

SystemD::UnitInfo SystemD::unitNew(const std::string& name,
                                   const std::string& path,
                                   const std::shared_ptr<GDBusConnection>& bus)
{
    if (path == "/")
    {
        throw std::runtime_error{"Job path for unit is '/' so likely failed"};
    }

    g_debug("New Unit: %s", name.c_str());

    auto info = parseUnit(name);

    auto data = std::make_shared<UnitData>();
    data->jobpath = path;

    /* We already have this one, continue on */
    if (!unitPaths.insert(std::make_pair(info, data)).second)
    {
        throw std::runtime_error{"Duplicate unit, not really new"};
    }

    /* We need to get the path, we're blocking everyone else on
       this call if they try to get the path. But we're just locking
       up the UAL thread so it should be a big deal. But if someone
       comes an asking at this point we'll think that we have the
       app, but not yet its path */
    GError* error{nullptr};
    auto reg = registry_.lock();

    if (!reg)
    {
        g_warning("Unable to get SystemD unit path for '%s': Registry out of scope", name.c_str());
        throw std::runtime_error{"Unable to get SystemD unit path for '" + name + "': Registry out of scope"};
    }

    GVariant* call = g_dbus_connection_call_sync(bus.get(),                                /* user bus */
                                                 SYSTEMD_DBUS_ADDRESS,                     /* bus name */
                                                 SYSTEMD_DBUS_PATH_MANAGER,                /* path */
                                                 SYSTEMD_DBUS_IFACE_MANAGER,               /* interface */
                                                 "GetUnit",                                /* method */
                                                 g_variant_new("(s)", name.c_str()),       /* params */
                                                 G_VARIANT_TYPE("(o)"),                    /* ret type */
                                                 G_DBUS_CALL_FLAGS_NONE,                   /* flags */
                                                 -1,                                       /* timeout */
                                                 reg->impl->thread.getCancellable().get(), /* cancellable */
                                                 &error);

    if (error != nullptr)
    {
        std::string message = "Unable to get SystemD unit path for '" + name + "': " + error->message;
        g_error_free(error);
        throw std::runtime_error{message};
    }

    /* Parse variant */
    gchar* gpath = nullptr;
    g_variant_get(call, "(o)", &gpath);
    if (gpath != nullptr)
    {
        data->unitpath = gpath;
    }

    g_clear_pointer(&call, g_variant_unref);

    return info;
}

void SystemD::unitRemoved(const std::string& name, const std::string& path)
{
    UnitInfo info = parseUnit(name);

    auto it = unitPaths.find(info);
    if (it != unitPaths.end())
    {
        unitPaths.erase(it);
        emitSignal(sig_appStopped, info);
    }
}

void SystemD::emitSignal(
    core::Signal<const std::shared_ptr<Application>&, const std::shared_ptr<Application::Instance>&>& sig,
    UnitInfo& info)
{
    auto reg = registry_.lock();
    if (!reg)
    {
        g_warning("Unable to emit systemd signal, invalid registry");
        return;
    }

    auto appid = AppID::find(reg, info.appid);
    auto app = Application::create(appid, reg);
    auto inst = std::dynamic_pointer_cast<app_impls::Base>(app)->findInstance(info.inst);

    sig(app, inst);
}

pid_t SystemD::unitPrimaryPid(const AppID& appId, const std::string& job, const std::string& instance)
{
    auto registry = registry_.lock();

    if (!registry)
    {
        g_warning("Unable to get registry to determine primary PID");
        return 0;
    }

    auto unitinfo = SystemD::UnitInfo{appId, job, instance};
    auto unitname = unitName(unitinfo);
    auto unitpath = unitPath(unitinfo);

    if (unitpath.empty())
    {
        return 0;
    }

    return registry->impl->thread.executeOnThread<pid_t>([this, registry, unitname, unitpath]() {
        GError* error{nullptr};
        GVariant* call =
            g_dbus_connection_call_sync(userbus_.get(),                                               /* user bus */
                                        SYSTEMD_DBUS_ADDRESS,                                         /* bus name */
                                        unitpath.c_str(),                                             /* path */
                                        "org.freedesktop.DBus.Properties",                            /* interface */
                                        "Get",                                                        /* method */
                                        g_variant_new("(ss)", SYSTEMD_DBUS_IFACE_SERVICE, "MainPID"), /* params */
                                        G_VARIANT_TYPE("(v)"),                                        /* ret type */
                                        G_DBUS_CALL_FLAGS_NONE,                                       /* flags */
                                        -1,                                                           /* timeout */
                                        registry->impl->thread.getCancellable().get(),                /* cancellable */
                                        &error);

        if (error != nullptr)
        {
            auto message =
                std::string{"Unable to get SystemD PID for '"} + unitname + std::string{"': "} + error->message;
            g_error_free(error);
            throw std::runtime_error(message);
        }

        /* Parse variant */
        GVariant* vpid{nullptr};
        g_variant_get(call, "(v)", &vpid);
        g_clear_pointer(&call, g_variant_unref);

        pid_t pid;
        pid = g_variant_get_uint32(vpid);
        g_clear_pointer(&vpid, g_variant_unref);

        return pid;
    });
}

std::vector<pid_t> SystemD::unitPids(const AppID& appId, const std::string& job, const std::string& instance)
{
    auto registry = registry_.lock();

    if (!registry)
    {
        g_warning("Unable to get registry to determine primary PID");
        return {};
    }

    auto unitinfo = SystemD::UnitInfo{appId, job, instance};
    auto unitname = unitName(unitinfo);
    auto unitpath = unitPath(unitinfo);

    if (unitpath.empty())
    {
        return {};
    }

    auto cgrouppath = registry->impl->thread.executeOnThread<std::string>([this, registry, unitname, unitpath]() {
        GError* error{nullptr};
        GVariant* call =
            g_dbus_connection_call_sync(userbus_.get(),                    /* user bus */
                                        SYSTEMD_DBUS_ADDRESS,              /* bus name */
                                        unitpath.c_str(),                  /* path */
                                        "org.freedesktop.DBus.Properties", /* interface */
                                        "Get",                             /* method */
                                        g_variant_new("(ss)", SYSTEMD_DBUS_IFACE_SERVICE, "ControlGroup"), /* params */
                                        G_VARIANT_TYPE("(v)"),                         /* ret type */
                                        G_DBUS_CALL_FLAGS_NONE,                        /* flags */
                                        -1,                                            /* timeout */
                                        registry->impl->thread.getCancellable().get(), /* cancellable */
                                        &error);

        if (error != nullptr)
        {
            auto message = std::string{"Unable to get SystemD Control Group for '"} + unitname + std::string{"': "} +
                           error->message;
            g_error_free(error);
            throw std::runtime_error(message);
        }

        /* Parse variant */
        GVariant* vstring = nullptr;
        g_variant_get(call, "(v)", &vstring);
        g_clear_pointer(&call, g_variant_unref);

        if (vstring == nullptr)
        {
            return std::string{};
        }

        std::string group;
        auto ggroup = g_variant_get_string(vstring, nullptr);
        if (ggroup != nullptr)
        {
            group = ggroup;
        }
        g_variant_unref(vstring);

        return group;
    });

    gchar* fullpath = g_build_filename(cgroup_root_.c_str(), cgrouppath.c_str(), "tasks", nullptr);
    gchar* pidstr = nullptr;
    GError* error = nullptr;

    g_debug("Getting PIDs from %s", fullpath);
    g_file_get_contents(fullpath, &pidstr, nullptr, &error);
    g_free(fullpath);

    if (error != nullptr)
    {
        g_warning("Unable to read cgroup PID list: %s", error->message);
        g_error_free(error);
        return {};
    }

    gchar** pidlines = g_strsplit(pidstr, "\n", -1);
    g_free(pidstr);
    std::vector<pid_t> pids;

    for (auto i = 0; pidlines[i] != nullptr; i++)
    {
        const gchar* pidline = pidlines[i];
        if (pidline[0] != '\n')
        {
            auto pid = std::atoi(pidline);
            if (pid != 0)
            {
                pids.emplace_back(pid);
            }
        }
    }

    g_strfreev(pidlines);

    return pids;
}

void SystemD::stopUnit(const AppID& appId, const std::string& job, const std::string& instance)
{
    auto registry = registry_.lock();
    auto unitname = unitName(SystemD::UnitInfo{appId, job, instance});

    registry->impl->thread.executeOnThread<bool>([this, registry, unitname] {
        GError* error{nullptr};
        GVariant* call = g_dbus_connection_call_sync(
            userbus_.get(),             /* user bus */
            SYSTEMD_DBUS_ADDRESS,       /* bus name */
            SYSTEMD_DBUS_PATH_MANAGER,  /* path */
            SYSTEMD_DBUS_IFACE_MANAGER, /* interface */
            "StopUnit",                 /* method */
            g_variant_new(
                "(ss)",                  /* params */
                unitname.c_str(),        /* param: specify unit */
                "replace-irreversibly"), /* param: replace the current job but don't allow us to be replaced */
            G_VARIANT_TYPE("(o)"),       /* ret type */
            G_DBUS_CALL_FLAGS_NONE,      /* flags */
            -1,                          /* timeout */
            registry->impl->thread.getCancellable().get(), /* cancellable */
            &error);

        if (error != nullptr)
        {
            auto message =
                std::string{"Unable to get SystemD to stop '"} + unitname + std::string{"': "} + error->message;
            g_error_free(error);
            throw std::runtime_error(message);
        }

        g_clear_pointer(&call, g_variant_unref);

        return true;
    });
}

core::Signal<const std::shared_ptr<Application>&, const std::shared_ptr<Application::Instance>&>& SystemD::appStarted()
{
    /* For systemd we're automatically listening to the UnitNew signal
       and emitting on the object */
    return sig_appStarted;
}

core::Signal<const std::shared_ptr<Application>&, const std::shared_ptr<Application::Instance>&>& SystemD::appStopped()
{
    /* For systemd we're automatically listening to the UnitRemoved signal
       and emitting on the object */
    return sig_appStopped;
}

struct FailedData
{
    std::weak_ptr<Registry> registry;
};

core::Signal<const std::shared_ptr<Application>&, const std::shared_ptr<Application::Instance>&, Registry::FailureType>&
    SystemD::appFailed()
{
    std::call_once(flag_appFailed, [this]() {
        auto reg = registry_.lock();

        reg->impl->thread.executeOnThread<bool>([this, reg]() {
            auto data = new FailedData{reg};

            handle_appFailed = g_dbus_connection_signal_subscribe(
                reg->impl->_dbus.get(),            /* bus */
                SYSTEMD_DBUS_ADDRESS,              /* sender */
                "org.freedesktop.DBus.Properties", /* interface */
                "PropertiesChanged",               /* signal */
                nullptr,                           /* path */
                SYSTEMD_DBUS_IFACE_SERVICE,        /* arg0 */
                G_DBUS_SIGNAL_FLAGS_NONE,
                [](GDBusConnection*, const gchar*, const gchar* path, const gchar*, const gchar*, GVariant* params,
                   gpointer user_data) -> void {
                    auto data = static_cast<FailedData*>(user_data);
                    auto reg = data->registry.lock();

                    if (!reg)
                    {
                        g_warning("Registry object invalid!");
                        return;
                    }

                    auto manager = std::dynamic_pointer_cast<SystemD>(reg->impl->jobs);

                    /* Check to see if this is a path we care about */
                    bool pathfound{false};
                    UnitInfo unitinfo;
                    for (const auto& unit : manager->unitPaths)
                    {
                        if (unit.second->unitpath == path)
                        {
                            pathfound = true;
                            unitinfo = unit.first;
                            break;
                        }
                    }
                    if (!pathfound)
                    {
                        return;
                    }

                    /* Now see if it is a property we care about */
                    auto vdict = g_variant_get_child_value(params, 1);
                    GVariantDict dict;
                    g_variant_dict_init(&dict, vdict);
                    g_clear_pointer(&vdict, g_variant_unref);

                    if (g_variant_dict_contains(&dict, "Result") == FALSE)
                    {
                        /* We don't care about anything else */
                        g_variant_dict_clear(&dict);
                        return;
                    }

                    /* Check to see if it just was successful */
                    const gchar* value{nullptr};
                    g_variant_dict_lookup(&dict, "Result", "&s", &value);

                    if (g_strcmp0(value, "success") == 0)
                    {
                        g_variant_dict_clear(&dict);
                        return;
                    }
                    g_variant_dict_clear(&dict);

                    /* Oh, we might want to do something now */
                    auto reason{Registry::FailureType::CRASH};
                    if (g_strcmp0(value, "exit-code") == 0)
                    {
                        reason = Registry::FailureType::START_FAILURE;
                    }

                    auto appid = AppID::find(reg, unitinfo.appid);
                    auto app = Application::create(appid, reg);
                    auto inst = std::dynamic_pointer_cast<app_impls::Base>(app)->findInstance(unitinfo.inst);

                    manager->sig_appFailed(app, inst, reason);
                },    /* callback */
                data, /* user data */
                [](gpointer user_data) {
                    auto data = static_cast<FailedData*>(user_data);
                    delete data;
                }); /* user data destroy */

            return true;
        });
    });

    return sig_appFailed;
}

}  // namespace manager
}  // namespace jobs
}  // namespace app_launch
}  // namespace ubuntu
