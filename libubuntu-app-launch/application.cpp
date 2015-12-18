
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
Application::create (const Application::Package &package,
	             const Application::AppName &appname,
	             const Application::Version &version,
	             std::shared_ptr<Registry> registry)
{
	auto appid = package.value() + "_" + appname.value() + "_" + version.value();
	if (app_info_legacy(appname.value().c_str(), NULL, NULL)) {
		return std::make_shared<AppImpls::Legacy>(appname, registry);
	} else if (app_info_click(appid.c_str(), NULL, NULL)) {
		return std::make_shared<AppImpls::Click>(package, appname, version, registry);
	} else if (app_info_libertine(appid.c_str(), NULL, NULL)) {
		return std::make_shared<AppImpls::Libertine>(package, appname, registry);
	} else {
		throw std::runtime_error("Invalid app ID: " + appid);
	}
}

std::tuple<Application::Package, Application::AppName, Application::Version>
Application::appIdParse (const Application::AppID &appid)
{
	gchar * cpackage;
	gchar * cappname;
	gchar * cversion;

	if (ubuntu_app_launch_app_id_parse(appid.value().c_str(), &cpackage, &cappname, &cversion) == FALSE) {
		throw std::runtime_error("Unable to parse: " + appid.value());
	}

	auto tuple = std::make_tuple(Application::Package::from_raw(cpackage), Application::AppName::from_raw(cappname), Application::Version::from_raw(cversion));

	g_free(cpackage);
	g_free(cappname);
	g_free(cversion);

	return tuple;
}


}; // namespace AppLaunch
}; // namespace Ubuntu
