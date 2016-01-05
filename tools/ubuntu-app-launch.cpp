/*
 * Copyright © 2016 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *     Ted Gould <ted.gould@canonical.com>
 */

#include <iostream>
#include <future>
#include <csignal>
#include "libubuntu-app-launch/application.h"
#include "libubuntu-app-launch/registry.h"

Ubuntu::AppLaunch::AppID global_appid;
std::promise<int> retval;

int
main (int argc, char * argv[]) {
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " <app id> [uris]" << std::endl;
		return 1;
	}

	global_appid = Ubuntu::AppLaunch::AppID::parse(argv[1]);

	std::vector<Ubuntu::AppLaunch::Application::URL> urls;
	for (int i = 2; i < argc; i++) {
		urls.push_back(Ubuntu::AppLaunch::Application::URL::from_raw(argv[i]));
	}

	auto registry = std::make_shared<Ubuntu::AppLaunch::Registry>();

	registry->appStarted.connect([](std::shared_ptr<Ubuntu::AppLaunch::Application> app, std::shared_ptr<Ubuntu::AppLaunch::Application::Instance> instance) {
		if (app->appId() != global_appid)
			return;

		std::cout << "Started: " << (std::string)app->appId() << std::endl;
		retval.set_value(0);
	});

	registry->appFailed.connect([](std::shared_ptr<Ubuntu::AppLaunch::Application> app, std::shared_ptr<Ubuntu::AppLaunch::Application::Instance> instance, Ubuntu::AppLaunch::Registry::FailureType type) {
		if (app->appId() != global_appid)
			return;

		std::cout << "Failed:  " << (std::string)app->appId() << std::endl;
		retval.set_value(-1);
	});

	auto app = Ubuntu::AppLaunch::Application::create(global_appid, registry);
	app->launch(urls);

	std::signal(SIGTERM, [](int signal) -> void {
		retval.set_value(0);
	});
	return retval.get_future().get();
}
