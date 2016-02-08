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

#include "helper.h"

#include "helper-impl-click.h"

namespace Ubuntu
{
namespace AppLaunch
{

std::shared_ptr<Helper> Helper::create(Type type, AppID appid, std::shared_ptr<Registry> registry)
{
    /* Only one type today */
    return std::make_shared<HelperImpls::Click>(type, appid, registry);
}

};  // namespace AppLaunch
};  // namespace Ubuntu
