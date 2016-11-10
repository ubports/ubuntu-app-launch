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
#include <cerrno>
#include <cstring>
#include <map>
#include <numeric>
#include <regex>

#include <cgmanager/cgmanager.h>
#include <upstart.h>

#include "helpers.h"
#include "registry-impl.h"
#include "second-exec-core.h"

extern "C" {
#include "ubuntu-app-launch-trace.h"
}

#include "jobs-upstart.h"

namespace ubuntu
{
namespace app_launch
{
namespace jobs
{
namespace instance
{

/** An object that represents an instance of a job on Upstart. This
    then implements everything needed by the instance interface. Most
    applications tie into this today and use it as the backend for
    their instances. */
class Upstart : public Base
{
public:
    explicit Upstart(const AppID& appId,
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

    /* C Callback */
    static void application_start_cb(GObject* obj, GAsyncResult* res, gpointer user_data);

private:
    std::string upstartJobPath(const std::string& job);
    std::string upstartName();

    static std::shared_ptr<gchar*> urlsToStrv(const std::vector<Application::URL>& urls);
};

/** Uses Upstart to get the primary PID of the instance using Upstart's
    DBus interface */
pid_t Upstart::primaryPid()
{
    auto jobpath = upstartJobPath(job_);
    if (jobpath.empty())
    {
        g_debug("Unable to get a valid job path");
        return 0;
    }

    return registry_->impl->thread.executeOnThread<pid_t>([this, &jobpath]() -> pid_t {
        GError* error = nullptr;

        std::string instancename = std::string(appId_);
        if (job_ != "application-click")
        {
            instancename += "-" + instance_;
        }

        g_debug("Getting instance by name: %s", instance_.c_str());
        GVariant* vinstance_path =
            g_dbus_connection_call_sync(registry_->impl->_dbus.get(),                   /* connection */
                                        DBUS_SERVICE_UPSTART,                           /* service */
                                        jobpath.c_str(),                                /* object path */
                                        DBUS_INTERFACE_UPSTART_JOB,                     /* iface */
                                        "GetInstanceByName",                            /* method */
                                        g_variant_new("(s)", instancename.c_str()),     /* params */
                                        G_VARIANT_TYPE("(o)"),                          /* return type */
                                        G_DBUS_CALL_FLAGS_NONE,                         /* flags */
                                        -1,                                             /* timeout: default */
                                        registry_->impl->thread.getCancellable().get(), /* cancellable */
                                        &error);

        if (error != nullptr)
        {
            g_warning("Unable to get instance '%s' of job '%s': %s", instance_.c_str(), job_.c_str(), error->message);
            g_error_free(error);
            return 0;
        }

        /* Jump rope to make this into a C++ type */
        std::string instance_path;
        gchar* cinstance_path = nullptr;
        g_variant_get(vinstance_path, "(o)", &cinstance_path);
        g_variant_unref(vinstance_path);
        if (cinstance_path != nullptr)
        {
            instance_path = cinstance_path;
            g_free(cinstance_path);
        }

        if (instance_path.empty())
        {
            g_debug("No instance object for instance name: %s", instance_.c_str());
            return 0;
        }

        GVariant* props_tuple =
            g_dbus_connection_call_sync(registry_->impl->_dbus.get(),                          /* connection */
                                        DBUS_SERVICE_UPSTART,                                  /* service */
                                        instance_path.c_str(),                                 /* object path */
                                        "org.freedesktop.DBus.Properties",                     /* interface */
                                        "GetAll",                                              /* method */
                                        g_variant_new("(s)", DBUS_INTERFACE_UPSTART_INSTANCE), /* params */
                                        G_VARIANT_TYPE("(a{sv})"),                             /* return type */
                                        G_DBUS_CALL_FLAGS_NONE,                                /* flags */
                                        -1,                                                    /* timeout: default */
                                        registry_->impl->thread.getCancellable().get(),        /* cancellable */
                                        &error);

        if (error != nullptr)
        {
            g_warning("Unable to name of properties '%s': %s", instance_path.c_str(), error->message);
            g_error_free(error);
            error = nullptr;
            return 0;
        }

        GVariant* props_dict = g_variant_get_child_value(props_tuple, 0);

        pid_t retval = 0;
        GVariant* processes = g_variant_lookup_value(props_dict, "processes", G_VARIANT_TYPE("a(si)"));
        if (processes != nullptr && g_variant_n_children(processes) > 0)
        {

            GVariant* first_entry = g_variant_get_child_value(processes, 0);
            GVariant* pidv = g_variant_get_child_value(first_entry, 1);

            retval = g_variant_get_int32(pidv);

            g_variant_unref(pidv);
            g_variant_unref(first_entry);
        }
        else
        {
            g_debug("Unable to get 'processes' from properties of instance at path: %s", instance_path.c_str());
        }

        g_variant_unref(props_dict);

        return retval;
    });
}

/** Generate the full name of the Upstart job for the job, the
    instance and how all those fit together.

    Handles the special case of application-click which isn't designed
    to have multi-instance apps.
*/
std::string Upstart::upstartName()
{
    std::string path = job_ + "-" + std::string(appId_);
    if (job_ != "application-click")
    {
        path += "-";
    }
    if (!instance_.empty())
    {
        path += instance_;
    }

    return path;
}

/** Gets the path to the log file for this instance */
std::string Upstart::logPath()
{
    std::string logfile = upstartName() + ".log";

    gchar* cpath = g_build_filename(g_get_user_cache_dir(), "upstart", logfile.c_str(), nullptr);
    std::string path(cpath);
    g_free(cpath);

    return path;
}

/** Returns all the PIDs that are in the cgroup for this application */
std::vector<pid_t> Upstart::pids()
{
    auto manager = std::dynamic_pointer_cast<manager::Upstart>(registry_->impl->jobs);
    auto pids = manager->pidsFromCgroup(upstartName());
    g_debug("Got %d PIDs for AppID '%s'", int(pids.size()), std::string(appId_).c_str());
    return pids;
}

/** Stops this instance by asking Upstart to stop it. Upstart will then
    send a SIGTERM and five seconds later start killing things. */
void Upstart::stop()
{
    if (!registry_->impl->thread.executeOnThread<bool>([this]() {
            auto manager = std::dynamic_pointer_cast<manager::Upstart>(registry_->impl->jobs);

            g_debug("Stopping job %s app_id %s instance_id %s", job_.c_str(), std::string(appId_).c_str(),
                    instance_.c_str());

            auto jobpath = manager->upstartJobPath(job_);
            if (jobpath.empty())
            {
                throw new std::runtime_error("Unable to get job path for Upstart job '" + job_ + "'");
            }

            GVariantBuilder builder;
            g_variant_builder_init(&builder, G_VARIANT_TYPE_TUPLE);
            g_variant_builder_open(&builder, G_VARIANT_TYPE_ARRAY);

            g_variant_builder_add_value(
                &builder, g_variant_new_take_string(g_strdup_printf("APP_ID=%s", std::string(appId_).c_str())));

            if (!instance_.empty())
            {
                g_variant_builder_add_value(
                    &builder, g_variant_new_take_string(g_strdup_printf("INSTANCE_ID=%s", instance_.c_str())));
            }

            g_variant_builder_close(&builder);
            g_variant_builder_add_value(&builder, g_variant_new_boolean(FALSE)); /* wait */

            GError* error = nullptr;
            GVariant* stop_variant =
                g_dbus_connection_call_sync(registry_->impl->_dbus.get(),                   /* Dbus */
                                            DBUS_SERVICE_UPSTART,                           /* Upstart name */
                                            jobpath.c_str(),                                /* path */
                                            DBUS_INTERFACE_UPSTART_JOB,                     /* interface */
                                            "Stop",                                         /* method */
                                            g_variant_builder_end(&builder),                /* params */
                                            nullptr,                                        /* return */
                                            G_DBUS_CALL_FLAGS_NONE,                         /* flags */
                                            -1,                                             /* timeout: default */
                                            registry_->impl->thread.getCancellable().get(), /* cancellable */
                                            &error);                                        /* error (hopefully not) */

            g_clear_pointer(&stop_variant, g_variant_unref);

            if (error != nullptr)
            {
                g_warning("Unable to stop job %s app_id %s instance_id %s: %s", job_.c_str(),
                          std::string(appId_).c_str(), instance_.c_str(), error->message);
                g_error_free(error);
                return false;
            }

            return true;
        }))
    {
        g_warning("Unable to stop Upstart instance");
    }
}

/** Create a new Upstart Instance object that can track the job and
    get information about it.

    \param appId Application ID
    \param job Upstart job name
    \param instance Upstart instance name
    \param urls URLs sent to the application (only on launch today)
    \param registry Registry of persistent connections to use
*/
Upstart::Upstart(const AppID& appId,
                 const std::string& job,
                 const std::string& instance,
                 const std::vector<Application::URL>& urls,
                 const std::shared_ptr<Registry>& registry)
    : Base(appId, job, instance, urls, registry)
{
    g_debug("Creating a new Upstart for '%s' instance '%s'", std::string(appId).c_str(), instance.c_str());
}

/** Reformat a C++ vector of URLs into a C GStrv of strings

    \param urls Vector of URLs to make into C strings
*/
std::shared_ptr<gchar*> Upstart::urlsToStrv(const std::vector<Application::URL>& urls)
{
    if (urls.empty())
    {
        return {};
    }

    auto array = g_array_new(TRUE, FALSE, sizeof(gchar*));

    for (auto url : urls)
    {
        auto str = g_strdup(url.value().c_str());
        g_debug("Converting URL: %s", str);
        g_array_append_val(array, str);
    }

    return std::shared_ptr<gchar*>((gchar**)g_array_free(array, FALSE), g_strfreev);
}

/** Small helper that we can new/delete to work better with C stuff */
struct StartCHelper
{
    std::shared_ptr<Upstart> ptr;
};

/** Callback from starting an application. It checks to see whether the
    app is already running. If it is already running then we need to send
    the URLs to it via DBus.

    \param obj The GDBusConnection object
    \param res Async result object
    \param user_data A pointer to a StartCHelper structure
*/
void Upstart::application_start_cb(GObject* obj, GAsyncResult* res, gpointer user_data)
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
            if (g_strcmp0(remote_error, "com.ubuntu.Upstart0_6.Error.AlreadyStarted") == 0)
            {
                auto urls = urlsToStrv(data->ptr->urls_);
                second_exec(data->ptr->registry_->impl->_dbus.get(),                   /* DBus */
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

std::string Upstart::upstartJobPath(const std::string& job)
{
    auto manager = std::dynamic_pointer_cast<manager::Upstart>(registry_->impl->jobs);
    return manager->upstartJobPath(job);
}

}  // namespace instances

namespace manager
{

Upstart::Upstart(std::shared_ptr<Registry> registry)
    : Base(registry)
{
}

Upstart::~Upstart()
{
    auto dohandle = [&](guint& handle) {
        if (handle != 0)
        {
            g_dbus_connection_signal_unsubscribe(dbus_.get(), handle);
            handle = 0;
        }
    };

    dohandle(handle_appStarted);
    dohandle(handle_appStopped);
    dohandle(handle_appFailed);
    dohandle(handle_appPaused);
    dohandle(handle_appResumed);
    dohandle(handle_managerSignalFocus);
    dohandle(handle_managerSignalResume);
    dohandle(handle_managerSignalStarting);
}

/** Launch an application and create a new Upstart instance object to track
    its progress.

    \param appId Application ID
    \param job Upstart job name
    \param instance Upstart instance name
    \param urls URLs sent to the application (only on launch today)
    \param mode Whether or not to setup the environment for testing
    \param getenv A function to get additional environment variable when appropriate
*/
std::shared_ptr<Application::Instance> Upstart::launch(
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
    return registry->impl->thread.executeOnThread<std::shared_ptr<instance::Upstart>>(
        [&]() -> std::shared_ptr<instance::Upstart> {
            auto manager = std::dynamic_pointer_cast<manager::Upstart>(registry->impl->jobs);
            std::string appIdStr{appId};
            g_debug("Initializing params for an new instance::Upstart for: %s", appIdStr.c_str());

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

            /* Figure out the DBus path for the job */
            auto jobpath = manager->upstartJobPath(job);

            /* Build up our environment */
            auto env = getenv();

            env.emplace_back(std::make_pair("APP_ID", appIdStr));                           /* Application ID */
            env.emplace_back(std::make_pair("APP_LAUNCHER_PID", std::to_string(getpid()))); /* Who we are, for bugs */

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

            g_variant_builder_open(&builder, G_VARIANT_TYPE_ARRAY);

            for (const auto& envvar : env)
            {
                g_variant_builder_add_value(&builder, g_variant_new_take_string(g_strdup_printf(
                                                          "%s=%s", envvar.first.c_str(), envvar.second.c_str())));
            }

            g_variant_builder_close(&builder);
            g_variant_builder_add_value(&builder, g_variant_new_boolean(TRUE));

            auto retval = std::make_shared<instance::Upstart>(appId, job, instance, urls, registry);
            auto chelper = new instance::StartCHelper{};
            chelper->ptr = retval;

            tracepoint(ubuntu_app_launch, handshake_wait, appIdStr.c_str());
            starting_handshake_wait(handshake);
            tracepoint(ubuntu_app_launch, handshake_complete, appIdStr.c_str());

            /* Call the job start function */
            g_debug("Asking Upstart to start task for: %s", appIdStr.c_str());
            g_dbus_connection_call(registry->impl->_dbus.get(),                   /* bus */
                                   DBUS_SERVICE_UPSTART,                          /* service name */
                                   jobpath.c_str(),                               /* Path */
                                   DBUS_INTERFACE_UPSTART_JOB,                    /* interface */
                                   "Start",                                       /* method */
                                   g_variant_builder_end(&builder),               /* params */
                                   nullptr,                                       /* return */
                                   G_DBUS_CALL_FLAGS_NONE,                        /* flags */
                                   -1,                                            /* default timeout */
                                   registry->impl->thread.getCancellable().get(), /* cancellable */
                                   instance::Upstart::application_start_cb,       /* callback */
                                   chelper                                        /* object */
                                   );

            tracepoint(ubuntu_app_launch, libual_start_message_sent, appIdStr.c_str());

            return retval;
        });
}

/** Special characters that could be an application name that
    would activate in a regex */
const static std::regex regexCharacters("([\\.\\-])");

std::shared_ptr<Application::Instance> Upstart::existing(const AppID& appId,
                                                         const std::string& job,
                                                         const std::string& instance,
                                                         const std::vector<Application::URL>& urls)
{
    return std::make_shared<instance::Upstart>(appId, job, instance, urls, registry_.lock());
}

std::vector<std::shared_ptr<instance::Base>> Upstart::instances(const AppID& appID, const std::string& job)
{
    std::vector<std::shared_ptr<instance::Base>> vect;
    auto startsWith = std::string(appID);
    if (job != "application-click")
    {
        startsWith += "-";
    }

    auto regexstr =
        std::string{"^(?:"} + std::regex_replace(startsWith, regexCharacters, "\\$&") + std::string{")(\\d*)$"};
    auto instanceRegex = std::regex(regexstr);

    for (auto instance : upstartInstancesForJob(job))
    {
        std::smatch instanceMatch;
        g_debug("Looking at job '%s' instance: %s", job.c_str(), instance.c_str());
        if (std::regex_match(instance, instanceMatch, instanceRegex))
        {
            auto app = existing(appID, job, instanceMatch[1].str(), {});
            vect.emplace_back(std::dynamic_pointer_cast<instance::Base>(app));
        }
    }

    g_debug("App '%s' has %d instances", std::string(appID).c_str(), int(vect.size()));

    return vect;
}

/** Structure to track the data needed for upstart events. This cleans
    up the lifecycle as we're passing this as a pointer through the
    GLib calls. */
struct upstartEventData
{
    /** Keeping a weak pointer because the handle is held by
        the registry implementation. */
    std::weak_ptr<Registry> weakReg;
};

/** Take the GVariant of parameters and turn them into an application and
    and instance. Easier to read in the smaller function */
std::tuple<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>> Upstart::managerParams(
    const std::shared_ptr<GVariant>& params, const std::shared_ptr<Registry>& reg)
{
    std::shared_ptr<Application> app;
    std::shared_ptr<Application::Instance> instance;

    const gchar* cappid = nullptr;
    g_variant_get(params.get(), "(&s)", &cappid);

    auto appid = ubuntu::app_launch::AppID::find(reg, cappid);
    app = ubuntu::app_launch::Application::create(appid, reg);

    return std::make_tuple(app, instance);
}

/** Used to store data for manager based signal handlers. Has a link to the
    registry and the callback to use in a C++ style. */
struct managerEventData
{
    /* Keeping a weak pointer because the handle is held by
       the registry implementation. */
    std::weak_ptr<Registry> weakReg;
    std::function<void(const std::shared_ptr<Registry>& reg,
                       const std::shared_ptr<Application>& app,
                       const std::shared_ptr<Application::Instance>& instance,
                       const std::shared_ptr<GDBusConnection>&,
                       const std::string&,
                       const std::shared_ptr<GVariant>&)>
        func;
};

/** Register for a signal for the manager. All of the signals needed this same
    code so it got pulled out into a function. Takes the same of the signal, the registry
    that we're using and a function to call after we've messaged all the parameters
    into being something C++-ish. */
guint Upstart::managerSignalHelper(const std::shared_ptr<Registry>& reg,
                                   const std::string& signalname,
                                   std::function<void(const std::shared_ptr<Registry>& reg,
                                                      const std::shared_ptr<Application>& app,
                                                      const std::shared_ptr<Application::Instance>& instance,
                                                      const std::shared_ptr<GDBusConnection>&,
                                                      const std::string&,
                                                      const std::shared_ptr<GVariant>&)> responsefunc)
{
    managerEventData* focusdata = new managerEventData{reg, responsefunc};

    return g_dbus_connection_signal_subscribe(
        reg->impl->_dbus.get(),          /* bus */
        nullptr,                         /* sender */
        "com.canonical.UbuntuAppLaunch", /* interface */
        signalname.c_str(),              /* signal */
        "/",                             /* path */
        nullptr,                         /* arg0 */
        G_DBUS_SIGNAL_FLAGS_NONE,
        [](GDBusConnection* cconn, const gchar* csender, const gchar*, const gchar*, const gchar*, GVariant* params,
           gpointer user_data) -> void {
            auto data = reinterpret_cast<managerEventData*>(user_data);
            auto reg = data->weakReg.lock();

            /* If we're still conneted and the manager has been cleared
               we'll just be a no-op */
            auto ljobs = std::dynamic_pointer_cast<Upstart>(reg->impl->jobs);
            if (!ljobs->manager_)
            {
                return;
            }

            auto vparams = std::shared_ptr<GVariant>(g_variant_ref(params), g_variant_unref);
            auto conn = std::shared_ptr<GDBusConnection>(reinterpret_cast<GDBusConnection*>(g_object_ref(cconn)),
                                                         [](GDBusConnection* con) { g_clear_object(&con); });
            std::string sender = csender;
            std::shared_ptr<Application> app;
            std::shared_ptr<Application::Instance> instance;

            std::tie(app, instance) = managerParams(vparams, reg);

            data->func(reg, app, instance, conn, sender, vparams);
        },
        focusdata,
        [](gpointer user_data) {
            auto data = reinterpret_cast<managerEventData*>(user_data);
            delete data;
        }); /* user data destroy */
}

/** Set the manager for the registry. This includes tracking the pointer
    as well as setting up the signals to call back into the manager. The
    signals are only setup once per registry even if the manager is cleared
    and changed again. They will just be no-op's in those cases.
*/
void Upstart::setManager(std::shared_ptr<Registry::Manager> manager)
{
    if (manager_)
    {
        throw std::runtime_error("Already have a manager and trying to set another");
    }

    g_debug("Setting a new manager");
    manager_ = manager;

    std::call_once(flag_managerSignals, [this]() {
        auto reg = registry_.lock();

        if (!reg->impl->thread.executeOnThread<bool>([this, reg]() {
                handle_managerSignalFocus = managerSignalHelper(
                    reg, "UnityFocusRequest",
                    [](const std::shared_ptr<Registry>& reg, const std::shared_ptr<Application>& app,
                       const std::shared_ptr<Application::Instance>& instance,
                       const std::shared_ptr<GDBusConnection>& conn, const std::string& sender,
                       const std::shared_ptr<GVariant>& params) {
                        /* Nothing to do today */
                        std::dynamic_pointer_cast<Upstart>(reg->impl->jobs)
                            ->manager_->focusRequest(app, instance, [](bool response) {
                                /* NOTE: We have no clue what thread this is gonna be
                                   executed on, but since we're just talking to the GDBus
                                   thread it isn't an issue today. Be careful in changing
                                   this code. */
                            });
                    });
                handle_managerSignalStarting = managerSignalHelper(
                    reg, "UnityStartingBroadcast",
                    [](const std::shared_ptr<Registry>& reg, const std::shared_ptr<Application>& app,
                       const std::shared_ptr<Application::Instance>& instance,
                       const std::shared_ptr<GDBusConnection>& conn, const std::string& sender,
                       const std::shared_ptr<GVariant>& params) {

                        std::dynamic_pointer_cast<Upstart>(reg->impl->jobs)
                            ->manager_->startingRequest(app, instance, [conn, sender, params](bool response) {
                                /* NOTE: We have no clue what thread this is gonna be
                                   executed on, but since we're just talking to the GDBus
                                   thread it isn't an issue today. Be careful in changing
                                   this code. */
                                if (response)
                                {
                                    g_dbus_connection_emit_signal(conn.get(), sender.c_str(),      /* destination */
                                                                  "/",                             /* path */
                                                                  "com.canonical.UbuntuAppLaunch", /* interface */
                                                                  "UnityStartingSignal",           /* signal */
                                                                  params.get(), /* params, the same */
                                                                  nullptr);     /* error */
                                }
                            });
                    });
                handle_managerSignalResume = managerSignalHelper(
                    reg, "UnityResumeRequest",
                    [](const std::shared_ptr<Registry>& reg, const std::shared_ptr<Application>& app,
                       const std::shared_ptr<Application::Instance>& instance,
                       const std::shared_ptr<GDBusConnection>& conn, const std::string& sender,
                       const std::shared_ptr<GVariant>& params) {
                        std::dynamic_pointer_cast<Upstart>(reg->impl->jobs)
                            ->manager_->resumeRequest(app, instance, [conn, sender, params](bool response) {
                                /* NOTE: We have no clue what thread this is gonna be
                                   executed on, but since we're just talking to the GDBus
                                   thread it isn't an issue today. Be careful in changing
                                   this code. */
                                if (response)
                                {
                                    g_dbus_connection_emit_signal(conn.get(), sender.c_str(),      /* destination */
                                                                  "/",                             /* path */
                                                                  "com.canonical.UbuntuAppLaunch", /* interface */
                                                                  "UnityResumeResponse",           /* signal */
                                                                  params.get(), /* params, the same */
                                                                  nullptr);     /* error */
                                }
                            });
                    });

                return true;
            }))
        {
            g_warning("Unable to install manager signals");
        }
    });
}

/** Clear the manager pointer */
void Upstart::clearManager()
{
    g_debug("Clearing the manager");
    manager_.reset();
}

/** Regex to parse the JOB environment variable from Upstart */
std::regex jobenv_regex{"^JOB=(application\\-(?:click|snap|legacy))$"};
/** Regex to parse the INSTANCE environment variable from Upstart */
std::regex instanceenv_regex{"^INSTANCE=(.*?)(?:\\-([0-9]*))?+$"};

/** Core of most of the events that come from Upstart directly. Includes parsing of the
    Upstart event environment and calling the appropriate signal with the right Application
    object and eventually its instance */
void Upstart::upstartEventEmitted(
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>>& signal,
    std::shared_ptr<GVariant> params,
    const std::shared_ptr<Registry>& reg)
{
    std::string jobname;
    std::string sappid;
    std::string instance;

    gchar* env = nullptr;
    GVariant* envs = g_variant_get_child_value(params.get(), 1);
    GVariantIter iter;
    g_variant_iter_init(&iter, envs);

    while (g_variant_iter_loop(&iter, "s", &env))
    {
        std::smatch match;
        std::string senv = env;

        if (std::regex_match(senv, match, jobenv_regex))
        {
            jobname = match[1].str();
        }
        else if (std::regex_match(senv, match, instanceenv_regex))
        {
            sappid = match[1].str();
            instance = match[2].str();
        }
    }

    g_variant_unref(envs);

    if (jobname.empty())
    {
        return;
    }

    g_debug("Upstart Event for job '%s' appid '%s' instance '%s'", jobname.c_str(), sappid.c_str(), instance.c_str());

    auto appid = AppID::find(reg, sappid);
    auto app = Application::create(appid, reg);

    // TODO: Figure otu creating instances

    signal(app, {});
}

/** Grab the signal object for application startup. If we're not already listing for
    those signals this sets up a listener for them. */
core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>>& Upstart::appStarted()
{
    std::call_once(flag_appStarted, [this]() {
        auto reg = registry_.lock();

        reg->impl->thread.executeOnThread<bool>([this, reg]() {
            upstartEventData* data = new upstartEventData{reg};

            handle_appStarted = g_dbus_connection_signal_subscribe(
                reg->impl->_dbus.get(), /* bus */
                nullptr,                /* sender */
                DBUS_INTERFACE_UPSTART, /* interface */
                "EventEmitted",         /* signal */
                DBUS_PATH_UPSTART,      /* path */
                "started",              /* arg0 */
                G_DBUS_SIGNAL_FLAGS_NONE,
                [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar*, GVariant* params,
                   gpointer user_data) -> void {
                    auto data = reinterpret_cast<upstartEventData*>(user_data);
                    auto reg = data->weakReg.lock();
                    auto sparams = std::shared_ptr<GVariant>(g_variant_ref(params), g_variant_unref);
                    auto upstart = std::dynamic_pointer_cast<Upstart>(reg->impl->jobs);
                    upstart->upstartEventEmitted(upstart->sig_appStarted, sparams, reg);
                },    /* callback */
                data, /* user data */
                [](gpointer user_data) {
                    auto data = reinterpret_cast<upstartEventData*>(user_data);
                    delete data;
                }); /* user data destroy */

            return true;
        });
    });

    return sig_appStarted;
}

/** Grab the signal object for application stopping. If we're not already listing for
    those signals this sets up a listener for them. */
core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>>& Upstart::appStopped()
{
    std::call_once(flag_appStopped, [this]() {
        auto reg = registry_.lock();

        reg->impl->thread.executeOnThread<bool>([this, reg]() {
            upstartEventData* data = new upstartEventData{reg};

            handle_appStopped = g_dbus_connection_signal_subscribe(
                reg->impl->_dbus.get(), /* bus */
                nullptr,                /* sender */
                DBUS_INTERFACE_UPSTART, /* interface */
                "EventEmitted",         /* signal */
                DBUS_PATH_UPSTART,      /* path */
                "stopped",              /* arg0 */
                G_DBUS_SIGNAL_FLAGS_NONE,
                [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar*, GVariant* params,
                   gpointer user_data) -> void {
                    auto data = reinterpret_cast<upstartEventData*>(user_data);
                    auto reg = data->weakReg.lock();
                    auto sparams = std::shared_ptr<GVariant>(g_variant_ref(params), g_variant_unref);
                    auto upstart = std::dynamic_pointer_cast<Upstart>(reg->impl->jobs);
                    upstart->upstartEventEmitted(upstart->sig_appStopped, sparams, reg);
                },    /* callback */
                data, /* user data */
                [](gpointer user_data) {
                    auto data = reinterpret_cast<upstartEventData*>(user_data);
                    delete data;
                }); /* user data destroy */

            return true;
        });
    });

    return sig_appStopped;
}

/** Grab the signal object for application failing. If we're not already listing for
    those signals this sets up a listener for them. */
core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>, Registry::FailureType>&
    Upstart::appFailed()
{
    std::call_once(flag_appFailed, [this]() {
        auto reg = registry_.lock();

        reg->impl->thread.executeOnThread<bool>([this, reg]() {
            upstartEventData* data = new upstartEventData{reg};

            handle_appFailed = g_dbus_connection_signal_subscribe(
                reg->impl->_dbus.get(),          /* bus */
                nullptr,                         /* sender */
                "com.canonical.UbuntuAppLaunch", /* interface */
                "ApplicationFailed",             /* signal */
                "/",                             /* path */
                nullptr,                         /* arg0 */
                G_DBUS_SIGNAL_FLAGS_NONE,
                [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar*, GVariant* params,
                   gpointer user_data) -> void {
                    auto data = reinterpret_cast<upstartEventData*>(user_data);
                    auto reg = data->weakReg.lock();

                    const gchar* sappid = NULL;
                    const gchar* typestr = NULL;

                    Registry::FailureType type = Registry::FailureType::CRASH;
                    g_variant_get(params, "(&s&s)", &sappid, &typestr);

                    if (g_strcmp0("crash", typestr) == 0)
                    {
                        type = Registry::FailureType::CRASH;
                    }
                    else if (g_strcmp0("start-failure", typestr) == 0)
                    {
                        type = Registry::FailureType::START_FAILURE;
                    }
                    else
                    {
                        g_warning("Application failure type '%s' unknown, reporting as a crash", typestr);
                    }

                    auto appid = AppID::find(reg, sappid);
                    auto app = Application::create(appid, reg);

                    /* TODO: Instance issues */

                    auto upstart = std::dynamic_pointer_cast<Upstart>(reg->impl->jobs);
                    upstart->sig_appFailed(app, {}, type);
                },    /* callback */
                data, /* user data */
                [](gpointer user_data) {
                    auto data = reinterpret_cast<upstartEventData*>(user_data);
                    delete data;
                }); /* user data destroy */

            return true;
        });
    });

    return sig_appFailed;
}

/** Core handler for pause and resume events. Includes turning the GVariant
    pid list into a std::vector and getting the application object. */
void Upstart::pauseEventEmitted(
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>, std::vector<pid_t>&>& signal,
    const std::shared_ptr<GVariant>& params,
    const std::shared_ptr<Registry>& reg)
{
    std::vector<pid_t> pids;
    GVariant* vappid = g_variant_get_child_value(params.get(), 0);
    GVariant* vpids = g_variant_get_child_value(params.get(), 1);
    guint64 pid;
    GVariantIter thispid;
    g_variant_iter_init(&thispid, vpids);

    while (g_variant_iter_loop(&thispid, "t", &pid))
    {
        pids.emplace_back(pid);
    }

    auto cappid = g_variant_get_string(vappid, NULL);
    auto appid = ubuntu::app_launch::AppID::find(reg, cappid);
    auto app = Application::create(appid, reg);

    /* TODO: Instance */
    signal(app, {}, pids);

    g_variant_unref(vappid);
    g_variant_unref(vpids);

    return;
}

/** Grab the signal object for application paused. If we're not already listing for
    those signals this sets up a listener for them. */
core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>, std::vector<pid_t>&>&
    Upstart::appPaused()
{
    std::call_once(flag_appPaused, [this]() {
        auto reg = registry_.lock();

        reg->impl->thread.executeOnThread<bool>([this, reg]() {
            upstartEventData* data = new upstartEventData{reg};

            handle_appPaused = g_dbus_connection_signal_subscribe(
                reg->impl->_dbus.get(),          /* bus */
                nullptr,                         /* sender */
                "com.canonical.UbuntuAppLaunch", /* interface */
                "ApplicationPaused",             /* signal */
                "/",                             /* path */
                nullptr,                         /* arg0 */
                G_DBUS_SIGNAL_FLAGS_NONE,
                [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar*, GVariant* params,
                   gpointer user_data) -> void {
                    auto data = reinterpret_cast<upstartEventData*>(user_data);
                    auto reg = data->weakReg.lock();
                    auto sparams = std::shared_ptr<GVariant>(g_variant_ref(params), g_variant_unref);
                    auto upstart = std::dynamic_pointer_cast<Upstart>(reg->impl->jobs);
                    upstart->pauseEventEmitted(upstart->sig_appPaused, sparams, reg);
                },    /* callback */
                data, /* user data */
                [](gpointer user_data) {
                    auto data = reinterpret_cast<upstartEventData*>(user_data);
                    delete data;
                }); /* user data destroy */

            return true;
        });
    });

    return sig_appPaused;
}

/** Grab the signal object for application resumed. If we're not already listing for
    those signals this sets up a listener for them. */
core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>, std::vector<pid_t>&>&
    Upstart::appResumed()
{
    std::call_once(flag_appResumed, [this]() {
        auto reg = registry_.lock();

        reg->impl->thread.executeOnThread<bool>([this, reg]() {
            upstartEventData* data = new upstartEventData{reg};

            handle_appResumed = g_dbus_connection_signal_subscribe(
                reg->impl->_dbus.get(),          /* bus */
                nullptr,                         /* sender */
                "com.canonical.UbuntuAppLaunch", /* interface */
                "ApplicationResumed",            /* signal */
                "/",                             /* path */
                nullptr,                         /* arg0 */
                G_DBUS_SIGNAL_FLAGS_NONE,
                [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar*, GVariant* params,
                   gpointer user_data) -> void {
                    auto data = reinterpret_cast<upstartEventData*>(user_data);
                    auto reg = data->weakReg.lock();
                    auto sparams = std::shared_ptr<GVariant>(g_variant_ref(params), g_variant_unref);
                    auto upstart = std::dynamic_pointer_cast<Upstart>(reg->impl->jobs);
                    upstart->pauseEventEmitted(upstart->sig_appResumed, sparams, reg);
                },    /* callback */
                data, /* user data */
                [](gpointer user_data) {
                    auto data = reinterpret_cast<upstartEventData*>(user_data);
                    delete data;
                }); /* user data destroy */

            return true;
        });
    });

    return sig_appResumed;
}

/** Initialize the CGManager connection, including a timeout to disconnect
    as CGManager doesn't free resources entirely well. So it's better if
    we connect and disconnect occationally */
void Upstart::initCGManager()
{
    if (cgManager_)
        return;

    std::promise<std::shared_ptr<GDBusConnection>> promise;
    auto future = promise.get_future();
    auto registry = registry_.lock();

    registry->impl->thread.executeOnThread([this, &promise, &registry]() {
        bool use_session_bus = g_getenv("UBUNTU_APP_LAUNCH_CG_MANAGER_SESSION_BUS") != nullptr;
        if (use_session_bus)
        {
            /* For working dbusmock */
            g_debug("Connecting to CG Manager on session bus");
            promise.set_value(registry->impl->_dbus);
            return;
        }

        auto cancel =
            std::shared_ptr<GCancellable>(g_cancellable_new(), [](GCancellable* cancel) { g_clear_object(&cancel); });

        /* Ensure that we do not wait for more than a second */
        registry->impl->thread.timeoutSeconds(std::chrono::seconds{1},
                                              [cancel]() { g_cancellable_cancel(cancel.get()); });

        g_dbus_connection_new_for_address(
            CGMANAGER_DBUS_PATH,                           /* cgmanager path */
            G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT, /* flags */
            nullptr,                                       /* Auth Observer */
            cancel.get(),                                  /* Cancellable */
            [](GObject* obj, GAsyncResult* res, gpointer data) -> void {
                GError* error = nullptr;
                auto promise = reinterpret_cast<std::promise<std::shared_ptr<GDBusConnection>>*>(data);

                auto gcon = g_dbus_connection_new_for_address_finish(res, &error);
                if (error != nullptr)
                {
                    g_error_free(error);
                }

                auto con = std::shared_ptr<GDBusConnection>(gcon, [](GDBusConnection* con) { g_clear_object(&con); });
                promise->set_value(con);
            },
            &promise);
    });

    cgManager_ = future.get();
    registry->impl->thread.timeoutSeconds(std::chrono::seconds{10}, [this]() { cgManager_.reset(); });
}

/** Get a list of PIDs from a CGroup, uses the CGManager connection to list
    all of the PIDs. It is important to note that this is an IPC call, so it can
    by its nature, be racy. Once the message has been sent the group can change.
    You should take that into account in your usage of it. */
std::vector<pid_t> Upstart::pidsFromCgroup(const std::string& jobpath)
{
    initCGManager();
    auto lmanager = cgManager_; /* Grab a local copy so we ensure it lasts through our lifetime */
    auto registry = registry_.lock();

    return registry->impl->thread.executeOnThread<std::vector<pid_t>>([&jobpath, lmanager]() -> std::vector<pid_t> {
        GError* error = nullptr;
        const gchar* name = g_getenv("UBUNTU_APP_LAUNCH_CG_MANAGER_NAME");
        std::string groupname;
        if (!jobpath.empty())
        {
            groupname = "upstart/" + jobpath;
        }

        g_debug("Looking for cg manager '%s' group '%s'", name, groupname.c_str());

        GVariant* vtpids = g_dbus_connection_call_sync(
            lmanager.get(),                     /* connection */
            name,                               /* bus name for direct connection is NULL */
            "/org/linuxcontainers/cgmanager",   /* object */
            "org.linuxcontainers.cgmanager0_0", /* interface */
            "GetTasksRecursive",                /* method */
            g_variant_new("(ss)", "freezer", groupname.empty() ? "" : groupname.c_str()), /* params */
            G_VARIANT_TYPE("(ai)"),                                                       /* output */
            G_DBUS_CALL_FLAGS_NONE,                                                       /* flags */
            -1,                                                                           /* default timeout */
            nullptr,                                                                      /* cancellable */
            &error);                                                                      /* error */

        if (error != nullptr)
        {
            g_warning("Unable to get PID list from cgroup manager: %s", error->message);
            g_error_free(error);
            return {};
        }

        GVariant* vpids = g_variant_get_child_value(vtpids, 0);
        GVariantIter iter;
        g_variant_iter_init(&iter, vpids);
        gint32 pid;
        std::vector<pid_t> pids;

        while (g_variant_iter_loop(&iter, "i", &pid))
        {
            pids.push_back(pid);
        }

        g_variant_unref(vpids);
        g_variant_unref(vtpids);

        return pids;
    });
}

/** Looks to find the Upstart object path for a specific Upstart job. This first
    checks the cache, and otherwise does the lookup on DBus. */
std::string Upstart::upstartJobPath(const std::string& job)
{
    try
    {
        return upstartJobPathCache_.at(job);
    }
    catch (std::out_of_range& e)
    {
        auto registry = registry_.lock();
        auto path = registry->impl->thread.executeOnThread<std::string>([this, &job, &registry]() -> std::string {
            GError* error = nullptr;
            GVariant* job_path_variant =
                g_dbus_connection_call_sync(registry->impl->_dbus.get(),                   /* connection */
                                            DBUS_SERVICE_UPSTART,                          /* service */
                                            DBUS_PATH_UPSTART,                             /* path */
                                            DBUS_INTERFACE_UPSTART,                        /* iface */
                                            "GetJobByName",                                /* method */
                                            g_variant_new("(s)", job.c_str()),             /* params */
                                            G_VARIANT_TYPE("(o)"),                         /* return */
                                            G_DBUS_CALL_FLAGS_NONE,                        /* flags */
                                            -1,                                            /* timeout: default */
                                            registry->impl->thread.getCancellable().get(), /* cancellable */
                                            &error);                                       /* error */

            if (error != nullptr)
            {
                g_warning("Unable to find job '%s': %s", job.c_str(), error->message);
                g_error_free(error);
                return {};
            }

            gchar* job_path = nullptr;
            g_variant_get(job_path_variant, "(o)", &job_path);
            g_variant_unref(job_path_variant);

            if (job_path != nullptr)
            {
                std::string path(job_path);
                g_free(job_path);
                return path;
            }
            else
            {
                return {};
            }
        });

        upstartJobPathCache_[job] = path;
        return path;
    }
}

/** Queries Upstart to get all the instances of a given job. This
    can take a while as the number of dbus calls is n+1. It is
    rare that apps have many instances though. */
std::list<std::string> Upstart::upstartInstancesForJob(const std::string& job)
{
    std::string jobpath = upstartJobPath(job);
    if (jobpath.empty())
    {
        return {};
    }

    auto registry = registry_.lock();
    return registry->impl->thread.executeOnThread<std::list<std::string>>(
        [this, &job, &jobpath, &registry]() -> std::list<std::string> {
            GError* error = nullptr;
            GVariant* instance_tuple =
                g_dbus_connection_call_sync(registry->impl->_dbus.get(),                   /* connection */
                                            DBUS_SERVICE_UPSTART,                          /* service */
                                            jobpath.c_str(),                               /* object path */
                                            DBUS_INTERFACE_UPSTART_JOB,                    /* iface */
                                            "GetAllInstances",                             /* method */
                                            nullptr,                                       /* params */
                                            G_VARIANT_TYPE("(ao)"),                        /* return type */
                                            G_DBUS_CALL_FLAGS_NONE,                        /* flags */
                                            -1,                                            /* timeout: default */
                                            registry->impl->thread.getCancellable().get(), /* cancellable */
                                            &error);

            if (error != nullptr)
            {
                g_warning("Unable to get instances of job '%s': %s", job.c_str(), error->message);
                g_error_free(error);
                return {};
            }

            if (instance_tuple == nullptr)
            {
                return {};
            }

            GVariant* instance_list = g_variant_get_child_value(instance_tuple, 0);
            g_variant_unref(instance_tuple);

            GVariantIter instance_iter;
            g_variant_iter_init(&instance_iter, instance_list);
            const gchar* instance_path = nullptr;
            std::list<std::string> instances;

            while (g_variant_iter_loop(&instance_iter, "&o", &instance_path))
            {
                GVariant* props_tuple =
                    g_dbus_connection_call_sync(registry->impl->_dbus.get(),                           /* connection */
                                                DBUS_SERVICE_UPSTART,                                  /* service */
                                                instance_path,                                         /* object path */
                                                "org.freedesktop.DBus.Properties",                     /* interface */
                                                "GetAll",                                              /* method */
                                                g_variant_new("(s)", DBUS_INTERFACE_UPSTART_INSTANCE), /* params */
                                                G_VARIANT_TYPE("(a{sv})"),                             /* return type */
                                                G_DBUS_CALL_FLAGS_NONE,                                /* flags */
                                                -1,                                            /* timeout: default */
                                                registry->impl->thread.getCancellable().get(), /* cancellable */
                                                &error);

                if (error != nullptr)
                {
                    g_warning("Unable to name of instance '%s': %s", instance_path, error->message);
                    g_error_free(error);
                    error = nullptr;
                    continue;
                }

                GVariant* props_dict = g_variant_get_child_value(props_tuple, 0);

                GVariant* namev = g_variant_lookup_value(props_dict, "name", G_VARIANT_TYPE_STRING);
                if (namev != nullptr)
                {
                    auto name = g_variant_get_string(namev, NULL);
                    g_debug("Adding instance for job '%s': %s", job.c_str(), name);
                    instances.push_back(name);
                    g_variant_unref(namev);
                }

                g_variant_unref(props_dict);
                g_variant_unref(props_tuple);
            }

            g_variant_unref(instance_list);

            return instances;
        });
}

std::list<std::shared_ptr<Application>> Upstart::runningApps()
{
    std::list<std::string> instances;

    /* Get all the legacy instances */
    instances.splice(instances.begin(), upstartInstancesForJob("application-legacy"));
    /* Get all the snap instances */
    instances.splice(instances.begin(), upstartInstancesForJob("application-snap"));

    /* Remove the instance ID */
    std::transform(instances.begin(), instances.end(), instances.begin(), [](std::string& instancename) -> std::string {
        static const std::regex instanceregex("^(.*)-[0-9]*$");
        std::smatch match;
        if (std::regex_match(instancename, match, instanceregex))
        {
            return match[1].str();
        }
        else
        {
            g_warning("Unable to match instance name: %s", instancename.c_str());
            return {};
        }
    });

    /* Deduplicate Set */
    std::set<std::string> instanceset;
    for (auto instance : instances)
    {
        if (!instance.empty())
            instanceset.insert(instance);
    }

    /* Add in the click instances */
    for (auto instance : upstartInstancesForJob("application-click"))
    {
        instanceset.insert(instance);
    }

    g_debug("Overall there are %d instances: %s", int(instanceset.size()),
            std::accumulate(instanceset.begin(), instanceset.end(), std::string{},
                            [](const std::string& instr, std::string instance) {
                                return instr.empty() ? instance : instr + ", " + instance;
                            })
                .c_str());

    /* Convert to Applications */
    auto registry = registry_.lock();
    std::list<std::shared_ptr<Application>> apps;
    for (auto instance : instanceset)
    {
        auto appid = AppID::find(registry, instance);
        auto app = Application::create(appid, registry);
        apps.push_back(app);
    }

    return apps;
}

}  // namespace manager
}  // namespace jobs
}  // namespace app_launch
}  // namespace ubuntu
