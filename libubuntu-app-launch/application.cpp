
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

}; // namespace AppLaunch
}; // namespace Ubuntu
