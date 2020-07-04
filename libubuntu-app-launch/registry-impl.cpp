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

#include "registry-impl.h"
#include "application-icon-finder.h"
#include <cgmanager/cgmanager.h>
#include <upstart.h>

namespace ubuntu
{
namespace app_launch
{

Registry::Impl::Impl(Registry* registry)
    : thread([]() {},
             [this]() {
                 _clickUser.reset();
                 _clickDB.reset();

                 zgLog_.reset();
                 cgManager_.reset();

                 if (_dbus)
                     g_dbus_connection_flush_sync(_dbus.get(), nullptr, nullptr);
                 _dbus.reset();
             })
    , _registry(registry)
    , _iconFinders()
// _manager(nullptr)
{
    auto session_cancel = thread.getCancellable();
    auto system_cancel = thread.getCancellable();
    _dbus = thread.executeOnThread<std::shared_ptr<GDBusConnection>>([session_cancel]() {
        return std::shared_ptr<GDBusConnection>(g_bus_get_sync(G_BUS_TYPE_SESSION, session_cancel.get(), nullptr),
                                                [](GDBusConnection* bus) { g_clear_object(&bus); });
    });
    _dbus_system = thread.executeOnThread<std::shared_ptr<GDBusConnection>>([system_cancel]() {
        return std::shared_ptr<GDBusConnection>(g_bus_get_sync(G_BUS_TYPE_SYSTEM, system_cancel.get(), nullptr),
                                                [](GDBusConnection* bus) { g_clear_object(&bus); });
    });
}

void Registry::Impl::initClick()
{
    if (_clickDB && _clickUser)
    {
        return;
    }

    auto init = thread.executeOnThread<bool>([this]() {
        GError* error = nullptr;

        if (!_clickDB)
        {
            _clickDB = std::shared_ptr<ClickDB>(click_db_new(), [](ClickDB* db) { g_clear_object(&db); });
            /* If TEST_CLICK_DB is unset, this reads the system database. */
            click_db_read(_clickDB.get(), g_getenv("TEST_CLICK_DB"), &error);

            if (error != nullptr)
            {
                auto perror = std::shared_ptr<GError>(error, [](GError* error) { g_error_free(error); });
                throw std::runtime_error(perror->message);
            }
        }

        if (!_clickUser)
        {
            _clickUser =
                std::shared_ptr<ClickUser>(click_user_new_for_user(_clickDB.get(), g_getenv("TEST_CLICK_USER"), &error),
                                           [](ClickUser* user) { g_clear_object(&user); });

            if (error != nullptr)
            {
                auto perror = std::shared_ptr<GError>(error, [](GError* error) { g_error_free(error); });
                throw std::runtime_error(perror->message);
            }
        }

        g_debug("Initialized Click DB");
        return true;
    });

    if (!init)
    {
        throw std::runtime_error("Unable to initialize the Click Database");
    }
}

#if JSON_CHECK_VERSION(1, 1, 2)
/** Helper function for printing JSON objects to debug output */
std::string Registry::Impl::printJson(std::shared_ptr<JsonObject> jsonobj)
{
    auto node = json_node_alloc();
    json_node_init_object(node, jsonobj.get());

    auto snode = std::shared_ptr<JsonNode>(node, json_node_unref);
    return printJson(snode);
}

/** Helper function for printing JSON nodes to debug output */
std::string Registry::Impl::printJson(std::shared_ptr<JsonNode> jsonnode)
{
    std::string retval;
    auto gstr = json_to_string(jsonnode.get(), TRUE);

    if (gstr != nullptr)
    {
        retval = gstr;
        g_free(gstr);
    }

    return retval;
}
#else
std::string Registry::Impl::printJson(std::shared_ptr<JsonObject> jsonobj)
{
    return "Your JSON-GLib is too old to print the JSON here, please talk to your vendor about upgrading";
}

std::string Registry::Impl::printJson(std::shared_ptr<JsonNode> jsonnode)
{
    return "Your JSON-GLib is too old to print the JSON here, please talk to your vendor about upgrading";
}
#endif

std::shared_ptr<JsonObject> Registry::Impl::getClickManifest(const std::string& package)
{
    initClick();

    auto retval = thread.executeOnThread<std::shared_ptr<JsonObject>>([this, package]() {
        GError* error = nullptr;
        auto mani = click_user_get_manifest(_clickUser.get(), package.c_str(), &error);

        if (error != nullptr)
        {
            auto perror = std::shared_ptr<GError>(error, [](GError* error) { g_error_free(error); });
            g_critical("Error parsing manifest for package '%s': %s", package.c_str(), perror->message);
            return std::shared_ptr<JsonObject>();
        }

        auto node = json_node_alloc();
        json_node_init_object(node, mani);

        auto retval = std::shared_ptr<JsonObject>(json_node_dup_object(node), json_object_unref);

        json_node_free(node);

        return retval;
    });

    if (!retval)
        throw std::runtime_error("Unable to get Click manifest for package: " + package);

    return retval;
}

std::list<AppID::Package> Registry::Impl::getClickPackages()
{
    initClick();

    return thread.executeOnThread<std::list<AppID::Package>>([this]() {
        GError* error = nullptr;
        GList* pkgs = click_user_get_package_names(_clickUser.get(), &error);

        if (error != nullptr)
        {
            auto perror = std::shared_ptr<GError>(error, [](GError* error) { g_error_free(error); });
            throw std::runtime_error(perror->message);
        }

        std::list<AppID::Package> list;
        for (GList* item = pkgs; item != NULL; item = g_list_next(item))
        {
            auto pkgobj = reinterpret_cast<gchar*>(item->data);
            if (pkgobj)
            {
                list.emplace_back(AppID::Package::from_raw(pkgobj));
            }
        }

        g_list_free_full(pkgs, g_free);
        return list;
    });
}

std::string Registry::Impl::getClickDir(const std::string& package)
{
    initClick();

    return thread.executeOnThread<std::string>([this, package]() {
        GError* error = nullptr;
        auto dir = click_user_get_path(_clickUser.get(), package.c_str(), &error);

        if (error != nullptr)
        {
            auto perror = std::shared_ptr<GError>(error, [](GError* error) { g_error_free(error); });
            throw std::runtime_error(perror->message);
        }

        std::string cppdir(dir);
        g_free(dir);
        return cppdir;
    });
}

/** Initialize the CGManager connection, including a timeout to disconnect
    as CGManager doesn't free resources entirely well. So it's better if
    we connect and disconnect occationally */
void Registry::Impl::initCGManager()
{
    if (cgManager_)
        return;

    cgManager_ = thread.executeOnThread<std::shared_ptr<GDBusConnection>>([this]() {
        bool use_session_bus = g_getenv("UBUNTU_APP_LAUNCH_CG_MANAGER_SESSION_BUS") != nullptr;
        if (use_session_bus)
        {
            /* For working dbusmock */
            g_debug("Connecting to CG Manager on session bus");
            return _dbus;
        }

        auto cancel =
            std::shared_ptr<GCancellable>(g_cancellable_new(), [](GCancellable* cancel) { g_clear_object(&cancel); });

        /* Ensure that we do not wait for more than a second */
        thread.timeoutSeconds(std::chrono::seconds{1}, [cancel]() { g_cancellable_cancel(cancel.get()); });

        GError* error = nullptr;
        auto retval = std::shared_ptr<GDBusConnection>(
            g_dbus_connection_new_for_address_sync(CGMANAGER_DBUS_PATH,                           /* cgmanager path */
                                                   G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT, /* flags */
                                                   nullptr,                                       /* Auth Observer */
                                                   cancel.get(),                                  /* Cancellable */
                                                   &error),
            [](GDBusConnection* con) { g_clear_object(&con); });

        if (error != nullptr)
        {
            g_warning("Unable to get CGManager connection: %s", error->message);
            g_error_free(error);
        }

        return retval;
    });

    /* NOTE: This will execute on the thread */
    thread.timeoutSeconds(std::chrono::seconds{10}, [this]() { cgManager_.reset(); });
}

/** Get a list of PIDs from a CGroup, uses the CGManager connection to list
    all of the PIDs. It is important to note that this is an IPC call, so it can
    by its nature, be racy. Once the message has been sent the group can change.
    You should take that into account in your usage of it. */
std::vector<pid_t> Registry::Impl::pidsFromCgroup(const std::string& jobpath)
{
    initCGManager();
    auto lmanager = cgManager_; /* Grab a local copy so we ensure it lasts through our lifetime */

    return thread.executeOnThread<std::vector<pid_t>>([&jobpath, lmanager]() -> std::vector<pid_t> {
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
std::string Registry::Impl::upstartJobPath(const std::string& job)
{
    try
    {
        return upstartJobPathCache_.at(job);
    }
    catch (std::out_of_range& e)
    {
        auto path = thread.executeOnThread<std::string>([this, &job]() -> std::string {
            GError* error = nullptr;
            GVariant* job_path_variant = g_dbus_connection_call_sync(_dbus.get(),                       /* connection */
                                                                     DBUS_SERVICE_UPSTART,              /* service */
                                                                     DBUS_PATH_UPSTART,                 /* path */
                                                                     DBUS_INTERFACE_UPSTART,            /* iface */
                                                                     "GetJobByName",                    /* method */
                                                                     g_variant_new("(s)", job.c_str()), /* params */
                                                                     G_VARIANT_TYPE("(o)"),             /* return */
                                                                     G_DBUS_CALL_FLAGS_NONE,            /* flags */
                                                                     -1, /* timeout: default */
                                                                     thread.getCancellable().get(), /* cancellable */
                                                                     &error);                       /* error */

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
std::list<std::string> Registry::Impl::upstartInstancesForJob(const std::string& job)
{
    std::string jobpath = upstartJobPath(job);
    if (jobpath.empty())
    {
        return {};
    }

    return thread.executeOnThread<std::list<std::string>>([this, &job, &jobpath]() -> std::list<std::string> {
        GError* error = nullptr;
        GVariant* instance_tuple = g_dbus_connection_call_sync(_dbus.get(),                   /* connection */
                                                               DBUS_SERVICE_UPSTART,          /* service */
                                                               jobpath.c_str(),               /* object path */
                                                               DBUS_INTERFACE_UPSTART_JOB,    /* iface */
                                                               "GetAllInstances",             /* method */
                                                               nullptr,                       /* params */
                                                               G_VARIANT_TYPE("(ao)"),        /* return type */
                                                               G_DBUS_CALL_FLAGS_NONE,        /* flags */
                                                               -1,                            /* timeout: default */
                                                               thread.getCancellable().get(), /* cancellable */
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
                g_dbus_connection_call_sync(_dbus.get(),                                           /* connection */
                                            DBUS_SERVICE_UPSTART,                                  /* service */
                                            instance_path,                                         /* object path */
                                            "org.freedesktop.DBus.Properties",                     /* interface */
                                            "GetAll",                                              /* method */
                                            g_variant_new("(s)", DBUS_INTERFACE_UPSTART_INSTANCE), /* params */
                                            G_VARIANT_TYPE("(a{sv})"),                             /* return type */
                                            G_DBUS_CALL_FLAGS_NONE,                                /* flags */
                                            -1,                            /* timeout: default */
                                            thread.getCancellable().get(), /* cancellable */
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

/** Send an event to Zietgeist using the registry thread so that
        the callback comes back in the right place. */
void Registry::Impl::zgSendEvent(AppID appid, const std::string& eventtype)
{
    thread.executeOnThread([this, appid, eventtype] {
        std::string uri;

        if (appid.package.value().empty())
        {
            uri = "application://" + appid.appname.value() + ".desktop";
        }
        else
        {
            uri = "application://" + appid.package.value() + "_" + appid.appname.value() + ".desktop";
        }

        g_debug("Sending ZG event for '%s': %s", uri.c_str(), eventtype.c_str());

        if (!zgLog_)
        {
            zgLog_ =
                std::shared_ptr<ZeitgeistLog>(zeitgeist_log_new(), /* create a new log for us */
                                              [](ZeitgeistLog* log) { g_clear_object(&log); }); /* Free as a GObject */
        }

        ZeitgeistEvent* event = zeitgeist_event_new();
        zeitgeist_event_set_actor(event, "application://ubuntu-app-launch.desktop");
        zeitgeist_event_set_interpretation(event, eventtype.c_str());
        zeitgeist_event_set_manifestation(event, ZEITGEIST_ZG_USER_ACTIVITY);

        ZeitgeistSubject* subject = zeitgeist_subject_new();
        zeitgeist_subject_set_interpretation(subject, ZEITGEIST_NFO_SOFTWARE);
        zeitgeist_subject_set_manifestation(subject, ZEITGEIST_NFO_SOFTWARE_ITEM);
        zeitgeist_subject_set_mimetype(subject, "application/x-desktop");
        zeitgeist_subject_set_uri(subject, uri.c_str());

        zeitgeist_event_add_subject(event, subject);

        zeitgeist_log_insert_event(zgLog_.get(), /* log */
                                   event,        /* event */
                                   nullptr,      /* cancellable */
                                   [](GObject* obj, GAsyncResult* res, gpointer user_data) -> void {
                                       GError* error = nullptr;
                                       GArray* result = nullptr;

                                       result = zeitgeist_log_insert_event_finish(ZEITGEIST_LOG(obj), res, &error);

                                       if (error != nullptr)
                                       {
                                           g_warning("Unable to submit Zeitgeist Event: %s", error->message);
                                           g_error_free(error);
                                       }

                                       g_array_free(result, TRUE);
                                   },        /* callback */
                                   nullptr); /* userdata */

        g_object_unref(event);
        g_object_unref(subject);
    });
}

std::shared_ptr<IconFinder> Registry::Impl::getIconFinder(std::string basePath)
{
    if (_iconFinders.find(basePath) == _iconFinders.end())
    {
        _iconFinders[basePath] = std::make_shared<IconFinder>(basePath);
    }
    return _iconFinders[basePath];
}

#if 0
void
Registry::Impl::setManager (Registry::Manager* manager)
{
    if (_manager != nullptr)
    {
        throw std::runtime_error("Already have a manager and trying to set another");
    }

    _manager = manager;
}

void
Registry::Impl::clearManager ()
{
    _manager = nullptr;
}
#endif

/** App start watching, if we're registered for the signal we
    can't wait on it. We are making this static right now because
    we need it to go across the C and C++ APIs smoothly, and those
    can end up with different registry objects. Long term, this
    should become a member variable. */
static bool watchingAppStarting_ = false;

/** Variable to track if this program is watching app startup
    so that we can know to not wait on the response to that. */
void Registry::Impl::watchingAppStarting(bool rWatching)
{
    watchingAppStarting_ = rWatching;
}

/** Accessor for the internal variable to know whether an app
    is watching for app startup */
bool Registry::Impl::isWatchingAppStarting()
{
    return watchingAppStarting_;
}

}  // namespace app_launch
}  // namespace ubuntu
