
#include <list>
#include <memory>
#include <functional>

#pragma once

namespace Ubuntu {
namespace AppLaunch {

class Application;
class ObserverHandle;

class Connection {
public:
	enum FailureTypes {
		CRASH,
		START_FAILURE,
	};

	Connection();

	/* Lots of application lists */
	std::list<std::shared_ptr<Application>> runningApps();
	std::list<std::shared_ptr<Application>> installedApps();

	/* Observers, NOTE: All functions called on a different thread */
	typedef std::function<void(const std::string& appid)> appObserver;
	typedef std::function<void(const std::string& appid, FailureTypes reason)> appFailedObserver;

	ObserverHandle observeAppStarting (appObserver callback);
	ObserverHandle observeAppStarted (appObserver callback);
	ObserverHandle observeAppStopped (appObserver callback);
	ObserverHandle observeAppFailed (appFailedObserver callback);
	ObserverHandle observeAppFocus (appObserver callback);
	ObserverHandle observeAppResume (appObserver callback);
	ObserverHandle observeAppResumed (appObserver callback);

	static std::shared_ptr<Connection> getDefault();
};

}; // namespace AppLaunch
}; // namespace Ubuntu
