
#include "application-impl.h"

#pragma once

namespace Ubuntu {
namespace AppLaunch {

class ImplLibertine : public Application::Impl {
public:
	ImplLibertine (const std::string &package,
	      const std::string &appname,
	      const std::string &version,
	      std::shared_ptr<Connection> connection) :
		Impl(appname, version, package, connection)
	{
    }

	const std::string &logPath();
	const std::string &name();
	const std::string &description();
	const std::string &iconPath();
	std::list<std::string> categories();
};

}; // namespace AppLaunch
}; // namespace Ubuntu
