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
    Base(const std::shared_ptr<Registry> &registry);

    bool hasInstances() override;
    std::vector<std::shared_ptr<Instance>> instances() override;

    std::shared_ptr<Instance> launch(const std::vector<Application::URL> &urls = {}) override;
    std::shared_ptr<Instance> launchTest(const std::vector<Application::URL> &urls = {}) override;

	virtual std::pair<const std::string, const std::string> jobAndInstance () = 0;

protected:
    std::shared_ptr<Registry> _registry;
};

};  // namespace app_impls
};  // namespace app_launch
};  // namespace ubuntu
