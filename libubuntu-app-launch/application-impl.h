
#include "application.h"

extern "C" {
#include "ubuntu-app-launch.h"
}

#pragma once

namespace Ubuntu {
namespace AppLaunch {

class Application::Impl {
public:
	Impl (const std::string &package,
	      const std::string &appname,
	      const std::string &version,
	      std::shared_ptr<Connection> connection);
	virtual ~Impl ();

	const std::string &package() {
		return _package;
	}

	const std::string &appname() {
		return _appname;
	}

	const std::string &version() {
		return _version;
	}

	std::string appId () {
		return _package + "_" + _appname + "_" + _version;
	}

	const std::string &logPath() {
		if (_logPath.empty()) {
			_logPath = ubuntu_app_launch_application_log_path(appId().c_str());
		}

		return _logPath;
	}

	virtual const std::string &name() = 0;
	virtual const std::string &description() = 0;
	virtual const std::string &iconPath() = 0;
	virtual std::list<std::string> categories() = 0;

protected:
	std::string _package;
	std::string _appname;
	std::string _version;
	std::shared_ptr<Connection> _connection;

	std::string _logPath;
};

}; // namespace AppLaunch
}; // namespace Ubuntu
