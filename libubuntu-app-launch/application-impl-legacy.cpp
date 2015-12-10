
#include "application-impl-legacy.h"

namespace Ubuntu {
namespace AppLaunch {
namespace AppImpls {

Legacy::Legacy (const std::string &appname,
	  std::shared_ptr<Connection> connection) :
	Impl(appname, "", "", connection)
{
	_appinfo = std::shared_ptr<GDesktopAppInfo>(
		g_desktop_app_info_new(appname.c_str()),
		[](GDesktopAppInfo * appinfo) { g_clear_object(&appinfo); });

	_name = g_app_info_get_display_name(G_APP_INFO(_appinfo.get()));
	_description = g_app_info_get_description(G_APP_INFO(_appinfo.get()));

	/* TODO: Icon */
}

const std::string&
Legacy::name () {
	return _name;
}

const std::string&
Legacy::description () {
	return _description;
}

const std::string&
Legacy::iconPath () {
	return _iconPath;
}

std::list<std::string>
Legacy::categories () {
	return {};
}

std::list<std::shared_ptr<Application>>
Legacy::list (std::shared_ptr<Connection> connection)
{
	return {};
}

}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
