
#include <sys/types.h>
#include <vector>
#include <memory>

#include "connection.h"

#pragma once

namespace Ubuntu {
namespace AppLaunch {

class Application {
public:
	static std::shared_ptr<Application> create (const std::string &package,
	                                            const std::string &appname,
	                                            const std::string &version,
	                                            std::shared_ptr<Connection> connection = Connection::getDefault());

	/* System level info */
	virtual const std::string &package() = 0;
	virtual const std::string &appname() = 0;
	virtual const std::string &version() = 0;
	virtual std::string appId() = 0;

	class Info {
		/* Package provided user visible info */
		virtual const std::string &name() = 0;
		virtual const std::string &description() = 0;
		virtual const std::string &iconPath() = 0;
		virtual std::list<std::string> categories() = 0;
	};

	virtual std::shared_ptr<Info> info() = 0;

	class Instance {
		/* Query lifecycle */
		virtual bool isRunning() = 0;
		virtual pid_t primaryPid() = 0;
		virtual bool hasPid(pid_t pid) = 0;
		virtual const std::string &logPath() = 0;

		/* Manage lifecycle */
		virtual void pause() = 0;
		virtual void resume() = 0;
		virtual void stop() = 0;
	};

	virtual bool hasInstances() = 0;
	virtual std::vector<std::shared_ptr<Instance>> instances() = 0;

	virtual std::shared_ptr<Instance> launch(std::vector<std::string> urls = {}) = 0;
};

}; // namespace AppLaunch
}; // namespace Ubuntu
