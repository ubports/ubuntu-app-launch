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

#include "application-impl-base.h"
#include "application-info-desktop.h"

#include <gio/gdesktopappinfo.h>
#include <json-glib/json-glib.h>

#pragma once

namespace ubuntu
{
namespace app_launch
{
namespace app_impls
{

class Click : public Base
{
public:
    Click(const AppID& appid, const std::shared_ptr<Registry>& registry);
    Click(const AppID& appid, const std::shared_ptr<JsonObject>& manifest, const std::shared_ptr<Registry>& registry);

    static std::list<std::shared_ptr<Application>> list(const std::shared_ptr<Registry>& registry);

    AppID appId() override;

    std::shared_ptr<Info> info() override;

    std::vector<std::shared_ptr<Instance>> instances() override;

    std::shared_ptr<Instance> launch(const std::vector<Application::URL>& urls = {}) override;
    std::shared_ptr<Instance> launchTest(const std::vector<Application::URL>& urls = {}) override;

private:
    AppID _appid;

    std::shared_ptr<JsonObject> _manifest;

    std::string _clickDir;
    std::shared_ptr<GKeyFile> _keyfile;

    std::shared_ptr<app_info::Desktop> _info;

    std::list<std::pair<std::string, std::string>> launchEnv();
};

}  // namespace app_impls
}  // namespace app_launch
}  // namespace ubuntu
