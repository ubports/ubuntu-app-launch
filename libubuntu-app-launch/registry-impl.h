
#include "registry.h"
#include "glib-thread.h"

#include <json-glib/json-glib.h>
#include <click.h>

#pragma once

namespace Ubuntu {
namespace AppLaunch {

class Registry::Impl {
public:
	Impl(Registry* registry);
	virtual ~Impl() {
		thread.quit();
	}

	std::shared_ptr<JsonObject> getClickManifest(const std::string& package);
	std::list<std::string> getClickPackages();
	std::string getClickDir(const std::string& package);

	void setManager (Registry::Manager* manager);
	void clearManager ();

private:
	Registry* _registry;
	Registry::Manager* _manager;

	GLib::ContextThread thread;

	std::shared_ptr<ClickDB> _clickDB;
	std::shared_ptr<ClickUser> _clickUser;

	void initClick ();
};

}; // namespace AppLaunch
}; // namespace Ubuntu
