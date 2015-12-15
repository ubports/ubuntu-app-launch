
#include "application-impl-base.h"

namespace Ubuntu {
namespace AppLaunch {
namespace AppImpls {

Base::Base (std::shared_ptr<Connection> connection) :
	_connection(connection)
{
}

std::string
Base::appId (void)
{
	return package() + "_" + appname() + "_" + version();
}

bool
Base::hasInstances()
{
	return ubuntu_app_launch_get_primary_pid(appId().c_str()) != 0;
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
Base::launch(std::vector<std::string> urls)
{
	// TODO URLs
	ubuntu_app_launch_start_application(appId().c_str(), NULL);
	return std::make_shared<BaseInstance>(appId());
}


}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
