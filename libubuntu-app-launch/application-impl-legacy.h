
#include <gio/gdesktopappinfo.h>

#include "application-impl-base.h"

#pragma once

namespace Ubuntu {
namespace AppLaunch {
namespace AppImpls {

class Legacy : public Base {
public:
	Legacy (const AppID::AppName &appname,
	        std::shared_ptr<Registry> registry);
	Legacy (const AppID::AppName &appname,
			std::shared_ptr<GDesktopAppInfo> appinfo,
	        std::shared_ptr<Registry> registry);

	AppID appId() override {
		return {
			package: AppID::Package::from_raw(""),
			appname: _appname,
			version: AppID::Version::from_raw("")
		};
	}

	std::shared_ptr<Info> info() override;

	static std::list<std::shared_ptr<Application>> list (std::shared_ptr<Registry> registry);

private:
	AppID::AppName _appname;
	std::shared_ptr<GDesktopAppInfo> _appinfo;
};

}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
