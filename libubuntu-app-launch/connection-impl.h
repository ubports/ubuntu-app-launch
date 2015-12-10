
#include "connection.h"
#include "glib-thread.h"

#include <json-glib/json-glib.h>
#include <click.h>

#pragma once

namespace Ubuntu {
namespace AppLaunch {

class Connection::Impl {
public:
	Impl();
	virtual ~Impl() {
		thread.quit();
	}

	std::shared_ptr<JsonObject> getClickManifest(const std::string& package);
	std::list<std::string> getClickPackages();

private:
	GLib::ContextThread thread;

	std::shared_ptr<ClickDB> _clickDB;
	std::shared_ptr<ClickUser> _clickUser;

	void initClick ();
};

}; // namespace AppLaunch
}; // namespace Ubuntu
