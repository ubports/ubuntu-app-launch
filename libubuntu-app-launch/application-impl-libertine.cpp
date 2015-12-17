
#include "application-impl-libertine.h"
#include "application-info-desktop.h"
#include "libertine.h"

namespace Ubuntu {
namespace AppLaunch {
namespace AppImpls {

Libertine::Libertine (const Application::Package &container,
	  const Application::AppName &appname,
	  std::shared_ptr<Registry> registry) :
	Base(registry),
	_container(container),
	_appname(appname)
{
}

const Application::Package &
Libertine::package()
{
	return _container;
}

const Application::AppName &
Libertine::appname()
{
	return _appname;
}

const Application::Version &
Libertine::version()
{
	static auto zero = Application::Version::from_raw("0.0");
	return zero;
}

std::list<std::shared_ptr<Application>>
Libertine::list (std::shared_ptr<Registry> registry)
{
	return {};
}

std::shared_ptr<Application::Info>
Libertine::info (void)
{
	return std::make_shared<AppInfo::Desktop>(_appinfo, libertine_container_path(_container.value().c_str()));
}

}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
