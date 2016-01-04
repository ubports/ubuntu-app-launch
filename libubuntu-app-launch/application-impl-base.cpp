
#include "application-impl-base.h"

namespace Ubuntu {
namespace AppLaunch {
namespace AppImpls {

Base::Base (std::shared_ptr<Registry> registry) :
	_registry(registry)
{
}

Application::AppID
Base::appId (void)
{
	return Application::AppID::from_raw(package().value() + "_" + appname().value() + "_" + version().value());
}

bool
Base::hasInstances()
{
	return ubuntu_app_launch_get_primary_pid(appId().value().c_str()) != 0;
}

class BaseInstance : public Application::Instance {
public:
	BaseInstance(const std::string &appId);

	/* Query lifecycle */
	bool isRunning() override {
		return ubuntu_app_launch_get_primary_pid(_appId.c_str()) != 0;
	}
	pid_t primaryPid() override {
		return ubuntu_app_launch_get_primary_pid(_appId.c_str());
	}
	bool hasPid(pid_t pid) override {
		return ubuntu_app_launch_pid_in_app_id(pid, _appId.c_str()) == TRUE;
	}
	const std::string &logPath() override {
		/* TODO: Log path */
		static std::string nullstr("");
		return nullstr;
	}
	std::vector<pid_t> pids () override {
		std::vector<pid_t> vector;
		GList * list = ubuntu_app_launch_get_pids(_appId.c_str());
		
		for (GList * pntr = list; pntr != nullptr; pntr = g_list_next(pntr)) {
			vector.push_back(static_cast<pid_t>(GPOINTER_TO_INT(list->data)));
		}

		g_list_free(list);

		return vector;
	}

	/* Manage lifecycle */
	void pause() override {
		ubuntu_app_launch_pause_application(_appId.c_str());
	}
	void resume() override {
		ubuntu_app_launch_resume_application(_appId.c_str());
	}
	void stop() override {
		ubuntu_app_launch_stop_application(_appId.c_str());
	}

private:
	std::string _appId;
};

BaseInstance::BaseInstance (const std::string &appId) :
	_appId(appId)
{
}

std::vector<std::shared_ptr<Application::Instance>>
Base::instances()
{
	std::vector<std::shared_ptr<Instance>> vect;
	vect.emplace_back(std::make_shared<BaseInstance>(appId()));
	return vect;
}

std::shared_ptr<Application::Instance>
Base::launch(std::vector<Application::URL> urls)
{
	// TODO URLs
	ubuntu_app_launch_start_application(appId().value().c_str(), NULL);
	return std::make_shared<BaseInstance>(appId());
}


}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
