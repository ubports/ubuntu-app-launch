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
#include <regex>

#include "application-impl-base.h"
#include "application-info-desktop.h"

#pragma once

namespace ubuntu
{
namespace app_launch
{
namespace app_impls
{

/** Application Implementation for Legacy applications. These are applications
    that are typically installed as Debian packages on the base system. The
    standard place for them to put their desktop files is in /usr/share/applications
    though other directories may be used by setting the appropriate XDG environment
    variables. This implementation makes use of the GIO Desktop Appinfo functions
    which do caching of those files to make access faster.

    AppIDs for legacy applications only include the Appname variable. Both the package
    and the version entries are empty strings. The appname variable is the filename
    of the desktop file describing the application with the ".desktop" suffix.

    More info: https://specifications.freedesktop.org/desktop-entry-spec/latest/
*/
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

    static bool hasAppId(const AppID& appId, const std::shared_ptr<Registry>& registry);

    static bool verifyPackage(const AppID::Package& package, const std::shared_ptr<Registry>& registry);
    static bool verifyAppname(const AppID::Package& package,
                              const AppID::AppName& appname,
                              const std::shared_ptr<Registry>& registry);
    static AppID::AppName findAppname(const AppID::Package& package,
                                      AppID::ApplicationWildcard card,
                                      const std::shared_ptr<Registry>& registry);
    static AppID::Version findVersion(const AppID::Package& package,
                                      const AppID::AppName& appname,
                                      const std::shared_ptr<Registry>& registry);

    virtual std::shared_ptr<Application::Instance> findInstance(const std::string& instanceid) override;

    static std::shared_ptr<info_watcher::Base> createInfoWatcher(const std::shared_ptr<Registry>& reg);

private:
    AppID::AppName _appname;
    std::string _basedir;
    std::shared_ptr<GKeyFile> _keyfile;
    std::shared_ptr<app_info::Desktop> appinfo_;
    std::string desktopPath_;
    std::regex instanceRegex_;

    std::list<std::pair<std::string, std::string>> launchEnv(const std::string& instance);
};

}  // namespace app_impls
}  // namespace app_launch
}  // namespace ubuntu
