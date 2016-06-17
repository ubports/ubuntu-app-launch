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

#include <memory>
#include <vector>

#include <json-glib/json-glib.h>

#include "appid.h"

namespace ubuntu
{
namespace app_launch
{
namespace app_impls
{
namespace snapd
{

class Info
{
public:
    Info();
    virtual ~Info() = default;

    struct AppInfo
    {
    };
    std::shared_ptr<AppInfo> appInfo(AppID &appid);

    std::vector<AppID> appsForInterface(const std::string &interface);

private:
    std::shared_ptr<JsonNode> snapdJson(const std::string &endpoint);
};

}  // namespace snapd
}  // namespace app_impls
}  // namespace app_launch
}  // namespace ubuntu
