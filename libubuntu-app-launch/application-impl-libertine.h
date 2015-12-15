
#include "application-impl-base.h"
#include <gio/gdesktopappinfo.h>

#pragma once

namespace Ubuntu {
namespace AppLaunch {
namespace AppImpls {

class Libertine : public Base {
public:
	Libertine (const std::string &container,
	      const std::string &appname,
	      std::shared_ptr<Connection> connection);

	static std::list<std::shared_ptr<Application>> list (std::shared_ptr<Connection> connection);

	const std::string &package() override;
	const std::string &appname() override;
	const std::string &version() override;

	std::shared_ptr<Info> info() override;

private:
	std::string _container;
	std::string _appname;
	std::shared_ptr<GDesktopAppInfo> _appinfo;
};

}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
