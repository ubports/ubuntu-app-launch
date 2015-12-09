
extern "C" {
#include "app-info.h"
#include "ubuntu-app-launch.h"
}

#include "application.h"
#include "application-impl-click.h"
#include "application-impl-legacy.h"
#include "application-impl-libertine.h"

namespace Ubuntu {
namespace AppLaunch {

Application::Application (const std::string &package,
	             const std::string &appname,
	             const std::string &version,
	             std::shared_ptr<Connection> connection)
{
	auto appid = package + "_" + appname + "_" + version;
	if (app_info_legacy(appid.c_str(), NULL, NULL)) {
		impl = std::unique_ptr<AppImpls::Legacy>(new AppImpls::Legacy(package, appname, version, connection));
	} else if (app_info_click(appid.c_str(), NULL, NULL)) {
		impl = std::unique_ptr<AppImpls::Click>(new AppImpls::Click(package, appname, version, connection));
	} else if (app_info_libertine(appid.c_str(), NULL, NULL)) {
		impl = std::unique_ptr<AppImpls::Libertine>(new AppImpls::Libertine(package, appname, version, connection));
	}
}

const std::string &
Application::package()
{
	return impl->package();
}

const std::string &
Application::appname()
{
	return impl->appname();
}

const std::string &
Application::version()
{
	return impl->version();
}


const std::string &
Application::logPath()
{
	return impl->logPath();
}


/* Package provided user visible info */
const std::string &
Application::name()
{
	return impl->name();
}

const std::string &
Application::description()
{
	return impl->description();
}

const std::string &
Application::iconPath()
{
	return impl->iconPath();
}

std::list<std::string> 
Application::categories()
{
	return impl->categories();
}


bool 
Application::isRunning()
{
	return primaryPid() == 0;
}

pid_t 
Application::primaryPid()
{
	return ubuntu_app_launch_get_primary_pid(impl->appId().c_str());
}

bool 
Application::hasPid(pid_t pid)
{
	return ubuntu_app_launch_pid_in_app_id(pid, impl->appId().c_str()) == TRUE;
}

void 
Application::launch(std::list<std::string> urls)
{
	// TODO URLs
	ubuntu_app_launch_start_application(impl->appId().c_str(), NULL);
}

void 
Application::pause()
{
	ubuntu_app_launch_pause_application(impl->appId().c_str());
}

void 
Application::resume()
{
	ubuntu_app_launch_resume_application(impl->appId().c_str());
}


}; // namespace AppLaunch
}; // namespace Ubuntu
