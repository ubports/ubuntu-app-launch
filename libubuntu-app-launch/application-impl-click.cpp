
#include "application-impl-click.h"

namespace Ubuntu {
namespace AppLaunch {
namespace AppImpls {

Click::Click (const std::string &package,
	  const std::string &appname,
	  const std::string &version,
	  std::shared_ptr<Connection> connection) :
	Impl(appname, version, package, connection)
{
}

const std::string&
Click::name () {
	return _name;
}

const std::string&
Click::description () {
	return _description;
}

const std::string&
Click::iconPath () {
	return _iconPath;
}

std::list<std::string>
Click::categories () {
	return {};
}


}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
