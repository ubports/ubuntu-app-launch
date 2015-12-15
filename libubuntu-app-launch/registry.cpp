
#include "registry.h"
#include "registry-impl.h"

#include "application-impl-click.h"
#include "application-impl-legacy.h"
#include "application-impl-libertine.h"

namespace Ubuntu {
namespace AppLaunch {

Registry::Registry ()
{

}

std::list<std::shared_ptr<Application>>
Registry::runningApps(std::shared_ptr<Registry> connection)
{
	auto strv = ubuntu_app_launch_list_running_apps();
	if (strv == nullptr) {
		return {};
	}

	std::list<std::shared_ptr<Application>> list;
	for (int i = 0; strv[i] != nullptr; i++) {
		gchar * package;
		gchar * appname;
		gchar * version;

		if (!ubuntu_app_launch_app_id_parse(strv[i], &package, &appname, &version)) {
			continue;
		}

		auto app = Application::create(package, appname, version, connection);
		list.push_back(app);

		g_free(package);
		g_free(appname);
		g_free(version);
	}

	return list;
}

std::list<std::shared_ptr<Application>>
Registry::installedApps(std::shared_ptr<Registry> connection)
{
	std::list<std::shared_ptr<Application>> list;

	list.splice(list.begin(), AppImpls::Click::list(connection));
	list.splice(list.begin(), AppImpls::Legacy::list(connection));
	list.splice(list.begin(), AppImpls::Libertine::list(connection));

	return list;
}


std::shared_ptr<Registry> defaultRegistry;
std::shared_ptr<Registry>
getDefault()
{
	if (!defaultRegistry) {
		defaultRegistry = std::make_shared<Registry>();
	}

	return defaultRegistry;
}

}; // namespace AppLaunch
}; // namespace Ubuntu
