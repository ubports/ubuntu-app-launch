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

#include "info-watcher-zg.h"

#include "eventually-fixture.h"
#include "registry-mock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <libdbustest/dbus-test.h>

class InfoWatcherZg : public EventuallyFixture
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
};

TEST_F(InfoWatcherZg, InitTest)
{
    auto watcher = std::make_shared<ubuntu::app_launch::info_watcher::Zietgeist>(registry);

    watcher.reset();
}
