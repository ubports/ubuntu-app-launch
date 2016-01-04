
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
	std::string sappid = appid;
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
Application::AppID::parse (const std::string &sappid)
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

Application::AppID::operator std::string() const
{
	return package.value() + "_" + appname.value() + "_" + version.value();
}

std::string
app_wildcard (Application::AppID::ApplicationWildcard card)
{
	switch (card) {
	case Application::AppID::ApplicationWildcard::FIRST_LISTED:
		return "first-listed";
	case Application::AppID::ApplicationWildcard::LAST_LISTED:
		return "last-listed";
	}

	return "";
}

std::string
ver_wildcard (Application::AppID::VersionWildcard card)
{
	switch (card) {
	case Application::AppID::VersionWildcard::CURRENT_USER_VERSION:
		return "current-user-version";
	}

	return "";
}

Application::AppID
Application::AppID::discover (const std::string &package)
{
	return discover(package, ApplicationWildcard::FIRST_LISTED, VersionWildcard::CURRENT_USER_VERSION);
}

Application::AppID
Application::AppID::discover (const std::string &package, const std::string &appname)
{
	return discover(package, appname, VersionWildcard::CURRENT_USER_VERSION);
}

Application::AppID
Application::AppID::discover (const std::string &package, const std::string &appname, const std::string &version)
{
	return Application::AppID::parse(ubuntu_app_launch_triplet_to_app_id(package.c_str(), appname.c_str(), version.c_str()));
}

Application::AppID
Application::AppID::discover (const std::string &package, ApplicationWildcard appwildcard)
{
	return Application::AppID::discover(package, appwildcard, VersionWildcard::CURRENT_USER_VERSION);
}

Application::AppID
Application::AppID::discover (const std::string &package, ApplicationWildcard appwildcard, VersionWildcard versionwildcard)
{
	return discover(package, app_wildcard(appwildcard), ver_wildcard(versionwildcard));
}

Application::AppID
Application::AppID::discover (const std::string &package, const std::string &appname, VersionWildcard versionwildcard)
{
	return discover(package, appname, ver_wildcard(versionwildcard));
}


}; // namespace AppLaunch
}; // namespace Ubuntu
