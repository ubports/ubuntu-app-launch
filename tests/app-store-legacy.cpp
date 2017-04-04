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
#include "test-directory.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <libdbustest/dbus-test.h>
#include <unity/util/GObjectMemory.h>
#include <unity/util/GlibMemory.h>

class AppStoreLegacy : public EventuallyFixture
{
protected:
    std::unique_ptr<DbusTestService, unity::util::GObjectDeleter> service;
    std::shared_ptr<RegistryMock> registry;

    virtual void SetUp()
    {
        setenv("XDG_DATA_DIRS", CMAKE_SOURCE_DIR, 1);

        service = unity::util::unique_gobject(dbus_test_service_new(nullptr));
        dbus_test_service_start_tasks(service.get());
        registry = std::make_shared<RegistryMock>(std::list<std::shared_ptr<ubuntu::app_launch::app_store::Base>>{},
                                                  std::shared_ptr<ubuntu::app_launch::jobs::manager::Base>{});
    }
};

TEST_F(AppStoreLegacy, Init)
{
    auto store = std::make_shared<ubuntu::app_launch::app_store::Legacy>(registry->impl);
    store.reset();
}

TEST_F(AppStoreLegacy, FindApp)
{
    TestDirectory testdir;
    testdir.addApp("testapp",
                   {{G_KEY_FILE_DESKTOP_GROUP,
                     {
                         {G_KEY_FILE_DESKTOP_KEY_NAME, "Test App"},
                         {G_KEY_FILE_DESKTOP_KEY_TYPE, "Application"},
                         {G_KEY_FILE_DESKTOP_KEY_ICON, "foo.png"},
                         {G_KEY_FILE_DESKTOP_KEY_EXEC, "foo"},
                     }}});

    auto store = std::make_shared<ubuntu::app_launch::app_store::Legacy>(registry->impl);

    EXPECT_TRUE(store->verifyAppname(ubuntu::app_launch::AppID::Package::from_raw({}),
                                     ubuntu::app_launch::AppID::AppName::from_raw("testapp")));
}

TEST_F(AppStoreLegacy, RemoveApp)
{
    TestDirectory testdir;
    testdir.addApp("testapp",
                   {{G_KEY_FILE_DESKTOP_GROUP,
                     {
                         {G_KEY_FILE_DESKTOP_KEY_NAME, "Test App"},
                         {G_KEY_FILE_DESKTOP_KEY_TYPE, "Application"},
                         {G_KEY_FILE_DESKTOP_KEY_ICON, "foo.png"},
                         {G_KEY_FILE_DESKTOP_KEY_EXEC, "foo"},
                     }}});

    auto store = std::make_shared<ubuntu::app_launch::app_store::Legacy>(registry->impl);

    std::promise<std::string> removedAppId;
    store->appRemoved().connect([&](const ubuntu::app_launch::AppID &appid) { removedAppId.set_value(appid); });

    testdir.removeApp("testapp");

    EXPECT_EVENTUALLY_FUTURE_EQ(std::string{"testapp"}, removedAppId.get_future());
}

TEST_F(AppStoreLegacy, AddedApp)
{
    TestDirectory testdir;
    auto store = std::make_shared<ubuntu::app_launch::app_store::Legacy>(registry->impl);

    std::promise<std::string> addedAppId;
    store->appAdded().connect(
        [&](const std::shared_ptr<ubuntu::app_launch::Application> &app) { addedAppId.set_value(app->appId()); });

    testdir.addApp("testapp",
                   {{G_KEY_FILE_DESKTOP_GROUP,
                     {
                         {G_KEY_FILE_DESKTOP_KEY_NAME, "Test App"},
                         {G_KEY_FILE_DESKTOP_KEY_TYPE, "Application"},
                         {G_KEY_FILE_DESKTOP_KEY_ICON, "foo.png"},
                         {G_KEY_FILE_DESKTOP_KEY_EXEC, "foo"},
                     }}});

    EXPECT_EVENTUALLY_FUTURE_EQ(std::string{"testapp"}, addedAppId.get_future());
}

TEST_F(AppStoreLegacy, ShadowDelete)
{
    TestDirectory testdir;
    testdir.addApp("testapp",
                   {{G_KEY_FILE_DESKTOP_GROUP,
                     {
                         {G_KEY_FILE_DESKTOP_KEY_NAME, "Test App"},
                         {G_KEY_FILE_DESKTOP_KEY_TYPE, "Application"},
                         {G_KEY_FILE_DESKTOP_KEY_ICON, "foo.png"},
                         {G_KEY_FILE_DESKTOP_KEY_EXEC, "foo"},
                     }}});

    TestDirectory testdir2;
    testdir2.addApp("testapp",
                    {{G_KEY_FILE_DESKTOP_GROUP,
                      {
                          {G_KEY_FILE_DESKTOP_KEY_NAME, "Test App"},
                          {G_KEY_FILE_DESKTOP_KEY_TYPE, "Application"},
                          {G_KEY_FILE_DESKTOP_KEY_ICON, "foo.png"},
                          {G_KEY_FILE_DESKTOP_KEY_EXEC, "foo"},
                      }}});

    auto store = std::make_shared<ubuntu::app_launch::app_store::Legacy>(registry->impl);

    std::promise<std::string> updatedAppId;
    store->infoChanged().connect(
        [&](const std::shared_ptr<ubuntu::app_launch::Application> &app) { updatedAppId.set_value(app->appId()); });

    std::promise<std::string> deleteAppId;
    store->appRemoved().connect([&](const ubuntu::app_launch::AppID &appid) { deleteAppId.set_value(appid); });
    std::shared_future<std::string> deleteFuture = deleteAppId.get_future();

    testdir.removeApp("testapp");

    EXPECT_EVENTUALLY_FUTURE_EQ(std::string{"testapp"}, updatedAppId.get_future());
    EXPECT_NE(std::future_status::ready, deleteFuture.wait_for(std::chrono::seconds{0}));

    testdir2.removeApp("testapp");

    EXPECT_EVENTUALLY_FUTURE_EQ(std::string{"testapp"}, deleteFuture);
}
