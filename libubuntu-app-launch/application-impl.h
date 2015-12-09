
#include "application.h"

#pragma once

namespace Ubuntu {
namespace AppLaunch {

class Application::Impl {
public:
	Impl (const std::string &package,
	      const std::string &appname,
	      const std::string &version,
	      std::shared_ptr<Connection> connection) {
		_package = package;
		_appname = appname;
		_version = version;
		_connection = connection;
    }

	const std::string &package() {
		return _package;
	}

	const std::string &appname() {
		return _appname;
	}

	const std::string &version() {
		return _version;
	}

	const std::string &logPath();
	const std::string &name();
	const std::string &description();
	const std::string &iconPath();
	std::list<std::string> categories();

protected:
	std::string _package;
	std::string _appname;
	std::string _version;
	std::shared_ptr<Connection> _connection;
};

}; // namespace AppLaunch
}; // namespace Ubuntu
