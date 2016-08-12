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

#include "application.h"

extern "C" {
#include "ubuntu-app-launch.h"
#include <gio/gio.h>
}

#pragma once

namespace ubuntu
{
namespace app_launch
{
namespace app_impls
{

/** Provides some helper functions that can be used by all
    implementations of application. Stores the registry pointer
    which everyone wants anyway. */
class Base : public ubuntu::app_launch::Application
{
public:
    Base(const std::shared_ptr<Registry>& registry);

    bool hasInstances() override;

protected:
    /** Pointer to the registry so we can ask it for things */
    std::shared_ptr<Registry> _registry;

    static std::list<std::pair<std::string, std::string>> confinedEnv(const std::string& package,
                                                                      const std::string& pkgdir);
};

/** An object that represents an instance of a job on Upstart. This
    then implements everything needed by the instance interface. Most
    applications tie into this today and use it as the backend for
    their instances. */
class UpstartInstance : public Application::Instance
{
public:
    explicit UpstartInstance(const AppID& appId,
                             const std::string& job,
                             const std::string& instance,
                             const std::vector<Application::URL>& urls,
                             const std::shared_ptr<Registry>& registry);

    /* Query lifecycle */
    bool isRunning() override;
    pid_t primaryPid() override;
    bool hasPid(pid_t pid) override;
    std::string logPath() override;
    std::vector<pid_t> pids() override;

    /* Manage lifecycle */
    void pause() override;
    void resume() override;
    void stop() override;

    /* OOM Functions */
    void setOomAdjustment(const oom::Score score) override;
    const oom::Score getOomAdjustment() override;

    /** Flag for whether we should include the testing environment variables */
    enum class launchMode
    {
        STANDARD, /**< Standard variable set */
        TEST      /**< Include testing environment vars */
    };
    static std::shared_ptr<UpstartInstance> launch(
        const AppID& appId,
        const std::string& job,
        const std::string& instance,
        const std::vector<Application::URL>& urls,
        const std::shared_ptr<Registry>& registry,
        launchMode mode,
        std::function<std::list<std::pair<std::string, std::string>>(void)> getenv);

private:
    /** Application ID */
    const AppID appId_;
    /** Upstart job name */
    const std::string job_;
    /** Instance ID environment value, empty if none */
    const std::string instance_;
    /** The URLs that this was launched for. Only valid on launched jobs, we
        should look at perhaps changing that. */
    std::vector<Application::URL> urls_;
    /** A link to the registry we're using for connections */
    std::shared_ptr<Registry> registry_;

    std::vector<pid_t> forAllPids(std::function<void(pid_t)> eachPid);
    void signalToPid(pid_t pid, int signal);
    std::string pidToOomPath(pid_t pid);
    void oomValueToPid(pid_t pid, const oom::Score oomvalue);
    void oomValueToPidHelper(pid_t pid, const oom::Score oomvalue);
    void pidListToDbus(const std::vector<pid_t>& pids, const std::string& signal);

    static std::shared_ptr<gchar*> urlsToStrv(const std::vector<Application::URL>& urls);
    static void application_start_cb(GObject* obj, GAsyncResult* res, gpointer user_data);
};

}  // namespace app_impls
}  // namespace app_launch
}  // namespace ubuntu
