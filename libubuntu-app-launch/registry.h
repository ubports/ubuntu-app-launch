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

#include <core/signal.h>
#include <functional>
#include <list>
#include <memory>

#include "application.h"
#include "helper.h"

#pragma once
#pragma GCC visibility push(default)

namespace ubuntu
{
namespace app_launch
{

/** The application registry provides a central source for finding information
    about the applications in the system. This includes installed applications
    and running applications.

    This class also holds onto shared resources for Ubuntu App Launch objects and
    functions. Generally speaking, there should only be one of them in the
    process. There are singleton functions, getDefault() and clearDefault(), which
    can be used to port applications from the old C API to the new C++ one
    but their use is discouraged. */
class Registry
{
public:
    /** Sometimes apps fail, this gives us information on why they
        failed. */
    enum class FailureType
    {
        CRASH,         /**< The application was running, but failed while running. */
        START_FAILURE, /**< Something in the configuration of the application made it impossible to start the
                          application */
    };

    Registry();
    virtual ~Registry();

    /* Lots of application lists */
    /** List the applications that are currently running, each will have a valid
        Application::Instance at call time, but that could change as soon as
        the call occurs.

        \param registry Shared registry for the tracking
    */
    static std::list<std::shared_ptr<Application>> runningApps(std::shared_ptr<Registry> registry = getDefault());
    /** List all of the applications that are currently installed on the system.
        Queries the various packaging schemes that are supported to get thier
        list of applications.

        \param registry Shared registry for the tracking
    */
    static std::list<std::shared_ptr<Application>> installedApps(std::shared_ptr<Registry> registry = getDefault());

    /* Signals to discover what is happening to apps */
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>>& appStarted();
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>>& appStopped();
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>, FailureType>& appFailed();
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>>& appPaused();
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>>& appResumed();

    /* The Application Manager, almost always if you're not Unity8, don't
       use this API. Testing is a special case. */
    class Manager
    {
        virtual bool focusRequest(std::shared_ptr<Application> app,
                                  std::shared_ptr<Application::Instance> instance) = 0;
        virtual bool startingRequest(std::shared_ptr<Application> app,
                                     std::shared_ptr<Application::Instance> instance) = 0;

    protected:
        Manager() = default;
    };

    void setManager(Manager* manager);
    void clearManager();

    /* Helper Lists */
    /** Get a list of all the helpers for a given helper type

        \param type Helper type string
        \param registry Shared registry for the tracking
    */
    static std::list<std::shared_ptr<Helper>> runningHelpers(Helper::Type type,
                                                             std::shared_ptr<Registry> registry = getDefault());

    /* Default Junk */
    /** Use the Registry as a global singleton, this function will create
        a Registry object if one doesn't exist. Use of this function is
        discouraged. */
    static std::shared_ptr<Registry> getDefault();
    /** Clear the default. If you're using the singleton interface in the
        Registry::getDefault() function you should call this as your service
        and/or tests exit to ensure you don't get Valgrind errors. */
    static void clearDefault();

    /* Hide our implementation */
    /** \private */
    class Impl;
    /** \private */
    std::unique_ptr<Impl> impl;
};

};  // namespace app_launch
};  // namespace ubuntu

#pragma GCC visibility pop
