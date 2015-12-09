
extern "C" {
#include "app-info.h"
}

#include "application.h"
#include "application-impl-click.h"
#include "application-impl-legacy.h"
#include "application-impl-libertine.h"

namespace Ubuntu {
namespace AppLaunch {

Application::Application (const std::string &package,
	             const std::string &appname,
	             const std::string &version,
	             std::shared_ptr<Connection> connection)
{
	auto appid = package + "_" + appname + "_" + version;
	if (app_info_legacy(appid.c_str(), NULL, NULL)) {
		impl = std::unique_ptr<ImplLegacy>(new ImplLegacy(package, appname, version, connection));
	} else if (app_info_click(appid.c_str(), NULL, NULL)) {
		impl = std::unique_ptr<ImplClick>(new ImplClick(package, appname, version, connection));
	} else if (app_info_libertine(appid.c_str(), NULL, NULL)) {
		impl = std::unique_ptr<ImplLibertine>(new ImplLibertine(package, appname, version, connection));
	}
}

}; // namespace AppLaunch
}; // namespace Ubuntu
