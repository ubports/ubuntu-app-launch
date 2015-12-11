
#include <gio/gdesktopappinfo.h>
#include <json-glib/json-glib.h>
#include "application-impl.h"

#pragma once

namespace Ubuntu {
namespace AppLaunch {
namespace AppImpls {

class Click : public Application::Impl {
public:
	Click (const std::string &package,
	      const std::string &appname,
	      const std::string &version,
	      std::shared_ptr<Connection> connection);
	Click (const std::string &package,
	      const std::string &appname,
	      const std::string &version,
	      std::shared_ptr<JsonObject> manifest,
	      std::shared_ptr<Connection> connection);

	const std::string &name() override;
	const std::string &description() override;
	const std::string &iconPath() override;
	std::list<std::string> categories() override;

	static std::list<std::shared_ptr<Application>> list (std::shared_ptr<Connection> connection);

private:
	std::string _name;
	std::string _description;
	std::string _iconPath;
	std::shared_ptr<JsonObject> _manifest;

	std::string _clickDir;
	std::shared_ptr<GDesktopAppInfo> _appinfo;

	static std::string manifestVersion (std::shared_ptr<JsonObject> manifest);
	static std::list<std::string> manifestApps (std::shared_ptr<JsonObject> manifest);
	static std::shared_ptr<GDesktopAppInfo> manifestAppDesktop (std::shared_ptr<JsonObject> manifest, const std::string &app, const std::string &clickDir);
};

}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
