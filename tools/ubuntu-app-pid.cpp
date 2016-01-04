/*
 * Copyright 2015 Canonical Ltd.
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
#include <libubuntu-app-launch/registry.h>
#include <libubuntu-app-launch/application.h>

int
main (int argc, char * argv[]) {

	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " <app id>" << std::endl;
		return 1;
	}

	auto appid = Ubuntu::AppLaunch::Application::appIdParse(argv[1]);
	auto app = Ubuntu::AppLaunch::Application::create(appid, Ubuntu::AppLaunch::Registry::getDefault());
	auto pid = app->instances()[0]->primaryPid();

	if (pid == 0) {
		return 1;
	}

	std::cout << pid << std::endl;
	return 0;
}
