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

#include "appid.h"
#include "jobs-base.h"

#include "eventually-fixture.h"
#include "registry-mock.h"
#include "spew-master.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <libdbustest/dbus-test.h>

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
    std::shared_ptr<DbusTestService> service;
    std::shared_ptr<RegistryMock> registry;

    virtual void SetUp()
    {
        service = std::shared_ptr<DbusTestService>(dbus_test_service_new(nullptr),
                                                   [](DbusTestService* service) { g_clear_object(&service); });
        dbus_test_service_start_tasks(service.get());
        registry = std::make_shared<RegistryMock>();
    }

    virtual void TearDown()
    {
        registry.reset();
        service.reset();
    }

    ubuntu::app_launch::AppID simpleAppID()
    {
        return {ubuntu::app_launch::AppID::Package::from_raw("package"),
                ubuntu::app_launch::AppID::AppName::from_raw("appname"),
                ubuntu::app_launch::AppID::Version::from_raw("version")};
    }

    std::shared_ptr<instanceMock> simpleInstance()
    {
        return std::make_shared<instanceMock>(simpleAppID(), "application-job", "1234567890",
                                              std::vector<ubuntu::app_launch::Application::URL>{}, registry);
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

TEST_F(JobBaseTest, pauseResume)
{
    g_setenv("UBUNTU_APP_LAUNCH_OOM_PROC_PATH", CMAKE_BINARY_DIR "/jobs-base-proc", TRUE);

    /* Setup some spew */
    SpewMaster spew;
    std::vector<pid_t> pids{spew.pid()};

    /* Build our instance */
    auto instance = simpleInstance();
    EXPECT_CALL(*instance, pids()).WillRepeatedly(testing::Return(pids));

    /* Setup registry */
    EXPECT_CALL(dynamic_cast<RegistryImplMock&>(*registry->impl), zgSendEvent(simpleAppID(), ZEITGEIST_ZG_LEAVE_EVENT))
        .WillOnce(testing::Return());

    /* Make sure it is running */
    EXPECT_EVENTUALLY_FUNC_NE(gsize{0}, std::function<gsize()>{[&spew] { return spew.dataCnt(); }});

    /*** Do Pause ***/
    instance->pause();

    spew.reset();
    pause(100);  // give spew a chance to send data if it is running

    EXPECT_EQ(0u, spew.dataCnt());

    EXPECT_EQ(std::to_string(int(ubuntu::app_launch::oom::paused())), spew.oomScore());

    /* Setup for Resume */
    EXPECT_CALL(dynamic_cast<RegistryImplMock&>(*registry->impl), zgSendEvent(simpleAppID(), ZEITGEIST_ZG_ACCESS_EVENT))
        .WillOnce(testing::Return());

    spew.reset();
    EXPECT_EQ(0u, spew.dataCnt());

    /*** Do Resume ***/
    instance->resume();

    EXPECT_EVENTUALLY_FUNC_NE(gsize{0}, std::function<gsize()>{[&spew] { return spew.dataCnt(); }});

    EXPECT_EQ(std::to_string(int(ubuntu::app_launch::oom::focused())), spew.oomScore());
}

TEST_F(JobBaseTest, pauseResumeNone)
{
    std::vector<pid_t> pids{};

    /* Build our instance */
    auto instance = simpleInstance();
    EXPECT_CALL(*instance, pids()).WillRepeatedly(testing::Return(pids));

    /* Setup registry */
    EXPECT_CALL(dynamic_cast<RegistryImplMock&>(*registry->impl), zgSendEvent(simpleAppID(), ZEITGEIST_ZG_LEAVE_EVENT))
        .WillOnce(testing::Return());

    /*** Do Pause ***/
    instance->pause();

    /* Setup for Resume */
    EXPECT_CALL(dynamic_cast<RegistryImplMock&>(*registry->impl), zgSendEvent(simpleAppID(), ZEITGEIST_ZG_ACCESS_EVENT))
        .WillOnce(testing::Return());

    /*** Do Resume ***/
    instance->resume();
}

TEST_F(JobBaseTest, pauseResumeMany)
{
    g_setenv("UBUNTU_APP_LAUNCH_OOM_PROC_PATH", CMAKE_BINARY_DIR "/jobs-base-proc", TRUE);

    /* Setup some A TON OF spew */
    std::array<SpewMaster, 50> spews;
    std::vector<pid_t> pids(50);
    std::transform(spews.begin(), spews.end(), pids.begin(), [](SpewMaster& spew) { return spew.pid(); });

    /* Build our instance */
    auto instance = simpleInstance();
    EXPECT_CALL(*instance, pids()).WillRepeatedly(testing::Return(pids));

    /* Setup registry */
    EXPECT_CALL(dynamic_cast<RegistryImplMock&>(*registry->impl), zgSendEvent(simpleAppID(), ZEITGEIST_ZG_LEAVE_EVENT))
        .WillOnce(testing::Return());

    /* Make sure it is running */
    for (auto& spew : spews)
    {
        EXPECT_EVENTUALLY_FUNC_NE(gsize{0}, std::function<gsize()>{[&spew] { return spew.dataCnt(); }});
    }

    /*** Do Pause ***/
    instance->pause();

    for (auto& spew : spews)
    {
        spew.reset();
    }
    pause(100);  // give spew a chance to send data if it is running

    for (auto& spew : spews)
    {
        EXPECT_EQ(0u, spew.dataCnt());

        EXPECT_EQ(std::to_string(int(ubuntu::app_launch::oom::paused())), spew.oomScore());
    }

    /* Setup for Resume */
    EXPECT_CALL(dynamic_cast<RegistryImplMock&>(*registry->impl), zgSendEvent(simpleAppID(), ZEITGEIST_ZG_ACCESS_EVENT))
        .WillOnce(testing::Return());

    for (auto& spew : spews)
    {
        spew.reset();
        EXPECT_EQ(0u, spew.dataCnt());
    }

    /*** Do Resume ***/
    instance->resume();

    for (auto& spew : spews)
    {
        EXPECT_EVENTUALLY_FUNC_NE(gsize{0}, std::function<gsize()>{[&spew] { return spew.dataCnt(); }});

        EXPECT_EQ(std::to_string(int(ubuntu::app_launch::oom::focused())), spew.oomScore());
    }
}
