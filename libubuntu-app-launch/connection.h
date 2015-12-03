
namespace Ubuntu {
namespace AppLaunch {

class Connection {
	Connection();

	/* Lots of application lists */
	std::list<Application::Ptr> runningApps();
	std::list<Application::Ptr> installedApps();

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

	enum FailureTypes {
		CRASH,
		START_FAILURE,
	};

	typedef std::shared_ptr<Connection> Ptr;
private:
	class Impl;
	std::static_ptr<Impl> impl;
};

}; // namespace AppLaunch
}; // namespace Ubuntu
