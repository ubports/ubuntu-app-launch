
#include <sys/types.h>

#include "connection.h"

#pragma once

namespace Ubuntu {
namespace AppLaunch {

class Application {
public:
	Application (const std::string &package,
	             const std::string &appname,
	             const std::string &version,
	             std::shared_ptr<Connection> connection = Connection::getDefault());

	/* System level info */
	const std::string &package();
	const std::string &appname();
	const std::string &version();

	const std::string &logPath();

	/* Package provided user visible info */
	const std::string &name();
	const std::string &description();
	const std::string &iconPath();
	std::list<std::string> categories();

	/* Query lifecycle */
	bool isRunning();
	pid_t primaryPid();
	bool hasPid(pid_t pid);

	/* Manage lifecycle */
	void launch(std::list<std::string> urls = {});
	void pause();
	void resume();

private:
	class Impl;
	std::unique_ptr<Impl> impl;
};

}; // namespace AppLaunch
}; // namespace Ubuntu
