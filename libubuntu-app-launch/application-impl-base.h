
#include "application.h"

extern "C" {
#include "ubuntu-app-launch.h"
}

#pragma once

namespace Ubuntu {
namespace AppLaunch {
namespace AppImpls {

class Base : public Ubuntu::AppLaunch::Application {
public:
	Base (std::shared_ptr<Registry> registry);

	Application::AppID appId () override;

	bool hasInstances() override;
	std::vector<std::shared_ptr<Instance>> instances() override;

	std::shared_ptr<Instance> launch(std::vector<Application::URL> urls = {}) override;
protected:
	std::shared_ptr<Registry> _registry;
};

}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
