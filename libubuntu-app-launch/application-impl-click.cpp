
#include "application-impl-click.h"
#include "connection-impl.h"

namespace Ubuntu {
namespace AppLaunch {
namespace AppImpls {

Click::Click (const std::string &package,
	  const std::string &appname,
	  const std::string &version,
	  std::shared_ptr<Connection> connection) :
	Click(package, appname, version, connection->impl->getClickManifest(package), connection)
{
}

Click::Click (const std::string &package,
	  const std::string &appname,
	  const std::string &version,
	  std::shared_ptr<JsonObject> manifest,
	  std::shared_ptr<Connection> connection) :
	Impl(package, appname, version, connection),
	_manifest(manifest)
{
}

const std::string&
Click::name () {
	return _name;
}

const std::string&
Click::description () {
	return _description;
}

const std::string&
Click::iconPath () {
	return _iconPath;
}

std::list<std::string>
Click::categories () {
	return {};
}

std::string
Click::manifestVersion (std::shared_ptr<JsonObject> manifest)
{
	auto cstr = json_object_get_string_member(manifest.get(), "version");

	if (cstr == nullptr) {
		return {};
	}

	std::string cppstr((const gchar *)cstr);
	return cppstr;
}

std::list<std::string>
Click::manifestApps (std::shared_ptr<JsonObject> manifest)
{
	auto hooks = json_object_get_object_member(manifest.get(), "hooks");
	if (hooks == nullptr) {
		return {};
	}

	auto gapps = json_object_get_members(hooks);
	if (gapps == nullptr) {
		return {};
	}
	
	std::list<std::string> apps;

	for (GList * item = gapps; item != nullptr; item = g_list_next(item)) {
		apps.emplace_back(std::string((gchar *)item->data));
	}

	g_list_free_full(gapps, g_free);
	return apps;
}

std::list<std::shared_ptr<Application>>
Click::list (std::shared_ptr<Connection> connection)
{
	std::list<std::shared_ptr<Application>> applist;

	for (auto pkg : connection->impl->getClickPackages()) {
		auto manifest = connection->impl->getClickManifest(pkg);

		for (auto appname : manifestApps(manifest)) {
			auto pclick = new Click(pkg, appname, manifestVersion(manifest), manifest, connection);
			auto app = std::make_shared<Application>(std::unique_ptr<Application::Impl>(pclick));
			applist.push_back(app);
		}
	}

	return applist;
}

}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
