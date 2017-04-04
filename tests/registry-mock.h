/*
 * Copyright © 2017 Canonical Ltd.
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

#pragma once

#include "app-store-base.h"
#include "info-watcher-zg.h"
#include "registry-impl.h"
#include "registry.h"

#include <gmock/gmock.h>

class MockStore : public ubuntu::app_launch::app_store::Base
{
public:
    MockStore(const std::shared_ptr<ubuntu::app_launch::Registry::Impl>& registry)
        : ubuntu::app_launch::app_store::Base(registry)
    {
    }

    ~MockStore()
    {
    }

    MOCK_METHOD1(verifyPackage, bool(const ubuntu::app_launch::AppID::Package&));
    MOCK_METHOD2(verifyAppname,
                 bool(const ubuntu::app_launch::AppID::Package&, const ubuntu::app_launch::AppID::AppName&));
    MOCK_METHOD2(findAppname,
                 ubuntu::app_launch::AppID::AppName(const ubuntu::app_launch::AppID::Package&,
                                                    ubuntu::app_launch::AppID::ApplicationWildcard));
    MOCK_METHOD2(findVersion,
                 ubuntu::app_launch::AppID::Version(const ubuntu::app_launch::AppID::Package&,
                                                    const ubuntu::app_launch::AppID::AppName&));
    MOCK_METHOD1(hasAppId, bool(const ubuntu::app_launch::AppID&));

    /* Possible apps */
    MOCK_METHOD0(list, std::list<std::shared_ptr<ubuntu::app_launch::Application>>());

    /* Application Creation */
    MOCK_METHOD1(create, std::shared_ptr<ubuntu::app_launch::app_impls::Base>(const ubuntu::app_launch::AppID&));
};

class MockApp : public ubuntu::app_launch::app_impls::Base
{
public:
    ubuntu::app_launch::AppID appid_;

    MockApp(const ubuntu::app_launch::AppID& appid, const std::shared_ptr<ubuntu::app_launch::Registry::Impl>& reg)
        : ubuntu::app_launch::app_impls::Base(reg)
        , appid_(appid)
    {
    }
    ~MockApp()
    {
    }

    ubuntu::app_launch::AppID appId() override
    {
        return appid_;
    }

    MOCK_METHOD1(findInstance, std::shared_ptr<ubuntu::app_launch::Application::Instance>(const std::string&));
    MOCK_METHOD1(findInstance, std::shared_ptr<ubuntu::app_launch::Application::Instance>(const pid_t&));
    MOCK_METHOD0(info, std::shared_ptr<ubuntu::app_launch::Application::Info>());
    MOCK_METHOD0(hasInstances, bool());
    MOCK_METHOD0(instances, std::vector<std::shared_ptr<ubuntu::app_launch::Application::Instance>>());
    MOCK_METHOD1(launch,
                 std::shared_ptr<ubuntu::app_launch::Application::Instance>(
                     const std::vector<ubuntu::app_launch::Application::URL>&));
    MOCK_METHOD1(launchTest,
                 std::shared_ptr<ubuntu::app_launch::Application::Instance>(
                     const std::vector<ubuntu::app_launch::Application::URL>&));
};

class MockInst : public ubuntu::app_launch::jobs::instance::Base
{
public:
    MockInst(const ubuntu::app_launch::AppID& appId,
             const std::string& job,
             const std::string& instance,
             const std::vector<ubuntu::app_launch::Application::URL>& urls,
             const std::shared_ptr<ubuntu::app_launch::Registry::Impl>& registry)
        : ubuntu::app_launch::jobs::instance::Base(appId, job, instance, urls, registry){};
    ~MockInst(){};

    MOCK_METHOD0(pids, std::vector<pid_t>());
    MOCK_METHOD0(primaryPid, pid_t());
    MOCK_METHOD0(stop, void());
};

class MockJobsManager : public ubuntu::app_launch::jobs::manager::Base
{
public:
    MockJobsManager(const std::shared_ptr<ubuntu::app_launch::Registry::Impl>& reg)
        : ubuntu::app_launch::jobs::manager::Base(reg)
    {
    }
    ~MockJobsManager()
    {
    }

    MOCK_METHOD6(launch,
                 std::shared_ptr<ubuntu::app_launch::Application::Instance>(
                     const ubuntu::app_launch::AppID&,
                     const std::string&,
                     const std::string&,
                     const std::vector<ubuntu::app_launch::Application::URL>&,
                     ubuntu::app_launch::jobs::manager::launchMode,
                     std::function<std::list<std::pair<std::string, std::string>>(void)>&));

    MOCK_METHOD4(existing,
                 std::shared_ptr<ubuntu::app_launch::Application::Instance>(
                     const ubuntu::app_launch::AppID&,
                     const std::string&,
                     const std::string&,
                     const std::vector<ubuntu::app_launch::Application::URL>&));

    MOCK_METHOD0(runningApps, std::list<std::shared_ptr<ubuntu::app_launch::Application>>());
    MOCK_METHOD1(runningHelpers,
                 std::list<std::shared_ptr<ubuntu::app_launch::Helper>>(const ubuntu::app_launch::Helper::Type&));

    MOCK_METHOD1(runningAppIds, std::list<std::string>(const std::list<std::string>&));

    MOCK_METHOD2(instances,
                 std::vector<std::shared_ptr<ubuntu::app_launch::jobs::instance::Base>>(
                     const ubuntu::app_launch::AppID& appID, const std::string& job));

    /* Job signals from implementations */
    MOCK_METHOD0(jobStarted, core::Signal<const std::string&, const std::string&, const std::string&>&());
    MOCK_METHOD0(jobStopped, core::Signal<const std::string&, const std::string&, const std::string&>&());
    MOCK_METHOD0(jobFailed,
                 core::Signal<const std::string&,
                              const std::string&,
                              const std::string&,
                              ubuntu::app_launch::Registry::FailureType>&());
};

class zgWatcherMock : public ubuntu::app_launch::info_watcher::Zeitgeist
{
public:
    zgWatcherMock()
        : ubuntu::app_launch::info_watcher::Zeitgeist({})
    {
    }

    virtual ~zgWatcherMock()
    {
    }

    MOCK_METHOD1(lookupAppPopularity,
                 ubuntu::app_launch::Application::Info::Popularity(const ubuntu::app_launch::AppID&));
};

class RegistryImplMock : public ubuntu::app_launch::Registry::Impl
{
public:
    RegistryImplMock()
        : ubuntu::app_launch::Registry::Impl()
    {
        setupZgWatcher();

        g_debug("Registry Mock Implementation Created");
    }

    void setupZgWatcher()
    {
        auto zgWatcher = std::make_shared<zgWatcherMock>();

        ON_CALL(*zgWatcher, lookupAppPopularity(testing::_))
            .WillByDefault(testing::Return(ubuntu::app_launch::Application::Info::Popularity::from_raw(1u)));

        setZgWatcher(zgWatcher);
    }

    ~RegistryImplMock()
    {
        g_debug("Registry Mock Implementation taken down");
    }

    MOCK_METHOD2(zgSendEvent, void(ubuntu::app_launch::AppID, const std::string& eventtype));
};

class RegistryMock : public ubuntu::app_launch::Registry
{
public:
    RegistryMock()
        : Registry(std::make_shared<RegistryImplMock>())
    {
        g_debug("Registry Mock Created");
    }

    RegistryMock(std::list<std::shared_ptr<ubuntu::app_launch::app_store::Base>> appStores,
                 std::shared_ptr<ubuntu::app_launch::jobs::manager::Base> jobManager)
        : Registry(std::make_shared<RegistryImplMock>())
    {
        impl->setAppStores(appStores);
        impl->setJobs(jobManager);
        g_debug("Registry Mock Created");
    }

    ~RegistryMock()
    {
        g_debug("Registry Mock taken down");
    }
};
