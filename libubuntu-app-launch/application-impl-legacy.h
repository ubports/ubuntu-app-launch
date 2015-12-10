
#include <gio/gdesktopappinfo.h>

#include "application-impl.h"

#pragma once

namespace Ubuntu {
namespace AppLaunch {
namespace AppImpls {

class Legacy : public Application::Impl {
public:
	Legacy (const std::string &appname,
	        std::shared_ptr<Connection> connection);
	Legacy (const std::string &appname,
			std::shared_ptr<GDesktopAppInfo> appinfo,
	        std::shared_ptr<Connection> connection);

	const std::string &name();
	const std::string &description();
	const std::string &iconPath();
	std::list<std::string> categories();

	std::string appId () override {
		return _package;
	}

	static std::list<std::shared_ptr<Application>> list (std::shared_ptr<Connection> connection);

private:
	std::shared_ptr<GDesktopAppInfo> _appinfo;
	std::string _name;
	std::string _description;
	std::string _iconPath;
};

}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
