
#include "application-impl-libertine.h"

namespace Ubuntu {
namespace AppLaunch {
namespace AppImpls {

Libertine::Libertine (const std::string &package,
	  const std::string &appname,
	  const std::string &version,
	  std::shared_ptr<Connection> connection) :
	Impl(appname, version, package, connection)
{
}

const std::string&
Libertine::name () {
	return _name;
}

const std::string&
Libertine::description () {
	return _description;
}

const std::string&
Libertine::iconPath () {
	return _iconPath;
}

std::list<std::string>
Libertine::categories () {
	return {};
}


}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
