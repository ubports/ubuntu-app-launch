
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

Legacy::Legacy (const Application::AppName &appname,
	  std::shared_ptr<GDesktopAppInfo> appinfo,
	  std::shared_ptr<Registry> registry) :
	Base(registry),
	_appname(appname),
	_appinfo(appinfo)
{
}

Legacy::Legacy (const Application::AppName &appname,
	  std::shared_ptr<Registry> registry) :
	Legacy(appname, std::shared_ptr<GDesktopAppInfo>(
		g_desktop_app_info_new(appname.value().c_str()),
		clear_app_info), registry)
{
}

std::shared_ptr<Application::Info>
Legacy::info (void)
{
	return std::make_shared<AppInfo::Desktop>(_appinfo, "/usr/share/icons/");
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
		auto app = std::make_shared<Legacy>(Application::AppName::from_raw(g_app_info_get_id(G_APP_INFO(appinfo))),
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
