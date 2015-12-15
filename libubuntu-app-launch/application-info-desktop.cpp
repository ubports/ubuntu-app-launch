
#include "application-info-desktop.h"

namespace Ubuntu {
namespace AppLaunch {
namespace AppInfo {

Desktop::Desktop(std::shared_ptr<GDesktopAppInfo> appinfo, const std::string &basePath) :
	_appinfo(appinfo),
	_basePath(basePath)
{
}

const std::string &
Desktop::name()
{
	std::call_once(_nameFlag, [this]() {
		_name = std::string(g_app_info_get_display_name(G_APP_INFO(_appinfo.get())));
	});

	return _name;
}

const std::string &
Desktop::description()
{
	std::call_once(_descriptionFlag, [this]() {
		_description = std::string(g_app_info_get_description(G_APP_INFO(_appinfo.get())));
	});

	return _description;
}

const std::string &
Desktop::iconPath()
{
	std::call_once(_iconPathFlag, [this]() {
		auto relative = std::shared_ptr<gchar>(g_desktop_app_info_get_string(_appinfo.get(), "Icon"), [](gchar * str) { g_clear_pointer(&str, g_free); }); 
		auto cpath = std::shared_ptr<gchar>(g_build_filename(_basePath.c_str(), relative.get(), nullptr), [](gchar * str) { g_clear_pointer(&str, g_free); });
		_iconPath = cpath.get();
	});

	return _iconPath;
}

std::list<std::string>
Desktop::categories()
{
	return {};
}

}; // namespace AppInfo
}; // namespace AppLaunch
}; // namespace Ubuntu
