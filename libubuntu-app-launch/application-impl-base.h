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
}

#pragma once

namespace ubuntu
{
namespace app_launch
{
namespace app_impls
{

class Base : public ubuntu::app_launch::Application
{
public:
    Base(const std::shared_ptr<Registry>& registry);

    bool hasInstances() override;

protected:
    std::shared_ptr<Registry> _registry;
};

class UpstartInstance : public Application::Instance
{
public:
    explicit UpstartInstance(const AppID& appId,
                             const std::string& job,
                             const std::string& instance,
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

    /* Creating by launch */
    enum class launchMode
    {
        STANDARD,
        TEST
    };
    static std::shared_ptr<UpstartInstance> launch(const AppID& appId,
                                                   const std::string& job,
                                                   const std::string& instance,
                                                   const std::vector<Application::URL>& urls,
                                                   const std::shared_ptr<Registry>& registry,
                                                   launchMode mode);

private:
    const AppID appId_;
    const std::string job_;
    const std::string instance_;
    std::shared_ptr<Registry> registry_;

    std::vector<pid_t> forAllPids(std::function<void(pid_t)> eachPid);
    void signalToPid(pid_t pid, int signal);
    std::string pidToOomPath(pid_t pid);
    void oomValueToPid(pid_t pid, const oom::Score oomvalue);
    void oomValueToPidHelper(pid_t pid, const oom::Score oomvalue);
    void pidListToDbus(const std::vector<pid_t>& pids, const std::string& signal);
};

}  // namespace app_impls
}  // namespace app_launch
}  // namespace ubuntu
