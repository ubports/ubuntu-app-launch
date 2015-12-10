
#include "application-impl.h"

#pragma once

namespace Ubuntu {
namespace AppLaunch {
namespace AppImpls {

class Click : public Application::Impl {
public:
	Click (const std::string &package,
	      const std::string &appname,
	      const std::string &version,
	      std::shared_ptr<Connection> connection);

	const std::string &name() override;
	const std::string &description() override;
	const std::string &iconPath() override;
	std::list<std::string> categories() override;

	static std::list<std::shared_ptr<Application>> list (std::shared_ptr<Connection> connection);

private:
	std::string _name;
	std::string _description;
	std::string _iconPath;
};

}; // namespace AppImpls
}; // namespace AppLaunch
}; // namespace Ubuntu
