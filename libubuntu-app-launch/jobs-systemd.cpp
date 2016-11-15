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

#include <algorithm>
#include <gio/gio.h>
#include <numeric>
#include <regex>
#include <sys/types.h>
#include <unistd.h>

#include "helpers.h"
#include "jobs-systemd.h"
#include "registry-impl.h"
#include "second-exec-core.h"

extern "C" {
#include "ubuntu-app-launch-trace.h"
}

namespace ubuntu
{
namespace app_launch
{
namespace jobs
{
namespace instance
{

class SystemD : public Base
{
    friend class manager::SystemD;

public:
    explicit SystemD(const AppID& appId,
                     const std::string& job,
                     const std::string& instance,
                     const std::vector<Application::URL>& urls,
                     const std::shared_ptr<Registry>& registry);

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
    return manager->stopUnit(appId_, job_, instance_);
}

}  // namespace instance

namespace manager
{

static const std::string SYSTEMD_DBUS_ADDRESS{"org.freedesktop.systemd1"};
static const std::string SYSTEMD_DBUS_IFACE_MANAGER{"org.freedesktop.systemd1.Manager"};
static const std::string SYSTEMD_DBUS_PATH_MANAGER{"/org/freedesktop/systemd1"};
static const std::string SYSTEMD_DBUS_IFACE_UNIT{"org.freedesktop.systemd1.Unit"};
static const std::string SYSTEMD_DBUS_IFACE_SERVICE{"org.freedesktop.systemd1.Service"};

SystemD::SystemD(std::shared_ptr<Registry> registry)
    : Base(registry)
{
    auto cancel = registry->impl->thread.getCancellable();
    userbus_ = registry->impl->thread.executeOnThread<std::shared_ptr<GDBusConnection>>([this, cancel]() {
        GError* error = nullptr;
        auto bus = std::shared_ptr<GDBusConnection>(
            g_dbus_connection_new_for_address_sync(
                ("unix:path=" + userBusPath()).c_str(), /* path to the user bus */
                (GDBusConnectionFlags)(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                       G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION), /* It is a message bus */
                nullptr,                                                                /* observer */
                cancel.get(), /* cancellable from the thread */
                &error),      /* error */
            [](GDBusConnection* bus) { g_clear_object(&bus); });

        if (error != nullptr)
        {
            std::string message = std::string("Unable to connect to user bus: ") + error->message;
            g_error_free(error);
            throw std::runtime_error(message);
        }

        /* Setup Unit add/remove signals */
        handle_unitNew =
            g_dbus_connection_signal_subscribe(bus.get(),                          /* bus */
                                               nullptr,                            /* sender */
                                               SYSTEMD_DBUS_IFACE_MANAGER.c_str(), /* interface */
                                               "UnitNew",                          /* signal */
                                               SYSTEMD_DBUS_PATH_MANAGER.c_str(),  /* path */
                                               nullptr,                            /* arg0 */
                                               G_DBUS_SIGNAL_FLAGS_NONE,
                                               [](GDBusConnection*, const gchar*, const gchar*, const gchar*,
                                                  const gchar*, GVariant* params, gpointer user_data) -> void {
                                                   auto pthis = static_cast<SystemD*>(user_data);

                                                   const gchar* unitname{nullptr};
                                                   const gchar* unitpath{nullptr};

                                                   g_variant_get(params, "(&s&o)", unitname, unitpath);

                                                   pthis->unitNew(unitname, unitpath);
                                               },        /* callback */
                                               this,     /* user data */
                                               nullptr); /* user data destroy */

        handle_unitRemoved =
            g_dbus_connection_signal_subscribe(bus.get(),                          /* bus */
                                               nullptr,                            /* sender */
                                               SYSTEMD_DBUS_IFACE_MANAGER.c_str(), /* interface */
                                               "UnitRemoved",                      /* signal */
                                               SYSTEMD_DBUS_PATH_MANAGER.c_str(),  /* path */
                                               nullptr,                            /* arg0 */
                                               G_DBUS_SIGNAL_FLAGS_NONE,
                                               [](GDBusConnection*, const gchar*, const gchar*, const gchar*,
                                                  const gchar*, GVariant* params, gpointer user_data) -> void {
                                                   auto pthis = static_cast<SystemD*>(user_data);

                                                   const gchar* unitname{nullptr};
                                                   const gchar* unitpath{nullptr};

                                                   g_variant_get(params, "(&s&o)", unitname, unitpath);

                                                   pthis->unitRemoved(unitname, unitpath);
                                               },        /* callback */
                                               this,     /* user data */
                                               nullptr); /* user data destroy */

        g_dbus_connection_call(
            bus.get(),                          /* user bus */
            SYSTEMD_DBUS_ADDRESS.c_str(),       /* bus name */
            SYSTEMD_DBUS_PATH_MANAGER.c_str(),  /* path */
            SYSTEMD_DBUS_IFACE_MANAGER.c_str(), /* interface */
            "ListUnits",                        /* method */
            nullptr,                            /* params */
            G_VARIANT_TYPE("(a(ssssssouso))"),  /* ret type */
            G_DBUS_CALL_FLAGS_NONE,             /* flags */
            -1,                                 /* timeout */
            cancel.get(),                       /* cancellable */
            [](GObject* obj, GAsyncResult* res, gpointer user_data) {
                auto pthis = static_cast<SystemD*>(user_data);
                GError* error{nullptr};
                GVariant* callt = g_dbus_connection_call_finish(G_DBUS_CONNECTION(obj), res, &error);

                if (error != nullptr)
                {
                    g_warning("Unable to list SystemD units: %s", error->message);
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
                while (g_variant_iter_loop(iter, "(&s&s&s&s&s&s&ou&s&o)", &id, &description, &loadState, &activeState,
                                           &subState, &following, &path, &jobId, &jobType, &jobPath))
                {
                    pthis->unitNew(id, jobPath);
                }

                g_variant_iter_free(iter);
                g_variant_unref(call);
            },
            this);
        return bus;
    });
}

SystemD::~SystemD()
{
    auto dohandle = [&](guint& handle) {
        if (handle != 0)
        {
            g_dbus_connection_signal_unsubscribe(dbus_.get(), handle);
            handle = 0;
        }
    };

    dohandle(handle_unitNew);
    dohandle(handle_unitRemoved);
    dohandle(handle_appFailed);
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

std::vector<std::string> SystemD::parseExec(std::list<std::pair<std::string, std::string>>& env)
{
    auto exec = findEnv("APP_EXEC", env);
    if (exec.empty())
    {
        g_debug("Application exec line is empty?!?!?");
        return {};
    }
    auto uris = findEnv("APP_URIS", env);

    auto execarray = desktop_exec_parse(exec.c_str(), uris.c_str());

    std::vector<std::string> retval;
    retval.reserve(execarray->len);
    for (unsigned int i = 0; i < execarray->len; i++)
    {
        retval.emplace_back(g_array_index(execarray, gchar*, i));
    }

    g_array_set_clear_func(execarray, g_free);
    g_array_free(execarray, FALSE); /* TODO: Not TRUE? */

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
    GError* error{nullptr};
    GVariant* result{nullptr};

    tracepoint(ubuntu_app_launch, libual_start_message_callback, std::string(data->ptr->appId_).c_str());

    g_debug("Started Message Callback: %s", std::string(data->ptr->appId_).c_str());

    result = g_dbus_connection_call_finish(G_DBUS_CONNECTION(obj), res, &error);

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
                            urls.get());                                               /* urls */
            }

            g_free(remote_error);
        }
        else
        {
            g_warning("Unable to emit event to start application: %s", error->message);
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
        std::string value = getenv(envname.c_str());
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
            std::string envfull = environ[i];
            std::string envname;
            bool seenequal = false;
            std::remove_copy_if(envfull.begin(), envfull.end(), std::back_inserter(envname),
                                [&seenequal](const char c) {
                                    if (c == '=')
                                    {
                                        seenequal = true;
                                    }
                                    return seenequal;
                                });
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

            auto handshake = starting_handshake_start(appIdStr.c_str(), timeout);
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
            copyEnvByPrefix("DBUS_", env);
            copyEnvByPrefix("MIR_", env);
            copyEnvByPrefix("QT_", env);
            copyEnvByPrefix("UBUNTU_", env);
            copyEnvByPrefix("UNITY_", env);
            copyEnvByPrefix("XDG_", env);

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

            /* ExecStart */
            auto commands = parseExec(env);
            gchar* pathexec{nullptr};
            if (!commands.empty() && ((pathexec = g_find_program_in_path(commands[0].c_str())) != nullptr))
            {
                g_variant_builder_open(&builder, G_VARIANT_TYPE_TUPLE);
                g_variant_builder_add_value(&builder, g_variant_new_string("ExecStart"));
                g_variant_builder_open(&builder, G_VARIANT_TYPE_VARIANT);
                g_variant_builder_open(&builder, G_VARIANT_TYPE_ARRAY);

                g_variant_builder_open(&builder, G_VARIANT_TYPE_TUPLE);
                g_variant_builder_add_value(&builder, g_variant_new_take_string(pathexec));

                g_variant_builder_open(&builder, G_VARIANT_TYPE_ARRAY);
                for (auto param : commands)
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
                                   SYSTEMD_DBUS_ADDRESS.c_str(),                  /* service name */
                                   SYSTEMD_DBUS_PATH_MANAGER.c_str(),             /* Path */
                                   SYSTEMD_DBUS_IFACE_MANAGER.c_str(),            /* interface */
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

    for (const auto& unit : unitPaths)
    {
        const SystemD::UnitInfo& unitinfo = unit.first;

        if (job != unitinfo.job)
        {
            continue;
        }

        if (std::string(appID) != unitinfo.appid)
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
    auto allJobs = getAllJobs();
    std::set<std::string> appids;
    auto registry = registry_.lock();

    if (!registry)
    {
        g_warning("Unable to list instances without a registry");
        return {};
    }

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
        auto id = AppID::find(appid);
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

SystemD::UnitInfo SystemD::parseUnit(const std::string& unit)
{
    std::smatch match;
    if (!std::regex_match(unit, match, unitNaming))
    {
        throw std::runtime_error{"Unable to parse unit name: " + unit};
    }

    return {match[2].str(), match[1].str(), match[3].str()};
}

std::string SystemD::unitName(const SystemD::UnitInfo& info)
{
    return std::string{"ubuntu-app-launch-"} + info.job + "-" + info.appid + "-" + info.inst + ".service";
}

void SystemD::unitNew(const std::string& name, const std::string& path)
{
    UnitInfo info;
    try
    {
        info = parseUnit(name);
    }
    catch (std::runtime_error& e)
    {
        return;
    }

    if (unitPaths.insert(std::make_pair(info, path)).second)
    {
        emitSignal(sig_appStarted, info);
    }
}

void SystemD::unitRemoved(const std::string& name, const std::string& path)
{
    UnitInfo info;
    try
    {
        info = parseUnit(name);
    }
    catch (std::runtime_error& e)
    {
        return;
    }

    auto it = unitPaths.find(info);
    if (it != unitPaths.end())
    {
        unitPaths.erase(it);
        emitSignal(sig_appStopped, info);
    }
}

void SystemD::emitSignal(core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>>& sig,
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

    // TODO: Figure otu creating instances

    sig(app, {});
}

pid_t SystemD::unitPrimaryPid(const AppID& appId, const std::string& job, const std::string& instance)
{
    auto registry = registry_.lock();
    auto unitinfo = SystemD::UnitInfo{appId, job, instance};
    auto unitname = unitName(unitinfo);
    auto unitpath = unitPaths[unitinfo];

    if (unitpath.empty())
    {
        return 0;
    }

    return registry->impl->thread.executeOnThread<pid_t>([this, registry, unitname, unitpath]() {
        GError* error{nullptr};
        GVariant* call = g_dbus_connection_call_sync(
            userbus_.get(),                                                       /* user bus */
            SYSTEMD_DBUS_ADDRESS.c_str(),                                         /* bus name */
            unitpath.c_str(),                                                     /* path */
            "org.freedesktop.DBus.Properties",                                    /* interface */
            "Get",                                                                /* method */
            g_variant_new("(ss)", SYSTEMD_DBUS_IFACE_SERVICE.c_str(), "MainPID"), /* params */
            G_VARIANT_TYPE("(v)"),                                                /* ret type */
            G_DBUS_CALL_FLAGS_NONE,                                               /* flags */
            -1,                                                                   /* timeout */
            registry->impl->thread.getCancellable().get(),                        /* cancellable */
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
        g_variant_unref(call);

        pid_t pid;
        pid = g_variant_get_uint32(vpid);
        g_variant_unref(vpid);

        return pid;
    });
}

std::vector<pid_t> SystemD::unitPids(const AppID& appId, const std::string& job, const std::string& instance)
{
    auto registry = registry_.lock();
    auto unitinfo = SystemD::UnitInfo{appId, job, instance};
    auto unitname = unitName(unitinfo);
    auto unitpath = unitPaths[unitinfo];

    if (unitpath.empty())
    {
        return {};
    }

    auto cgrouppath = registry->impl->thread.executeOnThread<std::string>([this, registry, unitname, unitpath]() {
        GError* error{nullptr};
        GVariant* call = g_dbus_connection_call_sync(
            userbus_.get(),                                                            /* user bus */
            SYSTEMD_DBUS_ADDRESS.c_str(),                                              /* bus name */
            unitpath.c_str(),                                                          /* path */
            "org.freedesktop.DBus.Properties",                                         /* interface */
            "Get",                                                                     /* method */
            g_variant_new("(ss)", SYSTEMD_DBUS_IFACE_SERVICE.c_str(), "ControlGroup"), /* params */
            G_VARIANT_TYPE("(v)"),                                                     /* ret type */
            G_DBUS_CALL_FLAGS_NONE,                                                    /* flags */
            -1,                                                                        /* timeout */
            registry->impl->thread.getCancellable().get(),                             /* cancellable */
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
        g_variant_unref(call);

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

    gchar* fullpath = g_build_filename("/sys", "fs", "cgroup", "systemd", cgrouppath.c_str(), "tasks", nullptr);
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
        GVariant* call = g_dbus_connection_call_sync(userbus_.get(),                                  /* user bus */
                                                     SYSTEMD_DBUS_ADDRESS.c_str(),                    /* bus name */
                                                     SYSTEMD_DBUS_PATH_MANAGER.c_str(),               /* path */
                                                     SYSTEMD_DBUS_IFACE_MANAGER.c_str(),              /* interface */
                                                     "StopUnit",                                      /* method */
                                                     g_variant_new("(ss)", unitname.c_str(), "fail"), /* params */
                                                     G_VARIANT_TYPE("(o)"),                           /* ret type */
                                                     G_DBUS_CALL_FLAGS_NONE,                          /* flags */
                                                     -1,                                              /* timeout */
                                                     registry->impl->thread.getCancellable().get(),   /* cancellable */
                                                     &error);

        if (error != nullptr)
        {
            auto message =
                std::string{"Unable to get SystemD to stop '"} + unitname + std::string{"': "} + error->message;
            g_error_free(error);
            throw std::runtime_error(message);
        }

        g_variant_unref(call);

        return true;
    });
}

core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>>& SystemD::appStarted()
{
    /* For systemd we're automatically listening to the UnitNew signal
       and emitting on the object */
    return sig_appStarted;
}

core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>>& SystemD::appStopped()
{
    /* For systemd we're automatically listening to the UnitRemoved signal
       and emitting on the object */
    return sig_appStopped;
}

struct FailedData
{
    std::weak_ptr<Registry> registry;
};

core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>, Registry::FailureType>&
    SystemD::appFailed()
{
    std::call_once(flag_appFailed, [this]() {
        auto reg = registry_.lock();

        reg->impl->thread.executeOnThread<bool>([this, reg]() {
            auto data = new FailedData{reg};

            handle_appFailed =
                g_dbus_connection_signal_subscribe(reg->impl->_dbus.get(),             /* bus */
                                                   SYSTEMD_DBUS_ADDRESS.c_str(),       /* sender */
                                                   "org.freedesktop.DBus.Properties",  /* interface */
                                                   "PropertiesChanged",                /* signal */
                                                   nullptr,                            /* path */
                                                   SYSTEMD_DBUS_IFACE_SERVICE.c_str(), /* arg0 */
                                                   G_DBUS_SIGNAL_FLAGS_NONE,
                                                   [](GDBusConnection*, const gchar*, const gchar*, const gchar*,
                                                      const gchar*, GVariant* params, gpointer user_data) -> void {
                                                       auto data = reinterpret_cast<FailedData*>(user_data);
                                                       auto reg = data->registry.lock();

                                                       if (!reg)
                                                       {
                                                           g_warning("Registry object invalid!");
                                                           return;
                                                       }

                                                       /* TODO */
                                                   },    /* callback */
                                                   data, /* user data */
                                                   [](gpointer user_data) {
                                                       auto data = reinterpret_cast<FailedData*>(user_data);
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
