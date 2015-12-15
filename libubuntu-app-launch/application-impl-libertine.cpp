
#include "application-impl-libertine.h"
#include "application-info-desktop.h"
#include "libertine.h"

namespace Ubuntu {
namespace AppLaunch {
namespace AppImpls {

Libertine::Libertine (const std::string &container,
	  const std::string &appname,
	  std::shared_ptr<Connection> connection) :
	Base(connection),
	_container(container),
	_appname(appname)
{
}

const std::string &
Libertine::package()
{
	return _container;
}

const std::string &
Libertine::appname()
{
	return _appname;
}

const std::string &
Libertine::version()
{
	static std::string zero("0.0");
	return zero;
}

std::list<std::shared_ptr<Application>>
Libertine::list (std::shared_ptr<Connection> connection)
{
	return {};
}

std::shared_ptr<Application::Info>
Libertine::info (void)
{
	return std::make_shared<AppInfo::Desktop>(_appinfo, libertine_container_path(_container.c_str()));
}

}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
