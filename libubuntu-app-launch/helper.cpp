
#include "helper.h"

#include "helper-impl-click.h"

namespace Ubuntu {
namespace AppLaunch {

std::shared_ptr<Helper>
Helper::create (Type type, AppID appid, std::shared_ptr<Registry> registry)
{
	/* Only one type today */
	return std::make_shared<HelperImpls::Click>(type, appid, registry);
}

}; // namespace AppLaunch
}; // namespace Ubuntu
