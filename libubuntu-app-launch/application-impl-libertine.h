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
#include <gio/gdesktopappinfo.h>

#pragma once

namespace Ubuntu {
namespace AppLaunch {
namespace AppImpls {

class Libertine : public Base {
public:
	Libertine (const AppID::Package &container,
	      const AppID::AppName &appname,
	      std::shared_ptr<Registry> registry);

	static std::list<std::shared_ptr<Application>> list (std::shared_ptr<Registry> registry);

	AppID appId() override {
		return {
			package: _container,
			appname: _appname,
			version: AppID::Version::from_raw("0.0")
		};
	}

	std::shared_ptr<Info> info() override;

private:
	AppID::Package _container;
	AppID::AppName _appname;
	std::shared_ptr<GDesktopAppInfo> _appinfo;
};

}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
