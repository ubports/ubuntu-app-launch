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

#include "app-store-legacy.h"

#include "eventually-fixture.h"
#include "registry-mock.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <libdbustest/dbus-test.h>
#include <unity/util/GObjectMemory.h>

class AppStoreLegacy : public EventuallyFixture
{
protected:
    std::unique_ptr<DbusTestService, unity::util::GObjectDeleter> service;
    std::shared_ptr<RegistryMock> registry;

    virtual void SetUp()
    {
        service = unity::util::unique_gobject(dbus_test_service_new(nullptr));
        dbus_test_service_start_tasks(service.get());
        registry = std::make_shared<RegistryMock>();
    }
};

TEST_F(AppStoreLegacy, Init)
{
    auto store = std::make_shared<ubuntu::app_launch::app_store::Legacy>();
    store.reset();
}
