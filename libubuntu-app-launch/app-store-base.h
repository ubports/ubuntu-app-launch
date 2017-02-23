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

#include "appid.h"
#include "info-watcher.h"
#include "registry.h"

namespace ubuntu
{
namespace app_launch
{
namespace app_store
{

class Base : public info_watcher::Base
{
public:
    AppStore();
    virtual ~AppStore();

    /* Discover tools */
    virtual bool verifyPackage(const AppID::Package& package, const std::shared_ptr<Registry>& registry) = 0;
    virtual bool verifyAppname(const AppID::Package& package,
                               const AppID::AppName& appname,
                               const std::shared_ptr<Registry>& registry) = 0;
    virtual AppID::AppName findAppname(const AppID::Package& package,
                                       AppID::ApplicationWildcard card,
                                       const std::shared_ptr<Registry>& registry) = 0;
    virtual AppID::Version findVersion(const AppID::Package& package,
                                       const AppID::AppName& appname,
                                       const std::shared_ptr<Registry>& registry) = 0;
    virtual bool hasAppId(const AppID& appid, const std::shared_ptr<Registry>& registry) = 0;

    /* Application Creation */
    virtual std::shared_ptr<app_impls::Base> create(const AppID& appid, const std::shared_ptr<Registry>& registry) = 0;
};

}  // namespace app_store
}  // namespace app_launch
}  // namespace ubuntu
