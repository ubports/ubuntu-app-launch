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

#include "jobs-systemd.h"

#include "eventually-fixture.h"
#include "registry-mock.h"
#include "systemd-mock.h"

#define CGROUP_DIR (CMAKE_BINARY_DIR "/systemd-cgroups")

class JobsSystemd : public EventuallyFixture
{
protected:
    std::shared_ptr<DbusTestService> service;
    std::shared_ptr<RegistryMock> registry;
    std::shared_ptr<SystemdMock> systemd;
    GDBusConnection *bus = nullptr;

    virtual void SetUp() override
    {
        /* Get the applications dir */
        g_setenv("XDG_DATA_DIRS", CMAKE_SOURCE_DIR, TRUE);

        /* Setting the cgroup temp directory */
        g_setenv("UBUNTU_APP_LAUNCH_SYSTEMD_CGROUP_ROOT", CGROUP_DIR, TRUE);

        /* Force over to session bus */
        g_setenv("UBUNTU_APP_LAUNCH_SYSTEMD_PATH", "/this/should/not/exist", TRUE);

        service = std::shared_ptr<DbusTestService>(dbus_test_service_new(nullptr),
                                                   [](DbusTestService *service) { g_clear_object(&service); });

        systemd = std::make_shared<SystemdMock>(
            std::list<SystemdMock::Instance>{
                {defaultJobName(), std::string{multipleAppID()}, "1234567890", 11, {12, 13, 11}},
                {defaultJobName(), std::string{multipleAppID()}, "0987654321", 10, {10}},
                {defaultJobName(), std::string{singleAppID()}, {}, 5, {1, 2, 3, 4, 5}}},
            CGROUP_DIR);
        dbus_test_service_add_task(service.get(), *systemd);

        dbus_test_service_start_tasks(service.get());
        registry = std::make_shared<RegistryMock>();

        bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
        g_dbus_connection_set_exit_on_close(bus, FALSE);
        g_object_add_weak_pointer(G_OBJECT(bus), (gpointer *)&bus);
    }

    virtual void TearDown() override
    {
        systemd.reset();
        registry.reset();
        service.reset();

        g_object_unref(bus);
        ASSERT_EVENTUALLY_EQ(nullptr, bus);
    }

    std::string defaultJobName()
    {
        return "application-legacy";
    }

    ubuntu::app_launch::AppID singleAppID()
    {
        return {ubuntu::app_launch::AppID::Package::from_raw({}),
                ubuntu::app_launch::AppID::AppName::from_raw("single"),
                ubuntu::app_launch::AppID::Version::from_raw({})};
    }

    ubuntu::app_launch::AppID multipleAppID()
    {
        return {ubuntu::app_launch::AppID::Package::from_raw({}),
                ubuntu::app_launch::AppID::AppName::from_raw("multiple"),
                ubuntu::app_launch::AppID::Version::from_raw({})};
    }
};

/* Make sure we can build an object and destroy it */
TEST_F(JobsSystemd, Init)
{
    registry->impl->jobs = std::make_shared<ubuntu::app_launch::jobs::manager::SystemD>(registry);
}

/* Make sure we make the initial call to get signals and an initial list */
TEST_F(JobsSystemd, Startup)
{
    registry->impl->jobs = std::make_shared<ubuntu::app_launch::jobs::manager::SystemD>(registry);

    EXPECT_EVENTUALLY_FUNC_EQ(true, std::function<bool()>([this]() { return systemd->subscribeCallsCnt() > 0; }));
    EXPECT_EVENTUALLY_FUNC_EQ(true, std::function<bool()>([this]() -> bool { return systemd->listCallsCnt() > 0; }));
}

std::function<bool(const std::shared_ptr<ubuntu::app_launch::Application> &app)> findAppID(
    const ubuntu::app_launch::AppID &appid)
{
    return [appid](const std::shared_ptr<ubuntu::app_launch::Application> &app) { return appid == app->appId(); };
}

/* Get the running apps and check out their instances */
TEST_F(JobsSystemd, RunningApps)
{
    auto manager = std::make_shared<ubuntu::app_launch::jobs::manager::SystemD>(registry);
    registry->impl->jobs = manager;

    auto apps = manager->runningApps();
    ASSERT_FALSE(apps.empty());
    EXPECT_EQ(2u, apps.size());

    auto single = *std::find_if(apps.begin(), apps.end(), findAppID(singleAppID()));
    EXPECT_TRUE(bool(single));

    auto multiple = *std::find_if(apps.begin(), apps.end(), findAppID(multipleAppID()));
    EXPECT_TRUE(bool(multiple));

    auto sinstances = single->instances();

    ASSERT_FALSE(sinstances.empty());
    EXPECT_EQ(1u, sinstances.size());

    auto minstances = multiple->instances();

    ASSERT_FALSE(minstances.empty());
    EXPECT_EQ(2u, minstances.size());
}

/* Check to make sure we're getting the user bus path correctly */
TEST_F(JobsSystemd, UserBusPath)
{
    auto manager = std::make_shared<ubuntu::app_launch::jobs::manager::SystemD>(registry);
    registry->impl->jobs = manager;

    EXPECT_EQ(std::string{"/this/should/not/exist"}, manager->userBusPath());

    unsetenv("UBUNTU_APP_LAUNCH_SYSTEMD_PATH");
    EXPECT_EQ(std::string{"/run/user/"} + std::to_string(getuid()) + std::string{"/bus"}, manager->userBusPath());
}

/* PID Tools */
TEST_F(JobsSystemd, PidTools)
{
    auto manager = std::make_shared<ubuntu::app_launch::jobs::manager::SystemD>(registry);
    registry->impl->jobs = manager;

    EXPECT_EQ(5, manager->unitPrimaryPid(singleAppID(), defaultJobName(), {}));
    std::vector<pid_t> pidlist{1, 2, 3, 4, 5};
    EXPECT_EQ(pidlist, manager->unitPids(singleAppID(), defaultJobName(), {}));
}

/* PID Instance */
TEST_F(JobsSystemd, PidInstance)
{
    auto manager = std::make_shared<ubuntu::app_launch::jobs::manager::SystemD>(registry);
    registry->impl->jobs = manager;

    auto inst = manager->existing(singleAppID(), defaultJobName(), {}, {});
    EXPECT_TRUE(bool(inst));

    EXPECT_EQ(5, inst->primaryPid());
    std::vector<pid_t> pidlist{1, 2, 3, 4, 5};
    EXPECT_EQ(pidlist, inst->pids());
}

/* Stopping a Job */
TEST_F(JobsSystemd, StopUnit)
{
    auto manager = std::make_shared<ubuntu::app_launch::jobs::manager::SystemD>(registry);
    registry->impl->jobs = manager;

    manager->stopUnit(singleAppID(), defaultJobName(), {});

    std::list<std::string> stopcalls;
    EXPECT_EVENTUALLY_FUNC_LT(0u, std::function<unsigned int()>([&]() {
                                  stopcalls = systemd->stopCalls();
                                  return stopcalls.size();
                              }));

    EXPECT_EQ(SystemdMock::instanceName({defaultJobName(), std::string{singleAppID()}, {}, 1, {}}), *stopcalls.begin());

    systemd->managerClear();
    stopcalls.clear();

    manager->stopUnit(multipleAppID(), defaultJobName(), "1234567890");

    EXPECT_EVENTUALLY_FUNC_LT(0u, std::function<unsigned int()>([&]() {
                                  stopcalls = systemd->stopCalls();
                                  return stopcalls.size();
                              }));

    EXPECT_EQ(SystemdMock::instanceName({defaultJobName(), std::string{multipleAppID()}, "1234567890", 1, {}}),
              *stopcalls.begin());
}

/* Stop Instance */
TEST_F(JobsSystemd, StopInstance)
{
    auto manager = std::make_shared<ubuntu::app_launch::jobs::manager::SystemD>(registry);
    registry->impl->jobs = manager;

    auto inst = manager->existing(singleAppID(), defaultJobName(), {}, {});
    EXPECT_TRUE(bool(inst));

    inst->stop();

    std::list<std::string> stopcalls;
    EXPECT_EVENTUALLY_FUNC_LT(0u, std::function<unsigned int()>([&]() {
                                  stopcalls = systemd->stopCalls();
                                  return stopcalls.size();
                              }));

    EXPECT_EQ(SystemdMock::instanceName({defaultJobName(), std::string{singleAppID()}, {}, 1, {}}), *stopcalls.begin());
}

/* Starting a new job */
TEST_F(JobsSystemd, LaunchJob)
{
    auto manager = std::make_shared<ubuntu::app_launch::jobs::manager::SystemD>(registry);
    registry->impl->jobs = manager;

    bool gotenv{false};
    std::function<std::list<std::pair<std::string, std::string>>()> getenvfunc =
        [&]() -> std::list<std::pair<std::string, std::string>> {
        gotenv = true;
        return {{"APP_EXEC", "sh"}};
    };

    auto inst = manager->launch(multipleAppID(), defaultJobName(), "123", {},
                                ubuntu::app_launch::jobs::manager::launchMode::STANDARD, getenvfunc);

    EXPECT_TRUE(bool(inst));
    EXPECT_TRUE(gotenv);

    /* Check to see that we got called */
    std::list<SystemdMock::TransientUnit> units;
    EXPECT_EVENTUALLY_FUNC_LT(0u, std::function<unsigned int()>([&]() {
                                  units = systemd->unitCalls();
                                  return units.size();
                              }));

    /* Make sure it was the right one */
    EXPECT_EQ(SystemdMock::instanceName({defaultJobName(), std::string{multipleAppID()}, "123", 1, {}}),
              units.begin()->name);

    /* Check some standard environment variables */
    EXPECT_NE(units.begin()->environment.end(),
              units.begin()->environment.find(std::string{"APP_ID="} + std::string(multipleAppID())));
    EXPECT_NE(
        units.begin()->environment.end(),
        units.begin()->environment.find(std::string{"DBUS_SESSION_BUS_ADDRESS="} + getenv("DBUS_SESSION_BUS_ADDRESS")));

    /* Ensure the exec is correct */
    EXPECT_EQ("/bin/sh", units.begin()->execpath);

    /* Try an entirely custom variable */
    systemd->managerClear();
    units.clear();

    std::function<std::list<std::pair<std::string, std::string>>()> arbitraryenvfunc =
        [&]() -> std::list<std::pair<std::string, std::string>> {
        return {{"ARBITRARY_KEY", "EVEN_MORE_ARBITRARY_VALUE"}};
    };

    manager->launch(multipleAppID(), defaultJobName(), "123", {},
                    ubuntu::app_launch::jobs::manager::launchMode::STANDARD, arbitraryenvfunc);

    EXPECT_EVENTUALLY_FUNC_LT(0u, std::function<unsigned int()>([&]() {
                                  units = systemd->unitCalls();
                                  return units.size();
                              }));

    EXPECT_NE(units.begin()->environment.end(),
              units.begin()->environment.find("ARBITRARY_KEY=EVEN_MORE_ARBITRARY_VALUE"));
}

TEST_F(JobsSystemd, SignalNew)
{
    auto manager = std::make_shared<ubuntu::app_launch::jobs::manager::SystemD>(registry);
    registry->impl->jobs = manager;

    std::promise<ubuntu::app_launch::AppID> newunit;
    manager->appStarted().connect([&](const std::shared_ptr<ubuntu::app_launch::Application> &app,
                                      const std::shared_ptr<ubuntu::app_launch::Application::Instance> &inst) {
        try
        {
            if (!app)
            {
                throw std::runtime_error("Invalid Application");
            }

            if (!inst)
            {
                throw std::runtime_error("Invalid Instance");
            }

            newunit.set_value(app->appId());
        }
        catch (...)
        {
            newunit.set_exception(std::current_exception());
        }
    });

    systemd->managerEmitNew(SystemdMock::instanceName(

                                {defaultJobName(), std::string{multipleAppID()}, "1234", 1, {}}),
                            "/foo");

    EXPECT_EQ(multipleAppID(), newunit.get_future().get());
}

TEST_F(JobsSystemd, SignalRemove)
{
    auto manager = std::make_shared<ubuntu::app_launch::jobs::manager::SystemD>(registry);
    registry->impl->jobs = manager;

    std::promise<ubuntu::app_launch::AppID> removeunit;
    manager->appStopped().connect([&](const std::shared_ptr<ubuntu::app_launch::Application> &app,
                                      const std::shared_ptr<ubuntu::app_launch::Application::Instance> &inst) {
        try
        {
            if (!app)
            {
                throw std::runtime_error("Invalid Application");
            }

            if (!inst)
            {
                throw std::runtime_error("Invalid Instance");
            }

            removeunit.set_value(app->appId());
        }
        catch (...)
        {
            removeunit.set_exception(std::current_exception());
        }
    });

    systemd->managerEmitRemoved(SystemdMock::instanceName(

                                    {defaultJobName(), std::string{multipleAppID()}, "1234567890", 1, {}}),
                                "/foo");

    EXPECT_EQ(multipleAppID(), removeunit.get_future().get());
}

TEST_F(JobsSystemd, UnitFailure)
{
    auto manager = std::make_shared<ubuntu::app_launch::jobs::manager::SystemD>(registry);
    registry->impl->jobs = manager;

    ubuntu::app_launch::AppID failedappid;
    manager->appFailed().connect([&](const std::shared_ptr<ubuntu::app_launch::Application> &app,
                                     const std::shared_ptr<ubuntu::app_launch::Application::Instance> &inst,
                                     ubuntu::app_launch::Registry::FailureType type) { failedappid = app->appId(); });

    systemd->managerEmitFailed({defaultJobName(), std::string{multipleAppID()}, "1234567890", 1, {}});

    EXPECT_EVENTUALLY_EQ(multipleAppID(), failedappid);

    std::list<std::string> resets;
    EXPECT_EVENTUALLY_FUNC_LT(0u, std::function<unsigned int()>([&]() {
                                  resets = systemd->resetCalls();
                                  return resets.size();
                              }));

    EXPECT_EQ(SystemdMock::instanceName({defaultJobName(), std::string{multipleAppID()}, "1234567890", 1, {}}),
              *resets.begin());
}
