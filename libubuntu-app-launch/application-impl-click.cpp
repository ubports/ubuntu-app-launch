
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
	_manifest(manifest),
	_clickDir(connection->impl->getClickDir(package)),
	_appinfo(manifestAppDesktop(manifest, appname, _clickDir))
{

}

const std::string&
Click::name () {
	if (_name.empty()) {
		_name = g_app_info_get_display_name(G_APP_INFO(_appinfo.get()));
	}
	return _name;
}

const std::string&
Click::description () {
	if (_description.empty()) {
		_description = g_app_info_get_description(G_APP_INFO(_appinfo.get()));
	}
	return _description;
}

const std::string&
Click::iconPath () {
	if (_iconPath.empty()) {
		auto relative = std::shared_ptr<gchar>(g_desktop_app_info_get_string(_appinfo.get(), "Icon"), [](gchar * str) { g_clear_pointer(&str, g_free); }); 
		auto cpath = std::shared_ptr<gchar>(g_build_filename(_clickDir.c_str(), relative.get(), nullptr), [](gchar * str) { g_clear_pointer(&str, g_free); });
		_iconPath = cpath.get();
	}
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
		auto appname = (const gchar *)item->data;

		auto hooklist = json_object_get_object_member(manifest.get(), appname);

		if (json_object_has_member(hooklist, "desktop") == TRUE)
			apps.emplace_back(std::string(appname));
	}

	g_list_free_full(gapps, g_free);
	return apps;
}

std::shared_ptr<GDesktopAppInfo>
Click::manifestAppDesktop (std::shared_ptr<JsonObject> manifest, const std::string &app, const std::string &clickDir)
{
	auto hooks = json_object_get_object_member(manifest.get(), "hooks");
	if (hooks == nullptr) {
		return {};
	}

	auto gapps = json_object_get_members(hooks);
	if (gapps == nullptr) {
		return {};
	}

	auto hooklist = json_object_get_object_member(manifest.get(), app.c_str());
	if (hooklist == nullptr) {
		return {};
	}

	auto desktoppath = json_object_get_string_member(hooklist, "desktop");
	if (desktoppath == nullptr) {
		return {};
	}

	auto path = std::shared_ptr<gchar>(g_build_filename(clickDir.c_str(), desktoppath, nullptr), g_free);

	std::shared_ptr<GKeyFile> keyfile(g_key_file_new(), g_key_file_free);
	GError * error = nullptr;
	g_key_file_load_from_file(keyfile.get(), path.get(), G_KEY_FILE_NONE, &error);
	if (error != nullptr) {
		auto perror = std::shared_ptr<GError>(error, g_error_free);
		throw std::runtime_error(error->message);
	}

	std::shared_ptr<GDesktopAppInfo> appinfo(g_desktop_app_info_new_from_keyfile(keyfile.get()), [](GDesktopAppInfo * appinfo) { g_clear_object(&appinfo); });
	return appinfo;
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
