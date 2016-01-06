
#include "registry-impl.h"
#include "helper-impl-click.h"

#include "ubuntu-app-launch.h"

namespace Ubuntu {
namespace AppLaunch {
namespace HelperImpls {


bool
Click::hasInstances()
{
	return _registry->impl->thread.executeOnThread<bool>([this] () {
		auto instances = ubuntu_app_launch_list_helper_instances(_type.value().c_str(), ((std::string)_appid).c_str());
		auto retval = (g_strv_length(instances) != 0);

		g_strfreev(instances);

		return retval;
	});
}

class ClickInstance : public Helper::Instance {
public: /* all one file, no one to hide from */
	AppID _appid;
	Helper::Type _type;
	std::string _instanceid;
	std::shared_ptr<Registry> _registry;

	ClickInstance (const AppID& appid, const Helper::Type &type, const std::string &instanceid, std::shared_ptr<Registry> registry) :
		_appid(appid),
		_type(type),
		_instanceid(instanceid),
		_registry(registry)
	{
	}

	bool isRunning() override {
		return _registry->impl->thread.executeOnThread<bool>([this] () {
			bool found = false;

			auto instances = ubuntu_app_launch_list_helper_instances(_type.value().c_str(), ((std::string)_appid).c_str());
			for (int i = 0; instances[i] != nullptr; i++) {
				if (_instanceid == std::string(instances[i])) {
					found = true;
					break;
				}
			}

			g_strfreev(instances);

			return found;
		});
	}

	void stop() override {
		_registry->impl->thread.executeOnThread<bool>([this] () {
			return ubuntu_app_launch_stop_multiple_helper(_type.value().c_str(), ((std::string)_appid).c_str(), _instanceid.c_str()) == TRUE;
		});
	}
};

std::vector<std::shared_ptr<Click::Instance>>
Click::instances()
{
	return _registry->impl->thread.executeOnThread<std::vector<std::shared_ptr<Click::Instance>>>([this] () -> std::vector<std::shared_ptr<Click::Instance>> {
		std::vector<std::shared_ptr<Click::Instance>> vect;
		auto instances = ubuntu_app_launch_list_helper_instances(_type.value().c_str(), ((std::string)_appid).c_str());
		for (int i = 0; instances[i] != nullptr; i++) {
			auto inst = std::make_shared<ClickInstance>(_appid, _type, instances[i], _registry);
			vect.push_back(inst);
		}

		g_strfreev(instances);

		return vect;
	});
}

std::shared_ptr<Click::Instance>
Click::launch (std::vector<Helper::URL> urls)
{
	return _registry->impl->thread.executeOnThread<std::shared_ptr<Click::Instance>>([this, urls] () {
		/* TODO: URLS */
		auto instanceid = ubuntu_app_launch_start_multiple_helper(_type.value().c_str(), ((std::string)_appid).c_str(), nullptr);

		return std::make_shared<ClickInstance>(_appid, _type, instanceid, _registry);
	});
}

std::shared_ptr<Click::Instance>
Click::launch (MirPromptSession * session, std::vector<Helper::URL> urls)
{
	return _registry->impl->thread.executeOnThread<std::shared_ptr<Click::Instance>>([this, session, urls] () {
		/* TODO: URLS */
		auto instanceid = ubuntu_app_launch_start_session_helper(_type.value().c_str(), session, ((std::string)_appid).c_str(), nullptr);

		return std::make_shared<ClickInstance>(_appid, _type, instanceid, _registry);
	});
}

std::list<std::shared_ptr<Helper>>
Click::running(Helper::Type type, std::shared_ptr<Registry> registry)
{
	return registry->impl->thread.executeOnThread<std::list<std::shared_ptr<Helper>>>([type, registry] () {
		std::list<std::shared_ptr<Helper>> helpers;

		auto appidv = ubuntu_app_launch_list_helpers(type.value().c_str());
		for (int i = 0; appidv[i] != nullptr; i++) {
			auto helper = std::make_shared<Click>(type, AppID::parse(appidv[i]), registry);
			helpers.push_back(helper);
		}

		g_strfreev(appidv);

		return helpers;
	});
}








}; // namespace HelperImpl
}; // namespace AppLaunch
}; // namespace Ubuntu
