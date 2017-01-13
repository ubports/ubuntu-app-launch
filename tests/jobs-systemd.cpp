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

class JobsSystemd : public EventuallyFixture
{
protected:
    std::shared_ptr<DbusTestService> service;
    std::shared_ptr<RegistryMock> registry;
    std::shared_ptr<SystemdMock> systemd;

    virtual void SetUp()
    {
        service = std::shared_ptr<DbusTestService>(dbus_test_service_new(nullptr),
                                                   [](DbusTestService* service) { g_clear_object(&service); });

        systemd = std::make_shared<SystemdMock>();
        dbus_test_service_add_task(service.get(), *systemd);

        dbus_test_service_start_tasks(service.get());
        registry = std::make_shared<RegistryMock>();
    }

    virtual void TearDown()
    {
        systemd.reset();
        registry.reset();
        service.reset();
    }

    ubuntu::app_launch::AppID simpleAppID()
    {
        return {ubuntu::app_launch::AppID::Package::from_raw("package"),
                ubuntu::app_launch::AppID::AppName::from_raw("appname"),
                ubuntu::app_launch::AppID::Version::from_raw("version")};
    }
};

TEST_F(JobsSystemd, Init)
{
    auto manager = std::make_shared<ubuntu::app_launch::jobs::manager::SystemD>(registry);

    manager.reset();
}
