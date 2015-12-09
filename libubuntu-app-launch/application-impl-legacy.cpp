
#include "application-impl-legacy.h"

namespace Ubuntu {
namespace AppLaunch {
namespace AppImpls {

Legacy::Legacy (const std::string &package,
	  const std::string &appname,
	  const std::string &version,
	  std::shared_ptr<Connection> connection) :
	Impl(appname, version, package, connection)
{
}

const std::string&
Legacy::name () {
	return _name;
}

const std::string&
Legacy::description () {
	return _description;
}

const std::string&
Legacy::iconPath () {
	return _iconPath;
}

std::list<std::string>
Legacy::categories () {
	return {};
}


}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
