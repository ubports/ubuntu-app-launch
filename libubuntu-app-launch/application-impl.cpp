
#include "application-impl.h"

namespace Ubuntu {
namespace AppLaunch {

Application::Impl::Impl (const std::string &package,
	      const std::string &appname,
	      const std::string &version,
	      std::shared_ptr<Connection> connection) {
	_package = package;
	_appname = appname;
	_version = version;
	_connection = connection;
}

Application::Impl::~Impl () {
}

}; // namespace AppLaunch
}; // namespace Ubuntu
