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

#pragma once

namespace ubuntu
{
namespace app_launch
{
namespace app_impls
{

/** Application Implmentation for the Libertine container system. Libertine
    sets up containers that are read/write on a read only system, to all for
    more dynamic packaging systems (like deb) to work. This provides some
    compatibility for older applications or those who are only distributed in
    packaging systems requiring full system access.

    Application IDs for Libertine applications have the package field as the
    name of the container. The appname is similar to that of the Legacy() implementation
    as the filename of the desktop file defining the application without the
    ".desktop" suffix. UAL has no way to know the version, so it is always hard
    coded to "0.0".

    Libertine applications always are setup with XMir and started using the
    libertine-launch utility which configures the environment for the container.

    More info: https://wiki.ubuntu.com/Touch/Libertine
*/
class Libertine : public Base
{
public:
    Libertine(const AppID::Package& container,
              const AppID::AppName& appname,
              const std::shared_ptr<Registry>& registry);

    static std::list<std::shared_ptr<Application>> list(const std::shared_ptr<Registry>& registry);

    AppID appId() override
    {
        return {_container, _appname, AppID::Version::from_raw("0.0")};
    }

    std::shared_ptr<Info> info() override;

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

private:
    AppID::Package _container;
    AppID::AppName _appname;
    std::shared_ptr<GKeyFile> _keyfile;
    std::string _basedir;
    std::shared_ptr<app_info::Desktop> appinfo_;

    std::list<std::pair<std::string, std::string>> launchEnv();
    static std::shared_ptr<GKeyFile> keyfileFromPath(const std::string& pathname);
    static std::shared_ptr<GKeyFile> findDesktopFile(const std::string& basepath, const std::string& subpath, const std::string& filename);
};

}  // namespace app_impls
}  // namespace app_launch
}  // namespace ubuntu
