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

#include <gio/gdesktopappinfo.h>

#include "application-impl-base.h"
#include "application-info-desktop.h"

#pragma once

namespace ubuntu
{
namespace app_launch
{
namespace app_impls
{

class Legacy : public Base
{
public:
    Legacy(const AppID::AppName& appname, const std::shared_ptr<Registry>& registry);

    AppID appId() override
    {
        return {AppID::Package::from_raw({}), _appname, AppID::Version::from_raw({})};
    }

    std::shared_ptr<Info> info() override;

    static std::list<std::shared_ptr<Application>> list(const std::shared_ptr<Registry>& registry);

    std::vector<std::shared_ptr<Instance>> instances() override;

    std::shared_ptr<Instance> launch(const std::vector<Application::URL>& urls = {}) override;
    std::shared_ptr<Instance> launchTest(const std::vector<Application::URL>& urls = {}) override;

private:
    AppID::AppName _appname;
    std::string _basedir;
    std::shared_ptr<GKeyFile> _keyfile;
    std::shared_ptr<app_info::Desktop> appinfo_;
    std::string desktopPath_;

    std::list<std::pair<std::string, std::string>> launchEnv(const std::string& instance);
    std::string getInstance();
};

}  // namespace app_impls
}  // namespace app_launch
}  // namespace ubuntu
