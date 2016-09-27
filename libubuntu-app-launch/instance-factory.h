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
#include "application.h"

namespace ubuntu
{
namespace app_launch
{

class InstanceFactory
{
public:
    InstanceFactory(std::shared_ptr<Registry> registry);
    virtual ~InstanceFactory() = default;

    /** Flag for whether we should include the testing environment variables */
    enum class launchMode
    {
        STANDARD, /**< Standard variable set */
        TEST      /**< Include testing environment vars */
    };

    virtual std::shared_ptr<Application::Instance> launch(
        const AppID& appId,
        const std::string& job,
        const std::string& instance,
        const std::vector<Application::URL>& urls,
        launchMode mode,
        std::function<std::list<std::pair<std::string, std::string>>(void)>& getenv) = 0;

    virtual std::shared_ptr<Application::Instance> existing(const AppID& appId,
                                                            const std::string& job,
                                                            const std::string& instance,
                                                            const std::vector<Application::URL>& urls) = 0;

    static std::shared_ptr<InstanceFactory> determineFactory(std::shared_ptr<Registry> registry);

protected:
    std::shared_ptr<Registry> registry_;
};

}  // namespace app_launch
}  // namespace ubuntu
