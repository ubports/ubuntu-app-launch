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
#include <gio/gio.h>
#include <map>

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

    static std::string userBusPath();

private:
    std::shared_ptr<GDBusConnection> userbus_;

    /* ssssssouso */
    struct UnitEntry
    {
        std::string id;
        std::string description;
        std::string loadState;
        std::string activeState;
        std::string subState;
        std::string following;
        std::string path;
        std::uint32_t jobId;
        std::string jobType;
        std::string jobPath;
    };
    std::list<UnitEntry> listUnits();

    struct UnitInfo
    {
        std::string job;
        std::string appid;
        std::string inst;
    };
    UnitInfo parseUnit(const std::string& unit);
    std::string unitName(const UnitInfo& info);
};

}  // namespace manager
}  // namespace jobs
}  // namespace app_launch
}  // namespace ubuntu
