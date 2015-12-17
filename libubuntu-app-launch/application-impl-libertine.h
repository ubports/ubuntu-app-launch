
#include "application-impl-base.h"
#include <gio/gdesktopappinfo.h>

#pragma once

namespace Ubuntu {
namespace AppLaunch {
namespace AppImpls {

class Libertine : public Base {
public:
	Libertine (const Application::Package &container,
	      const Application::AppName &appname,
	      std::shared_ptr<Registry> registry);

	static std::list<std::shared_ptr<Application>> list (std::shared_ptr<Registry> registry);

	const Application::Package &package() override;
	const Application::AppName &appname() override;
	const Application::Version &version() override;

	std::shared_ptr<Info> info() override;

private:
	Application::Package _container;
	Application::AppName _appname;
	std::shared_ptr<GDesktopAppInfo> _appinfo;
};

}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
