
#include "application-impl-click.h"
#include "registry-impl.h"
#include "application-info-desktop.h"

namespace Ubuntu {
namespace AppLaunch {
namespace AppImpls {

Click::Click (const std::string &package,
	  const std::string &appname,
	  const std::string &version,
	  std::shared_ptr<Registry> registry) :
	Click(package, appname, version, registry->impl->getClickManifest(package), registry)
{
}

Click::Click (const std::string &package,
	  const std::string &appname,
	  const std::string &version,
	  std::shared_ptr<JsonObject> manifest,
	  std::shared_ptr<Registry> registry) :
	Base(registry),
	_package(package),
	_appname(appname),
	_version(version),
	_manifest(manifest),
	_clickDir(registry->impl->getClickDir(package)),
	_appinfo(manifestAppDesktop(manifest, appname, _clickDir))
{

}

const std::string &
Click::package()
{
	return _package;
}

const std::string &
Click::appname()
{
	return _appname;
}

const std::string &
Click::version()
{
	return _version;
}

std::shared_ptr<Application::Info>
Click::info (void)
{
	return std::make_shared<AppInfo::Desktop>(_appinfo, _clickDir);
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
Click::list (std::shared_ptr<Registry> registry)
{
	std::list<std::shared_ptr<Application>> applist;

	for (auto pkg : registry->impl->getClickPackages()) {
		auto manifest = registry->impl->getClickManifest(pkg);

		for (auto appname : manifestApps(manifest)) {
			auto app = std::make_shared<Click>(pkg, appname, manifestVersion(manifest), manifest, registry);
			applist.push_back(app);
		}
	}

	return applist;
}

}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
