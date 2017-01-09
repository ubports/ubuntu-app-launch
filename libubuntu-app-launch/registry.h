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
        CRASH,        /**< The application was running, but failed while running. */
        START_FAILURE /**< Something in the configuration of the application made it impossible to start the
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
    /** Get the signal object that is signaled when an application has been
        started.

        \note This signal handler is activated on the UAL thread, if you want
            to execute on a different thread you'll need to move the work.

        \param reg Registry to get the handler from
    */
    static core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>>& appStarted(
        const std::shared_ptr<Registry>& reg = getDefault());

    /** Get the signal object that is signaled when an application has stopped.

        \note This signal handler is activated on the UAL thread, if you want
            to execute on a different thread you'll need to move the work.

        \param reg Registry to get the handler from
    */
    static core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>>& appStopped(
        const std::shared_ptr<Registry>& reg = getDefault());

    /** Get the signal object that is signaled when an application has failed.

        \note This signal handler is activated on the UAL thread, if you want
            to execute on a different thread you'll need to move the work.

        \param reg Registry to get the handler from
    */
    static core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>, FailureType>& appFailed(
        const std::shared_ptr<Registry>& reg = getDefault());

    /** Get the signal object that is signaled when an application has been
        paused.

        \note This signal handler is activated on the UAL thread, if you want
            to execute on a different thread you'll need to move the work.

        \param reg Registry to get the handler from
    */
    static core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>, std::vector<pid_t>&>&
        appPaused(const std::shared_ptr<Registry>& reg = getDefault());

    /** Get the signal object that is signaled when an application has been
        resumed.

        \note This signal handler is activated on the UAL thread, if you want
            to execute on a different thread you'll need to move the work.

        \param reg Registry to get the handler from
    */
    static core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>, std::vector<pid_t>&>&
        appResumed(const std::shared_ptr<Registry>& reg = getDefault());

    /** The Application Manager, almost always if you're not Unity8, don't
        use this API. Testing is a special case. Subclass this interface and
        implement these functions.

        Each function here is being passed a function object that takes a boolean
        to reply. This will accept or reject the request. The function object
        can be copied to another thread and executed if needed.

        The reply is required for the application to start. It will block (not
        currently implemented) until approval is given. If there are multiple requests
        sent they may be replied out of order if desired.
    */
    class Manager
    {
    public:
        /** Application wishes to startup

            \note This signal handler is activated on the UAL thread, if you want
                to execute on a different thread you'll need to move the work.

            \param app Application requesting startup
            \param instance Instance of the app, always valid but not useful
                        unless mulit-instance app.
            \param reply Function object to reply if it is allowed to start
        */
        virtual void startingRequest(std::shared_ptr<Application> app,
                                     std::shared_ptr<Application::Instance> instance,
                                     std::function<void(bool)> reply) = 0;

        /** Application wishes to have focus. Usually this occurs when
            a URL for the application is activated and the running app is
            requested.

            \note This signal handler is activated on the UAL thread, if you want
                to execute on a different thread you'll need to move the work.

            \param app Application requesting focus
            \param instance Instance of the app, always valid but not useful
                        unless mulit-instance app.
            \param reply Function object to reply if it is allowed to focus
        */
        virtual void focusRequest(std::shared_ptr<Application> app,
                                  std::shared_ptr<Application::Instance> instance,
                                  std::function<void(bool)> reply) = 0;

        /** Application wishes to resume. Usually this occurs when
            a URL for the application is activated and the running app is
            requested.

            \note This signal handler is activated on the UAL thread, if you want
                to execute on a different thread you'll need to move the work.

            \param app Application requesting resume
            \param instance Instance of the app, always valid but not useful
                        unless mulit-instance app.
            \param reply Function object to reply if it is allowed to resume
        */
        virtual void resumeRequest(std::shared_ptr<Application> app,
                                   std::shared_ptr<Application::Instance> instance,
                                   std::function<void(bool)> reply) = 0;

    protected:
        Manager() = default;
    };

    /** Set the manager of applications, which gives permissions for them to
        start and gain focus. In almost all cases this should be Unity8 as it
        will be controlling applications.

        This function will failure if there is already a manager set.

        \param manager A reference to the Manager object to call
        \param registry Registry to register the manager on
    */
    static void setManager(const std::shared_ptr<Manager>& manager, const std::shared_ptr<Registry>& registry);

    /** Remove the current manager on the registry */
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

}  // namespace app_launch
}  // namespace ubuntu

#pragma GCC visibility pop
