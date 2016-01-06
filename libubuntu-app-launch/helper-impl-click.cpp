
#include "helper-impl-click.h"

namespace Ubuntu {
namespace AppLaunch {
namespace HelperImpls {


bool
Click::hasInstances()
{
	return false;
}

std::vector<std::shared_ptr<Click::Instance>>
Click::instances()
{
	return {};
}

std::shared_ptr<Click::Instance>
Click::launch (std::vector<Helper::URL> urls)
{
	throw std::runtime_error("Not Implemented");
}

std::shared_ptr<Click::Instance>
Click::launch (MirPromptSession * session, std::vector<Helper::URL> urls)
{
	throw std::runtime_error("Not Implemented");
}

std::list<std::shared_ptr<Helper>>
Click::running(Helper::Type type, std::shared_ptr<Registry> registry)
{
	return {};
}








}; // namespace HelperImpl
}; // namespace AppLaunch
}; // namespace Ubuntu
