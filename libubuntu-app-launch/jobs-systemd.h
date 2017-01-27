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

#pragma once

#include "jobs-base.h"
#include <chrono>
#include <future>
#include <gio/gio.h>
#include <map>
#include <mutex>

namespace ubuntu
{
namespace app_launch
{
namespace jobs
{
namespace manager
{

class SystemD : public Base
{
public:
    SystemD(std::shared_ptr<Registry> registry);
    virtual ~SystemD();

    virtual std::shared_ptr<Application::Instance> launch(
        const AppID& appId,
        const std::string& job,
        const std::string& instance,
        const std::vector<Application::URL>& urls,
        launchMode mode,
        std::function<std::list<std::pair<std::string, std::string>>(void)>& getenv) override;
    virtual std::shared_ptr<Application::Instance> existing(const AppID& appId,
                                                            const std::string& job,
                                                            const std::string& instance,
                                                            const std::vector<Application::URL>& urls) override;

    virtual std::list<std::shared_ptr<Application>> runningApps() override;

    virtual std::vector<std::shared_ptr<instance::Base>> instances(const AppID& appID, const std::string& job) override;

    virtual core::Signal<const std::shared_ptr<Application>&, const std::shared_ptr<Application::Instance>&>&
        appStarted() override;
    virtual core::Signal<const std::shared_ptr<Application>&, const std::shared_ptr<Application::Instance>&>&
        appStopped() override;
    virtual core::Signal<const std::shared_ptr<Application>&,
                         const std::shared_ptr<Application::Instance>&,
                         Registry::FailureType>&
        appFailed() override;

    static std::string userBusPath();

    pid_t unitPrimaryPid(const AppID& appId, const std::string& job, const std::string& instance);
    std::vector<pid_t> unitPids(const AppID& appId, const std::string& job, const std::string& instance);
    void stopUnit(const AppID& appId, const std::string& job, const std::string& instance);

private:
    std::string cgroup_root_;
    std::shared_ptr<GDBusConnection> userbus_;

    guint handle_unitNew{0};     /**< GDBus signal watcher handle for the unit new signal */
    guint handle_unitRemoved{0}; /**< GDBus signal watcher handle for the unit removed signal */
    guint handle_appFailed{0};   /**< GDBus signal watcher handle for app failed signal */

    std::once_flag
        flag_appFailed; /**< Variable to track to see if signal handlers are installed for application failed */

    struct UnitInfo
    {
        std::string appid;
        std::string job;
        std::string inst;

        bool operator<(const UnitInfo& b) const
        {
            if (job < b.job)
            {
                return true;
            }
            else if (job == b.job)
            {
                if (appid < b.appid)
                {
                    return true;
                }
                else if (appid == b.appid)
                {
                    return inst < b.inst;
                }
                else
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
    };

    void getInitialUnits(const std::shared_ptr<GDBusConnection>& bus, const std::shared_ptr<GCancellable>& cancel);

    struct UnitData
    {
        std::string jobpath;
        std::string unitpath;
    };

    std::map<UnitInfo, std::shared_ptr<UnitData>> unitPaths;
    UnitInfo parseUnit(const std::string& unit) const;
    std::string unitName(const UnitInfo& info) const;
    std::string unitPath(const UnitInfo& info);

    UnitInfo unitNew(const std::string& name, const std::string& path, const std::shared_ptr<GDBusConnection>& bus);
    void unitRemoved(const std::string& name, const std::string& path);
    void emitSignal(
        core::Signal<const std::shared_ptr<Application>&, const std::shared_ptr<Application::Instance>&>& sig,
        UnitInfo& info);

    static std::string findEnv(const std::string& value, std::list<std::pair<std::string, std::string>>& env);
    static void removeEnv(const std::string& value, std::list<std::pair<std::string, std::string>>& env);
    static void copyEnv(const std::string& envname, std::list<std::pair<std::string, std::string>>& env);
    static void copyEnvByPrefix(const std::string& prefix, std::list<std::pair<std::string, std::string>>& env);
    static int envSize(std::list<std::pair<std::string, std::string>>& env);

    static std::vector<std::string> parseExec(std::list<std::pair<std::string, std::string>>& env);
    static void application_start_cb(GObject* obj, GAsyncResult* res, gpointer user_data);
};

}  // namespace manager
}  // namespace jobs
}  // namespace app_launch
}  // namespace ubuntu
