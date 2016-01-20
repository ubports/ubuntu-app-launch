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

#include "application-info-desktop.h"

namespace Ubuntu {
namespace AppLaunch {
namespace AppInfo {

Desktop::Desktop(std::shared_ptr<GDesktopAppInfo> appinfo, const std::string &basePath) :
	_name(Application::Info::Name::from_raw({})),
	_description(Application::Info::Description::from_raw({})),
	_iconPath(Application::Info::IconPath::from_raw({})),
	_appinfo(appinfo),
	_basePath(basePath)
{
}

const Application::Info::Name &
Desktop::name()
{
	std::call_once(_nameFlag, [this]() {
		_name = Application::Info::Name::from_raw(g_app_info_get_display_name(G_APP_INFO(_appinfo.get())));
	});

	return _name;
}

const Application::Info::Description &
Desktop::description()
{
	std::call_once(_descriptionFlag, [this]() {
		_description = Application::Info::Description::from_raw(g_app_info_get_description(G_APP_INFO(_appinfo.get())));
	});

	return _description;
}

const Application::Info::IconPath &
Desktop::iconPath()
{
	std::call_once(_iconPathFlag, [this]() {
		auto relative = std::shared_ptr<gchar>(g_desktop_app_info_get_string(_appinfo.get(), "Icon"), [](gchar * str) { g_clear_pointer(&str, g_free); }); 
		auto cpath = std::shared_ptr<gchar>(g_build_filename(_basePath.c_str(), relative.get(), nullptr), [](gchar * str) { g_clear_pointer(&str, g_free); });
		_iconPath = Application::Info::IconPath::from_raw(cpath.get());
	});

	return _iconPath;
}

std::list<Application::Info::Category>
Desktop::categories()
{
	return {};
}

}; // namespace AppInfo
}; // namespace AppLaunch
}; // namespace Ubuntu
