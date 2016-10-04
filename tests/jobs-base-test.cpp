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

#include "jobs-base.h"
#include "appid.h"
#include "registry-impl.h"
#include "registry.h"

#include "eventually-fixture.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

class RegistryImplMock : public ubuntu::app_launch::Registry::Impl
{
public:
    RegistryImplMock(ubuntu::app_launch::Registry* reg)
        : ubuntu::app_launch::Registry::Impl(reg)
    {
    }

    MOCK_METHOD2(zgSendEvent, void(ubuntu::app_launch::AppID, const std::string& eventtype));
};

class RegistryMock : public ubuntu::app_launch::Registry
{
public:
    RegistryMock()
    {
        impl = std::unique_ptr<RegistryImplMock>(new RegistryImplMock(this));
    }
};

class instanceMock : public ubuntu::app_launch::jobs::instance::Base
{
public:
    instanceMock(const ubuntu::app_launch::AppID& appId,
                 const std::string& job,
                 const std::string& instance,
                 const std::vector<ubuntu::app_launch::Application::URL>& urls,
                 const std::shared_ptr<ubuntu::app_launch::Registry>& registry)
        : ubuntu::app_launch::jobs::instance::Base(appId, job, instance, urls, registry)
    {
    }

    MOCK_METHOD0(primaryPid, pid_t());
    MOCK_METHOD0(logPath, std::string());
    MOCK_METHOD0(pids, std::vector<pid_t>());

    MOCK_METHOD0(stop, void());
};

class JobBaseTest : public EventuallyFixture
{
protected:
    std::shared_ptr<RegistryMock> registry;

    virtual void SetUp()
    {
        registry = std::make_shared<RegistryMock>();
    }

    virtual void TearDown()
    {
        registry.reset();
    }

    std::shared_ptr<instanceMock> simpleInstance()
    {
        return std::make_shared<instanceMock>(
            ubuntu::app_launch::AppID{ubuntu::app_launch::AppID::Package::from_raw("package"),
                                      ubuntu::app_launch::AppID::AppName::from_raw("appname"),
                                      ubuntu::app_launch::AppID::Version::from_raw("version")},
            "application-job", "1234567890", std::vector<ubuntu::app_launch::Application::URL>{}, registry);
    }
};

TEST_F(JobBaseTest, InitTest)
{
    auto instance = simpleInstance();

    instance.reset();
}

TEST_F(JobBaseTest, isRunning)
{
    auto instance = simpleInstance();

    EXPECT_CALL(*instance, primaryPid()).WillOnce(testing::Return(0));

    EXPECT_FALSE(instance->isRunning());

    EXPECT_CALL(*instance, primaryPid()).WillOnce(testing::Return(100));

    EXPECT_TRUE(instance->isRunning());
}

TEST_F(JobBaseTest, pause)
{
}
