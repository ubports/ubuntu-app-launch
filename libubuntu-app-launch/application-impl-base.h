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

    virtual std::shared_ptr<Application::Instance> findInstance(const std::string& instanceid) = 0;

protected:
    /** Pointer to the registry so we can ask it for things */
    std::shared_ptr<Registry> _registry;

    static std::list<std::pair<std::string, std::string>> confinedEnv(const std::string& package,
                                                                      const std::string& pkgdir);
};

}  // namespace app_impls
}  // namespace app_launch
}  // namespace ubuntu
