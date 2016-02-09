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

#include <list>
#include <memory>
#include <functional>
#include <core/signal.h>

#include "application.h"
#include "helper.h"

#pragma once
#pragma GCC visibility push(default)

namespace ubuntu
{
namespace app_launch
{

class Registry
{
public:
    enum FailureType
    {
        CRASH,
        START_FAILURE,
    };

    Registry();
    virtual ~Registry();

    /* Lots of application lists */
    static std::list<std::shared_ptr<Application>> runningApps(std::shared_ptr<Registry> registry = getDefault());
    static std::list<std::shared_ptr<Application>> installedApps(std::shared_ptr<Registry> registry = getDefault());

#if 0 /* TODO -- In next MR */
    /* Signals to discover what is happening to apps */
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>> appStarted;
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>> appStopped;
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>, FailureType> appFailed;
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>> appPaused;
    core::Signal<std::shared_ptr<Application>, std::shared_ptr<Application::Instance>> appResumed;

    /* The Application Manager, almost always if you're not Unity8, don't
       use this API. Testing is a special case. */
    class Manager
    {
        virtual bool focusRequest (std::shared_ptr<Application> app, std::shared_ptr<Application::Instance> instance) = 0;
        virtual bool startingRequest (std::shared_ptr<Application> app, std::shared_ptr<Application::Instance> instance) = 0;

    protected:
        Manager() = default;
    };

    void setManager (Manager* manager);
    void clearManager ();
#endif

    /* Helper Lists */
    static std::list<std::shared_ptr<Helper>> runningHelpers(Helper::Type type,
                                                             std::shared_ptr<Registry> registry = getDefault());

    /* Default Junk */
    static std::shared_ptr<Registry> getDefault();
    static void clearDefault();

    /* Hide our implementation */
    class Impl;
    std::unique_ptr<Impl> impl;
};

};  // namespace app_launch
};  // namespace ubuntu

#pragma GCC visibility pop
