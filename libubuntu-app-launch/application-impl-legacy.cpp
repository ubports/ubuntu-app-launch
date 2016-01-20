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

#include "application-impl-legacy.h"
#include "application-info-desktop.h"

namespace Ubuntu {
namespace AppLaunch {
namespace AppImpls {

void
clear_app_info (GDesktopAppInfo * appinfo)
{
	g_clear_object(&appinfo);
}

Legacy::Legacy (const AppID::AppName &appname,
	  std::shared_ptr<GDesktopAppInfo> appinfo,
	  std::shared_ptr<Registry> registry) :
	Base(registry),
	_appname(appname),
	_appinfo(appinfo)
{
}

Legacy::Legacy (const AppID::AppName &appname,
	  std::shared_ptr<Registry> registry) :
	Legacy(appname, std::shared_ptr<GDesktopAppInfo>(
		g_desktop_app_info_new(appname.value().c_str()),
		clear_app_info), registry)
{
}

std::shared_ptr<Application::Info>
Legacy::info (void)
{
	if (_appinfo) {
		return std::make_shared<AppInfo::Desktop>(_appinfo, "/usr/share/icons/");
	} else {
		return {};
	}
}

std::list<std::shared_ptr<Application>>
Legacy::list (std::shared_ptr<Registry> registry)
{
	std::list<std::shared_ptr<Application>> list;
	GList * head = g_app_info_get_all();
	for (GList * item = head; item != nullptr; item = g_list_next(item)) {
		GDesktopAppInfo * appinfo = G_DESKTOP_APP_INFO(item->data);

		if (appinfo == nullptr)
			continue;

		if (g_app_info_should_show(G_APP_INFO(appinfo)) == FALSE)
			continue;

		g_object_ref(appinfo);
		auto app = std::make_shared<Legacy>(AppID::AppName::from_raw(g_app_info_get_id(G_APP_INFO(appinfo))),
								  std::shared_ptr<GDesktopAppInfo>(appinfo, clear_app_info),
								  registry);
		list.push_back(app);
	}

	g_list_free_full(head, g_object_unref);

	return list;
}

}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
