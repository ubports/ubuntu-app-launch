
#include <list>
#include <memory>
#include <functional>
#include <core/signal.h>

#include "application.h"
#include "helper.h"

#pragma once
#pragma GCC visibility push(default)

namespace Ubuntu {
namespace AppLaunch {

class Registry {
public:
	enum FailureType {
		CRASH,
		START_FAILURE,
	};

	Registry();
	virtual ~Registry();

	/* Lots of application lists */
	static std::list<std::shared_ptr<Application>> runningApps(std::shared_ptr<Registry> registry = getDefault());
	static std::list<std::shared_ptr<Application>> installedApps(std::shared_ptr<Registry> registry = getDefault());

	/* Signals to discover what is happening to apps */
	core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>> appStarted;
	core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>> appStopped;
	core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>, FailureType> appFailed;
	core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>> appPaused;
	core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>> appResumed;

	/* The Application Manager, almost always if you're not Unity8, don't
	   use this API. Testing is a special case. */
	class Manager {
		virtual bool focusRequest (std::shared_ptr<Application> app, std::shared_ptr<Application::Instance> instance) = 0;
		virtual bool startingRequest (std::shared_ptr<Application> app, std::shared_ptr<Application::Instance> instance) = 0;
	
	  protected:
	    Manager() = default;
	};

	void setManager (Manager *manager);
	void clearManager ();

	/* Helper Lists */
	std::vector<std::shared_ptr<Helper>> runningHelpers (Helper::Type type);

	/* Default Junk */
	static std::shared_ptr<Registry> getDefault();
	static void clearDefault();

	/* Hide our implementation */
	class Impl;
	std::unique_ptr<Impl> impl;
};

}; // namespace AppLaunch
}; // namespace Ubuntu

#pragma GCC visibility pop
