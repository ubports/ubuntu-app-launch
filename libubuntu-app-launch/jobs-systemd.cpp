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
    return {};
}

std::string SystemD::logPath()
{
    return {};
}

std::vector<pid_t> SystemD::pids()
{
    return {};
}

void SystemD::stop()
{
}

}  // namespace instance

namespace manager
{

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
    return {};
}

std::vector<std::shared_ptr<instance::Base>> SystemD::instances(const AppID& appID, const std::string& job)
{
    return {};
}

std::list<std::shared_ptr<Application>> SystemD::runningApps()
{
    return {};
}

std::string SystemD::userBusPath()
{
    return std::string{"/run/user/"} + std::to_string(getuid()) + std::string{"/bus"};
}

}  // namespace manager
}  // namespace jobs
}  // namespace app_launch
}  // namespace ubuntu
