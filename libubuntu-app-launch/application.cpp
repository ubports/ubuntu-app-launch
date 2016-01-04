
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
	if (app_info_legacy(appid.appname.value().c_str(), NULL, NULL)) {
		return std::make_shared<AppImpls::Legacy>(appid.appname, registry);
	} else if (app_info_click(sappid.c_str(), NULL, NULL)) {
		return std::make_shared<AppImpls::Click>(appid, registry);
	} else if (app_info_libertine(sappid.c_str(), NULL, NULL)) {
		return std::make_shared<AppImpls::Libertine>(appid.package, appid.appname, registry);
	} else {
		throw std::runtime_error("Invalid app ID: " + sappid);
	}
}

Application::AppID
Application::appIdParse (const std::string &sappid)
{
	gchar * cpackage;
	gchar * cappname;
	gchar * cversion;

	if (ubuntu_app_launch_app_id_parse(sappid.c_str(), &cpackage, &cappname, &cversion) == FALSE) {
		throw std::runtime_error("Unable to parse: " + sappid);
	}

	Application::AppID appid{
		package: Application::Package::from_raw(cpackage),
		appname: Application::AppName::from_raw(cappname),
		version: Application::Version::from_raw(cversion)
	};

	g_free(cpackage);
	g_free(cappname);
	g_free(cversion);

	return appid;
}

std::string
Application::appIdString (const Application::AppID &appid)
{
	return appid.package.value() + "_" + appid.appname.value() + "_" + appid.version.value();
}



}; // namespace AppLaunch
}; // namespace Ubuntu
