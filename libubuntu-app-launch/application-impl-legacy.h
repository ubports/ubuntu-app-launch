
#include <gio/gdesktopappinfo.h>

#include "application-impl-base.h"

#pragma once

namespace Ubuntu {
namespace AppLaunch {
namespace AppImpls {

class Legacy : public Base {
public:
	Legacy (const Application::AppName &appname,
	        std::shared_ptr<Registry> registry);
	Legacy (const Application::AppName &appname,
			std::shared_ptr<GDesktopAppInfo> appinfo,
	        std::shared_ptr<Registry> registry);

	const Application::Package &package() override {
		static Application::Package nullpkg = Application::Package::from_raw("");
		return nullpkg;
	}

	const Application::AppName &appname() override {
		return _appname;
	}

	const Application::Version &version() override {
		static Application::Version nullver = Application::Version::from_raw("");
		return nullver;
	}

	Application::AppID appId () override {
		return Application::AppID::from_raw(_appname);
	}

	std::shared_ptr<Info> info() override;

	static std::list<std::shared_ptr<Application>> list (std::shared_ptr<Registry> registry);

private:
	std::shared_ptr<GDesktopAppInfo> _appinfo;
	Application::AppName _appname;
};

}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
