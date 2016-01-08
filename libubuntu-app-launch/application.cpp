
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
Application::create (const AppID &appid,
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

AppID::AppID () :
	package(Package::from_raw({})),
	appname(AppName::from_raw({})),
	version(Version::from_raw({}))
{
}

AppID::AppID (Package pkg, AppName app, Version ver) :
	package(pkg),
	appname(app),
	version(ver)
{
}

AppID
AppID::parse (const std::string &sappid)
{
	gchar * cpackage;
	gchar * cappname;
	gchar * cversion;

	if (ubuntu_app_launch_app_id_parse(sappid.c_str(), &cpackage, &cappname, &cversion) == FALSE) {
		/* Assume we're a legacy appid */
		return {
			AppID::Package::from_raw({}),
			AppID::AppName::from_raw(sappid),
			AppID::Version::from_raw({})
		};
	}

	AppID appid(
		AppID::Package::from_raw(cpackage),
		AppID::AppName::from_raw(cappname),
		AppID::Version::from_raw(cversion)
	);

	g_free(cpackage);
	g_free(cappname);
	g_free(cversion);

	return appid;
}

AppID::operator std::string() const
{
	if (package.value().empty() && version.value().empty() && !appname.value().empty()) {
		return appname.value();
	}

	return package.value() + "_" + appname.value() + "_" + version.value();
}

int
AppID::operator == (const AppID &other) const
{
	return package.value() == other.package.value() &&
		appname.value() == other.appname.value() &&
		version.value() == other.version.value();
}

int
AppID::operator != (const AppID &other) const
{
	return package.value() != other.package.value() ||
		appname.value() != other.appname.value() ||
		version.value() != other.version.value();
}

bool
AppID::empty () const
{
	return package.value().empty() &&
		appname.value().empty() &&
		version.value().empty();
}

std::string
app_wildcard (AppID::ApplicationWildcard card)
{
	switch (card) {
	case AppID::ApplicationWildcard::FIRST_LISTED:
		return "first-listed";
	case AppID::ApplicationWildcard::LAST_LISTED:
		return "last-listed";
	}

	return "";
}

std::string
ver_wildcard (AppID::VersionWildcard card)
{
	switch (card) {
	case AppID::VersionWildcard::CURRENT_USER_VERSION:
		return "current-user-version";
	}

	return "";
}

AppID
AppID::discover (const std::string &package)
{
	return discover(package, ApplicationWildcard::FIRST_LISTED, VersionWildcard::CURRENT_USER_VERSION);
}

AppID
AppID::discover (const std::string &package, const std::string &appname)
{
	return discover(package, appname, VersionWildcard::CURRENT_USER_VERSION);
}

AppID
AppID::discover (const std::string &package, const std::string &appname, const std::string &version)
{
	return AppID::parse(ubuntu_app_launch_triplet_to_app_id(package.c_str(), appname.c_str(), version.c_str()));
}

AppID
AppID::discover (const std::string &package, ApplicationWildcard appwildcard)
{
	return AppID::discover(package, appwildcard, VersionWildcard::CURRENT_USER_VERSION);
}

AppID
AppID::discover (const std::string &package, ApplicationWildcard appwildcard, VersionWildcard versionwildcard)
{
	return discover(package, app_wildcard(appwildcard), ver_wildcard(versionwildcard));
}

AppID
AppID::discover (const std::string &package, const std::string &appname, VersionWildcard versionwildcard)
{
	return discover(package, appname, ver_wildcard(versionwildcard));
}


}; // namespace AppLaunch
}; // namespace Ubuntu
