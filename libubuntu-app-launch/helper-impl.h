/*
 * Copyright © 2017 Canonical Ltd.
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

#include <list>

#include "helper.h"
#include "jobs-base.h"

namespace ubuntu
{
namespace app_launch
{
namespace helper_impls
{

/** We really should have put a relationship between Helper::Instance
and Application::Instance in the API. But to work around that today
we're just handling it here because the helper interface is a subset
of what the Application::Instance class provides */
class BaseInstance : public Helper::Instance
{
public:
    std::shared_ptr<jobs::instance::Base> impl;

    BaseInstance(const std::shared_ptr<jobs::instance::Base>& inst);
    BaseInstance(const std::shared_ptr<Application::Instance>& inst);

    bool isRunning() override;
    void stop() override;

    const std::string& getInstanceId()
    {
        return impl->getInstanceId();
    }
};

class Base : public Helper
{
public:
    Base(const Helper::Type& type, const AppID& appid, const std::shared_ptr<Registry>& registry);

    AppID appId() override;

    bool hasInstances() override;
    std::vector<std::shared_ptr<Helper::Instance>> instances() override;

    std::shared_ptr<Helper::Instance> launch(std::vector<Helper::URL> urls = {}) override;
    std::shared_ptr<Helper::Instance> launch(MirPromptSession* session, std::vector<Helper::URL> urls = {}) override;

    std::shared_ptr<Helper::Instance> existingInstance(const std::string& instanceid);

private:
    Helper::Type _type;
    AppID _appid;
    std::shared_ptr<Registry> _registry;
};

}  // namespace helper_impl
}  // namespace app_launch
}  // namespace ubuntu
