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
#include "snapd-info.h"

#pragma once

namespace ubuntu
{
namespace app_launch
{
namespace app_impls
{

/** Class implementing a Applications that are installed in the
    system as Snaps. This class connects to snapd to get information
    on the interfaces of the installed snaps and sees if any of them
    are applicable to the user session. Currently that means if the
    command has the mir, unity8, unity7 or x11 interfaces.

    For Application IDs snaps use a very similar scheme to Click
    packages. The package field is the name of the snap package, typically
    this is the overall application name. The appname is the command
    in the snap package, which needs to be associated with one of our
    supported interfaces and have a desktop file. Lastly the version
    field is actually the snap revision, this value changes even on
    updates between channels of the same version so it provides a
    greater amount of uniqueness.
*/
class Snap : public Base
{
public:
    typedef std::tuple<app_info::Desktop::XMirEnable, Application::Info::UbuntuLifecycle> InterfaceInfo;

    Snap(const AppID& appid, const std::shared_ptr<Registry>& registry);
    Snap(const AppID& appid, const std::shared_ptr<Registry>& registry, const InterfaceInfo &interfaceInfo);

    static std::list<std::shared_ptr<Application>> list(const std::shared_ptr<Registry>& registry);

    AppID appId() override;

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

    virtual std::shared_ptr<Application::Instance> findInstance(const std::string& instanceid) override;

private:
    /** AppID of the Snap. Should be the name of the snap package.
        The name of the command. And then the revision. */
    AppID appid_;
    /** The app's displayed information. Should be from a desktop
        file that is put in ${SNAP_DIR}/meta/gui/${command}.desktop */
    std::shared_ptr<app_info::Desktop> info_;
    /** Information that we get from Snapd on the package */
    std::shared_ptr<snapd::Info::PkgInfo> pkgInfo_;

    std::list<std::pair<std::string, std::string>> launchEnv();
    static InterfaceInfo findInterfaceInfo(const AppID& appid, const std::shared_ptr<Registry>& registry);
    static bool checkPkgInfo(const std::shared_ptr<snapd::Info::PkgInfo>& pkginfo, const AppID& appid);
};

}  // namespace app_impls
}  // namespace app_launch
}  // namespace ubuntu
