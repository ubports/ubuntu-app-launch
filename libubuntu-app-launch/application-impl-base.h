
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

	std::string appId () override;

	bool hasInstances() override;
	std::vector<std::shared_ptr<Instance>> instances() override;

	std::shared_ptr<Instance> launch(std::vector<std::string> urls = {}) override;
protected:
	std::shared_ptr<Registry> _registry;
};

}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
