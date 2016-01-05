
#include "application-impl-base.h"
#include <gio/gdesktopappinfo.h>

#pragma once

namespace Ubuntu {
namespace AppLaunch {
namespace AppImpls {

class Libertine : public Base {
public:
	Libertine (const AppID::Package &container,
	      const AppID::AppName &appname,
	      std::shared_ptr<Registry> registry);

	static std::list<std::shared_ptr<Application>> list (std::shared_ptr<Registry> registry);

	AppID appId() override {
		return {
			package: _container,
			appname: _appname,
			version: AppID::Version::from_raw("0.0")
		};
	}

	std::shared_ptr<Info> info() override;

private:
	AppID::Package _container;
	AppID::AppName _appname;
	std::shared_ptr<GDesktopAppInfo> _appinfo;
};

}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
