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

#include "info-watcher.h"
#include <glib.h>

namespace ubuntu
{
namespace app_launch
{
namespace info_watcher
{

Base::Base(const std::shared_ptr<Registry::Impl>& registry)
    : registry_(registry)
{
}

/** Accessor function to the registry that ensures we can still
    get it, which we always should be able to, but in case. */
std::shared_ptr<Registry::Impl> Base::getReg()
{
    auto reg = registry_.lock();
    if (G_UNLIKELY(!reg))
    {
        throw std::runtime_error{"App Store lost track of the Registry that owns it"};
    }
    return reg;
}

}  // namespace info_watcher
}  // namespace app_launch
}  // namespace ubuntu
