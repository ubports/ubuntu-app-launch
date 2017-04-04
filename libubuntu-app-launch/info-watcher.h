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

#include "application.h"
#include "registry.h"

#include <core/signal.h>
#include <set>

namespace ubuntu
{
namespace app_launch
{
namespace info_watcher
{

class Base
{
public:
    Base(const std::shared_ptr<Registry::Impl>& registry);
    virtual ~Base() = default;

    virtual core::Signal<const std::shared_ptr<Application>&>& infoChanged()
    {
        return infoChanged_;
    }

    virtual core::Signal<const std::shared_ptr<Application>&>& appAdded()
    {
        return appAdded_;
    }

    virtual core::Signal<const AppID&>& appRemoved()
    {
        return appRemoved_;
    }

protected:
    /** Signal for info changed on an application */
    core::Signal<const std::shared_ptr<Application>&> infoChanged_;
    /** Signal for applications added */
    core::Signal<const std::shared_ptr<Application>&> appAdded_;
    /** Signal for applications removed */
    core::Signal<const AppID&> appRemoved_;

    /** Accessor function to the registry that ensures we can still
        get it, which we always should be able to, but in case. */
    std::shared_ptr<Registry::Impl> getReg();

private:
    /** Pointer to our implementing registry */
    std::weak_ptr<Registry::Impl> registry_;
};

}  // namespace info_watcher
}  // namespace app_launch
}  // namespace ubuntu
