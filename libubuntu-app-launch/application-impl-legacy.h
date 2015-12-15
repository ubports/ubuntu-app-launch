
#include <gio/gdesktopappinfo.h>

#include "application-impl-base.h"

#pragma once

namespace Ubuntu {
namespace AppLaunch {
namespace AppImpls {

class Legacy : public Base {
public:
	Legacy (const std::string &appname,
	        std::shared_ptr<Registry> registry);
	Legacy (const std::string &appname,
			std::shared_ptr<GDesktopAppInfo> appinfo,
	        std::shared_ptr<Registry> registry);


	const std::string &package() override {
		return _appname;
	}

	const std::string &appname() override {
		static std::string nullstring;
		return nullstring;
	}

	const std::string &version() override {
		static std::string nullstring;
		return nullstring;
	}

	std::string appId () override {
		return _appname;
	}

	std::shared_ptr<Info> info() override;

	static std::list<std::shared_ptr<Application>> list (std::shared_ptr<Registry> registry);

private:
	std::shared_ptr<GDesktopAppInfo> _appinfo;
	std::string _appname;
};

}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
