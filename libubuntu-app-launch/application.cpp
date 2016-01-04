
extern "C" {
#include "app-info.h"
#include "ubuntu-app-launch.h"
}

#include "application.h"
#include "application-impl-click.h"
#include "application-impl-legacy.h"
#include "application-impl-libertine.h"

namespace Ubuntu {
namespace AppLaunch {

std::shared_ptr<Application>
Application::create (const Application::AppID &appid,
	             std::shared_ptr<Registry> registry)
{
	auto sappid = Application::appIdString(appid);
	if (app_info_legacy(std::get<1>(appid).value().c_str(), NULL, NULL)) {
		return std::make_shared<AppImpls::Legacy>(std::get<1>(appid), registry);
	} else if (app_info_click(sappid.c_str(), NULL, NULL)) {
		return std::make_shared<AppImpls::Click>(appid, registry);
	} else if (app_info_libertine(sappid.c_str(), NULL, NULL)) {
		return std::make_shared<AppImpls::Libertine>(std::get<0>(appid), std::get<1>(appid), registry);
	} else {
		throw std::runtime_error("Invalid app ID: " + sappid);
	}
}

std::tuple<Application::Package, Application::AppName, Application::Version>
Application::appIdParse (const std::string &appid)
{
	gchar * cpackage;
	gchar * cappname;
	gchar * cversion;

	if (ubuntu_app_launch_app_id_parse(appid.c_str(), &cpackage, &cappname, &cversion) == FALSE) {
		throw std::runtime_error("Unable to parse: " + appid);
	}

	auto tuple = std::make_tuple(Application::Package::from_raw(cpackage), Application::AppName::from_raw(cappname), Application::Version::from_raw(cversion));

	g_free(cpackage);
	g_free(cappname);
	g_free(cversion);

	return tuple;
}

std::string
Application::appIdString (const Application::AppID &appid)
{
	return std::get<0>(appid).value() + "_" + std::get<1>(appid).value() + "_" + std::get<2>(appid).value();
}



}; // namespace AppLaunch
}; // namespace Ubuntu
