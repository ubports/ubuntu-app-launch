
#include "application-impl.h"

#pragma once

namespace Ubuntu {
namespace AppLaunch {
namespace AppImpls {

class Libertine : public Application::Impl {
public:
	Libertine (const std::string &package,
	      const std::string &appname,
	      const std::string &version,
	      std::shared_ptr<Connection> connection);

	const std::string &name();
	const std::string &description();
	const std::string &iconPath();
	std::list<std::string> categories();

private:
	std::string _name;
	std::string _description;
	std::string _iconPath;
};

}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
