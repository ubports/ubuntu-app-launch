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

#include "app-store-legacy.h"

namespace ubuntu
{
namespace app_launch
{
namespace app_store
{

Legacy::Legacy()
{
}

Legacy::~Legacy()
{
}

bool Legacy::verifyPackage(const AppID::Package& package, const std::shared_ptr<Registry>& registry)
{
    return false;
}

bool Legacy::verifyAppname(const AppID::Package& package,
                           const AppID::AppName& appname,
                           const std::shared_ptr<Registry>& registry)
{
    return false;
}

AppID::AppName Legacy::findAppname(const AppID::Package& package,
                                   AppID::ApplicationWildcard card,
                                   const std::shared_ptr<Registry>& registry)
{
    return AppID::AppName::from_raw({});
}

AppID::Version Legacy::findVersion(const AppID::Package& package,
                                   const AppID::AppName& appname,
                                   const std::shared_ptr<Registry>& registry)
{
    return AppID::Version::from_raw({});
}

bool Legacy::hasAppId(const AppID& appid, const std::shared_ptr<Registry>& registry)
{
    return false;
}

std::list<std::shared_ptr<Application>> Legacy::list(const std::shared_ptr<Registry>& registry)
{
    return {};
}

std::shared_ptr<app_impls::Base> Legacy::create(const AppID& appid, const std::shared_ptr<Registry>& registry)
{
    return {};
}

}  // namespace app_store
}  // namespace app_launch
}  // namespace ubuntu
