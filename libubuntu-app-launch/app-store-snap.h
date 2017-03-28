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

#pragma once

#include "app-store-base.h"

namespace ubuntu
{
namespace app_launch
{
namespace app_store
{

class Snap : public Base
{
public:
    Snap(const Registry& registry);
    virtual ~Snap();

    /* Discover tools */
    virtual bool verifyPackage(const AppID::Package& package) override;
    virtual bool verifyAppname(const AppID::Package& package, const AppID::AppName& appname) override;
    virtual AppID::AppName findAppname(const AppID::Package& package, AppID::ApplicationWildcard card) override;
    virtual AppID::Version findVersion(const AppID::Package& package, const AppID::AppName& appname) override;
    virtual bool hasAppId(const AppID& appid) override;

    /* Possible apps */
    virtual std::list<std::shared_ptr<Application>> list() override;

    /* Application Creation */
    virtual std::shared_ptr<app_impls::Base> create(const AppID& appid) override;
};

}  // namespace app_store
}  // namespace app_launch
}  // namespace ubuntu
