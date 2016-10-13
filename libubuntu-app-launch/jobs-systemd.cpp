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

#include <gio/gio.h>
#include <regex>
#include <sys/types.h>
#include <unistd.h>

#include "jobs-systemd.h"
#include "registry-impl.h"

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
    userbus_ = registry->impl->thread.executeOnThread<std::shared_ptr<GDBusConnection>>([cancel]() {
        GError* error = nullptr;
        auto bus = std::shared_ptr<GDBusConnection>(
            g_dbus_connection_new_for_address_sync(
                ("unix:path=" + userBusPath()).c_str(),         /* path to the user bus */
                G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION, /* It is a message bus */
                nullptr,                                        /* observer */
                cancel.get(),                                   /* cancellable from the thread */
                &error),                                        /* error */
            [](GDBusConnection* bus) { g_clear_object(&bus); });

        if (error != nullptr)
        {
            std::string message = std::string("Unable to connect to user bus: ") + error->message;
            g_error_free(error);
            throw std::runtime_error(message);
        }

        return bus;
    });
}

SystemD::~SystemD()
{
}

std::shared_ptr<Application::Instance> SystemD::launch(
    const AppID& appId,
    const std::string& job,
    const std::string& instance,
    const std::vector<Application::URL>& urls,
    launchMode mode,
    std::function<std::list<std::pair<std::string, std::string>>(void)>& getenv)
{
    return {};
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
    auto registry = registry_.lock();
    std::vector<Application::URL> urls;

    for (const auto& unit : listUnits())
    {
        SystemD::UnitInfo unitinfo;

        try
        {
            unitinfo = parseUnit(unit.id);
        }
        catch (std::runtime_error& e)
        {
            continue;
        }

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

    return instances;
}

std::list<std::shared_ptr<Application>> SystemD::runningApps()
{
    auto allJobs = getAllJobs();
    auto registry = registry_.lock();
    std::set<std::string> appids;

    for (const auto& unit : listUnits())
    {
        SystemD::UnitInfo unitinfo;

        try
        {
            unitinfo = parseUnit(unit.id);
        }
        catch (std::runtime_error& e)
        {
            continue;
        }

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
    return std::string{"/run/user/"} + std::to_string(getuid()) + std::string{"/bus"};
}

std::list<SystemD::UnitEntry> SystemD::listUnits()
{
    auto registry = registry_.lock();
    return registry->impl->thread.executeOnThread<std::list<SystemD::UnitEntry>>([this, registry]() {
        GError* error{nullptr};
        std::list<SystemD::UnitEntry> ret;

        GVariant* call = g_dbus_connection_call_sync(userbus_.get(),                                /* user bus */
                                                     SYSTEMD_DBUS_ADDRESS.c_str(),                  /* bus name */
                                                     SYSTEMD_DBUS_PATH_MANAGER.c_str(),             /* path */
                                                     SYSTEMD_DBUS_IFACE_MANAGER.c_str(),            /* interface */
                                                     "ListUnits",                                   /* method */
                                                     nullptr,                                       /* params */
                                                     G_VARIANT_TYPE("a(ssssssouso)"),               /* ret type */
                                                     G_DBUS_CALL_FLAGS_NONE,                        /* flags */
                                                     -1,                                            /* timeout */
                                                     registry->impl->thread.getCancellable().get(), /* cancellable */
                                                     &error);

        if (error != nullptr)
        {
            auto message = std::string{"Unable to list SystemD units: "} + error->message;
            g_error_free(error);
            throw std::runtime_error(message);
        }

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
            ret.emplace_back(SystemD::UnitEntry{id, description, loadState, activeState, subState, following, path,
                                                jobId, jobType, jobPath});
        }

        g_variant_iter_free(iter);
        g_variant_unref(call);

        return ret;
    });
}

/* TODO: Application job names */
const std::regex unitNaming{"^ubuntu\\-app\\-launch\\-(application\\-.*)\\-(.*)\\-([0-9]*)$"};

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
    return std::string{"ubuntu-app-launch-"} + info.job + "-" + info.appid + "-" + info.inst;
}

/** Function that uses and maintains the cache of the paths for units
    on the systemd dbus connection. If we already have the entry in the
    cache we just return the path and this function is fast. If not we have
    to ask systemd for it and that can take a bit longer.

    After getting the data we throw a small background task in to clean
    up the cache if it has more than 50 entries. We delete those who
    haven't be used for an hour.
*/
std::string SystemD::unitPath(const std::string& unitName)
{
    auto registry = registry_.lock();
    std::string retval;

    if (true)
    {
        /* Create a context for the gaurd */
        std::lock_guard<std::mutex> guard(unitPathsMutex_);
        auto iter = std::find_if(unitPaths_.begin(), unitPaths_.end(),
                                 [&unitName](const SystemD::UnitPath& entry) { return entry.unitName == unitName; });

        if (iter != unitPaths_.end())
        {
            retval = iter->unitPath;
            iter->timeStamp = std::chrono::system_clock::now();
        }
    }

    if (retval.empty())
    {
        retval = registry->impl->thread.executeOnThread<std::string>([this, registry, unitName]() {
            std::string path;
            GError* error{nullptr};
            GVariant* call =
                g_dbus_connection_call_sync(userbus_.get(),                                /* user bus */
                                            SYSTEMD_DBUS_ADDRESS.c_str(),                  /* bus name */
                                            SYSTEMD_DBUS_PATH_MANAGER.c_str(),             /* path */
                                            SYSTEMD_DBUS_IFACE_MANAGER.c_str(),            /* interface */
                                            "GetUnit",                                     /* method */
                                            g_variant_new("(s)", unitName.c_str()),        /* params */
                                            G_VARIANT_TYPE("(o)"),                         /* ret type */
                                            G_DBUS_CALL_FLAGS_NONE,                        /* flags */
                                            -1,                                            /* timeout */
                                            registry->impl->thread.getCancellable().get(), /* cancellable */
                                            &error);

            if (error != nullptr)
            {
                auto message = std::string{"Unable to get SystemD unit path for '"} + unitName + std::string{"': "} +
                               error->message;
                g_error_free(error);
                throw std::runtime_error(message);
            }

            /* Parse variant */
            gchar* gpath = nullptr;
            g_variant_get(call, "(o)", &gpath);
            if (gpath != nullptr)
            {
                std::lock_guard<std::mutex> guard(unitPathsMutex_);
                path = gpath;
                unitPaths_.emplace_back(SystemD::UnitPath{unitName, path, std::chrono::system_clock::now()});
            }

            g_variant_unref(call);

            return path;
        });
    }

    /* Queue a possible cleanup */
    if (unitPaths_.size() > 50)
    {
        /* TODO: We should look at UnitRemoved as well */
        /* TODO: Add to cache on UnitNew */
        registry->impl->thread.executeOnThread([this] {
            std::lock_guard<std::mutex> guard(unitPathsMutex_);
            std::remove_if(unitPaths_.begin(), unitPaths_.end(), [](const SystemD::UnitPath& entry) -> bool {
                auto age = std::chrono::system_clock::now() - entry.timeStamp;
                return age > std::chrono::hours{1};
            });
        });
    }

    return retval;
}

pid_t SystemD::unitPrimaryPid(const AppID& appId, const std::string& job, const std::string& instance)
{
    auto registry = registry_.lock();
    auto unitname = unitName(SystemD::UnitInfo{appId, job, instance});
    auto unitpath = unitPath(unitname);

    return registry->impl->thread.executeOnThread<pid_t>([this, registry, unitname, unitpath]() {
        pid_t pid;
        GError* error{nullptr};
        GVariant* call = g_dbus_connection_call_sync(
            userbus_.get(),                                                       /* user bus */
            SYSTEMD_DBUS_ADDRESS.c_str(),                                         /* bus name */
            unitpath.c_str(),                                                     /* path */
            "org.freedesktop.DBus.Properties",                                    /* interface */
            "Get",                                                                /* method */
            g_variant_new("(ss)", SYSTEMD_DBUS_IFACE_SERVICE.c_str(), "MainPID"), /* params */
            G_VARIANT_TYPE("(<u>)"),                                              /* ret type */
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
        g_variant_get(call, "(<u>)", &pid);

        g_variant_unref(call);

        return pid;
    });
}

std::vector<pid_t> SystemD::unitPids(const AppID& appId, const std::string& job, const std::string& instance)
{
    auto unitname = unitName(SystemD::UnitInfo{appId, job, instance});
    return {};
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

}  // namespace manager
}  // namespace jobs
}  // namespace app_launch
}  // namespace ubuntu
