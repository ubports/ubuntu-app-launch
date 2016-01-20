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

#include "application.h"
#include <gio/gdesktopappinfo.h>
#include <mutex>

#pragma once

namespace Ubuntu {
namespace AppLaunch {
namespace AppInfo {

class Desktop : public Application::Info {
public:
	Desktop(std::shared_ptr<GDesktopAppInfo> appinfo,
	        const std::string &basePath);

	const Application::Info::Name &name() override;
	const Application::Info::Description &description() override;
	const Application::Info::IconPath &iconPath() override;
	std::list<Application::Info::Category> categories() override;

private:
	Application::Info::Name _name;
	Application::Info::Description _description;
	Application::Info::IconPath _iconPath;

	std::once_flag _nameFlag;
	std::once_flag _descriptionFlag;
	std::once_flag _iconPathFlag;

	std::shared_ptr<GDesktopAppInfo> _appinfo;
	std::string _basePath;
};


}; // namespace AppInfo
}; // namespace AppLaunch
}; // namespace Ubuntu
