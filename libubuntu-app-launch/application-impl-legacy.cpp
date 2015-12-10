
#include "application-impl-legacy.h"

namespace Ubuntu {
namespace AppLaunch {
namespace AppImpls {

void
clear_app_info (GDesktopAppInfo * appinfo)
{
	g_clear_object(&appinfo);
}

Legacy::Legacy (const std::string &appname,
	  std::shared_ptr<GDesktopAppInfo> appinfo,
	  std::shared_ptr<Connection> connection) :
	Impl(appname, "", "", connection),
	_appinfo(appinfo)
{
	_name = g_app_info_get_display_name(G_APP_INFO(_appinfo.get()));
	_description = g_app_info_get_description(G_APP_INFO(_appinfo.get()));

	/* TODO: Icon */
}

Legacy::Legacy (const std::string &appname,
	  std::shared_ptr<Connection> connection) :
	Legacy(appname, std::shared_ptr<GDesktopAppInfo>(
		g_desktop_app_info_new(appname.c_str()),
		clear_app_info), connection)
{
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
	std::list<std::shared_ptr<Application>> list;
	GList * head = g_app_info_get_all();
	for (GList * item = head; item != nullptr; item = g_list_next(item)) {
		GDesktopAppInfo * appinfo = G_DESKTOP_APP_INFO(item->data);

		if (appinfo == nullptr)
			continue;

		if (g_app_info_should_show(G_APP_INFO(appinfo)) == FALSE)
			continue;

		g_object_ref(appinfo);
		auto nlegacy = new Legacy(g_app_info_get_id(G_APP_INFO(appinfo)),
								  std::shared_ptr<GDesktopAppInfo>(appinfo, clear_app_info),
								  connection);
		auto app = std::make_shared<Application>(std::unique_ptr<Application::Impl>(nlegacy));
		list.push_back(app);
	}

	g_list_free_full(head, g_object_unref);

	return list;
}

}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
