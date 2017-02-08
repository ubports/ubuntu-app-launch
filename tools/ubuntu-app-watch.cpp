/*
 * Copyright © 2015 Canonical Ltd.
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

#include "libubuntu-app-launch/registry.h"
#include <csignal>
#include <future>

std::promise<int> retval;

int main(int argc, char* argv[])
{
    ubuntu::app_launch::Registry registry;

    registry.appStarted().connect([](const std::shared_ptr<ubuntu::app_launch::Application>& app,
                                     const std::shared_ptr<ubuntu::app_launch::Application::Instance>& instance) {
        std::cout << "Started: " << (std::string)app->appId() << std::endl;
    });
    registry.appStopped().connect([](const std::shared_ptr<ubuntu::app_launch::Application>& app,
                                     const std::shared_ptr<ubuntu::app_launch::Application::Instance>& instance) {
        std::cout << "Stopped: " << (std::string)app->appId() << std::endl;
    });
    registry.appPaused().connect([](const std::shared_ptr<ubuntu::app_launch::Application>& app,
                                    const std::shared_ptr<ubuntu::app_launch::Application::Instance>& instance,
                                    const std::vector<pid_t>& pids) {
        std::cout << "Paused:  " << (std::string)app->appId() << " (";

        for (auto pid : pids)
        {
            std::cout << std::to_string(pid) << " ";
        }

        std::cout << ")" << std::endl;
    });
    registry.appResumed().connect([](const std::shared_ptr<ubuntu::app_launch::Application>& app,
                                     const std::shared_ptr<ubuntu::app_launch::Application::Instance>& instance,
                                     const std::vector<pid_t>& pids) {
        std::cout << "Resumed: " << (std::string)app->appId() << " (";

        for (auto pid : pids)
        {
            std::cout << std::to_string(pid) << " ";
        }

        std::cout << ")" << std::endl;
    });
    registry.appFocused().connect([](const std::shared_ptr<ubuntu::app_launch::Application>& app,
                                     const std::shared_ptr<ubuntu::app_launch::Application::Instance>& instance,
                                     const std::vector<pid_t>& pids) {
        std::cout << "Focused: " << (std::string)app->appId() << " (";

        for (auto pid : pids)
        {
            std::cout << std::to_string(pid) << " ";
        }

        std::cout << ")" << std::endl;
    });
    registry.appFailed().connect([](const std::shared_ptr<ubuntu::app_launch::Application>& app,
                                    const std::shared_ptr<ubuntu::app_launch::Application::Instance>& instance,
                                    ubuntu::app_launch::Registry::FailureType type) {
        std::cout << "Failed:  " << (std::string)app->appId();
        switch (type)
        {
            case ubuntu::app_launch::Registry::FailureType::CRASH:
                std::cout << " (crash)";
                break;
            case ubuntu::app_launch::Registry::FailureType::START_FAILURE:
                std::cout << " (start failure)";
                break;
        }
        std::cout << std::endl;
    });

    std::signal(SIGTERM, [](int signal) -> void { retval.set_value(EXIT_SUCCESS); });
    return retval.get_future().get();
}
