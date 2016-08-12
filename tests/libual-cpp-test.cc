/*
 * Copyright 2013 Canonical Ltd.
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

#include <algorithm>
#include <functional>
#include <future>
#include <numeric>
#include <thread>

#include "eventually-fixture.h"
#include "mir-mock.h"
#include <gio/gio.h>
#include <gtest/gtest.h>
#include <zeitgeist.h>

#include "application.h"
#include "glib-thread.h"
#include "helper.h"
#include "registry.h"

extern "C" {
#include "libdbustest/dbus-test.h"
#include "ubuntu-app-launch.h"
#include <fcntl.h>
}

class LibUAL : public EventuallyFixture
{
protected:
    DbusTestService* service = NULL;
    DbusTestDbusMock* mock = NULL;
    DbusTestDbusMock* cgmock = NULL;
    GDBusConnection* bus = NULL;
    std::string last_focus_appid;
    std::string last_resume_appid;
    guint resume_timeout = 0;
    std::shared_ptr<ubuntu::app_launch::Registry> registry;

private:
    static void focus_cb(const gchar* appid, gpointer user_data)
    {
        g_debug("Focus Callback: %s", appid);
        LibUAL* _this = static_cast<LibUAL*>(user_data);
        _this->last_focus_appid = appid;
    }

    static void resume_cb(const gchar* appid, gpointer user_data)
    {
        g_debug("Resume Callback: %s", appid);
        LibUAL* _this = static_cast<LibUAL*>(user_data);
        _this->last_resume_appid = appid;

        if (_this->resume_timeout > 0)
        {
            _this->pause(_this->resume_timeout);
        }
    }

protected:
    /* Useful debugging stuff, but not on by default.  You really want to
       not get all this noise typically */
    void debugConnection()
    {
        if (true)
        {
            return;
        }

        DbusTestBustle* bustle = dbus_test_bustle_new("test.bustle");
        dbus_test_service_add_task(service, DBUS_TEST_TASK(bustle));
        g_object_unref(bustle);

        DbusTestProcess* monitor = dbus_test_process_new("dbus-monitor");
        dbus_test_service_add_task(service, DBUS_TEST_TASK(monitor));
        g_object_unref(monitor);
    }

    virtual void SetUp()
    {
        /* Click DB test mode */
        g_setenv("TEST_CLICK_DB", "click-db-dir", TRUE);
        g_setenv("TEST_CLICK_USER", "test-user", TRUE);

        gchar* linkfarmpath = g_build_filename(CMAKE_SOURCE_DIR, "link-farm", NULL);
        g_setenv("UBUNTU_APP_LAUNCH_LINK_FARM", linkfarmpath, TRUE);
        g_free(linkfarmpath);

        g_setenv("XDG_DATA_DIRS", CMAKE_SOURCE_DIR, TRUE);
        g_setenv("XDG_CACHE_HOME", CMAKE_SOURCE_DIR "/libertine-data", TRUE);
        g_setenv("XDG_DATA_HOME", CMAKE_SOURCE_DIR "/libertine-home", TRUE);

        service = dbus_test_service_new(NULL);

        debugConnection();

        mock = dbus_test_dbus_mock_new("com.ubuntu.Upstart");

        DbusTestDbusMockObject* obj =
            dbus_test_dbus_mock_get_object(mock, "/com/ubuntu/Upstart", "com.ubuntu.Upstart0_6", NULL);

        dbus_test_dbus_mock_object_add_method(mock, obj, "EmitEvent", G_VARIANT_TYPE("(sasb)"), NULL, "", NULL);

        dbus_test_dbus_mock_object_add_method(mock, obj, "GetJobByName", G_VARIANT_TYPE("s"), G_VARIANT_TYPE("o"),
                                              "if args[0] == 'application-click':\n"
                                              "	ret = dbus.ObjectPath('/com/test/application_click')\n"
                                              "elif args[0] == 'application-legacy':\n"
                                              "	ret = dbus.ObjectPath('/com/test/application_legacy')\n"
                                              "elif args[0] == 'untrusted-helper':\n"
                                              "	ret = dbus.ObjectPath('/com/test/untrusted/helper')\n",
                                              NULL);

        dbus_test_dbus_mock_object_add_method(mock, obj, "SetEnv", G_VARIANT_TYPE("(assb)"), NULL, "", NULL);

        /* Click App */
        DbusTestDbusMockObject* jobobj =
            dbus_test_dbus_mock_get_object(mock, "/com/test/application_click", "com.ubuntu.Upstart0_6.Job", NULL);

        dbus_test_dbus_mock_object_add_method(
            mock, jobobj, "Start", G_VARIANT_TYPE("(asb)"), NULL,
            "if args[0][0] == 'APP_ID=com.test.good_application_1.2.3':"
            "    raise dbus.exceptions.DBusException('Foo running', name='com.ubuntu.Upstart0_6.Error.AlreadyStarted')",
            NULL);

        dbus_test_dbus_mock_object_add_method(mock, jobobj, "Stop", G_VARIANT_TYPE("(asb)"), NULL, "", NULL);

        dbus_test_dbus_mock_object_add_method(mock, jobobj, "GetAllInstances", NULL, G_VARIANT_TYPE("ao"),
                                              "ret = [ dbus.ObjectPath('/com/test/app_instance') ]", NULL);

        dbus_test_dbus_mock_object_add_method(mock, jobobj, "GetInstanceByName", G_VARIANT_TYPE_STRING,
                                              G_VARIANT_TYPE("o"), "ret = dbus.ObjectPath('/com/test/app_instance')",
                                              NULL);

        DbusTestDbusMockObject* instobj =
            dbus_test_dbus_mock_get_object(mock, "/com/test/app_instance", "com.ubuntu.Upstart0_6.Instance", NULL);
        dbus_test_dbus_mock_object_add_property(mock, instobj, "name", G_VARIANT_TYPE_STRING,
                                                g_variant_new_string("com.test.good_application_1.2.3"), NULL);
        gchar* process_var = g_strdup_printf("[('main', %d)]", getpid());
        dbus_test_dbus_mock_object_add_property(mock, instobj, "processes", G_VARIANT_TYPE("a(si)"),
                                                g_variant_new_parsed(process_var), NULL);
        g_free(process_var);

        /*  Legacy App */
        DbusTestDbusMockObject* ljobobj =
            dbus_test_dbus_mock_get_object(mock, "/com/test/application_legacy", "com.ubuntu.Upstart0_6.Job", NULL);

        dbus_test_dbus_mock_object_add_method(mock, ljobobj, "Start", G_VARIANT_TYPE("(asb)"), NULL, "", NULL);

        dbus_test_dbus_mock_object_add_method(mock, ljobobj, "Stop", G_VARIANT_TYPE("(asb)"), NULL, "", NULL);

        dbus_test_dbus_mock_object_add_method(mock, ljobobj, "GetAllInstances", NULL, G_VARIANT_TYPE("ao"),
                                              "ret = [ dbus.ObjectPath('/com/test/legacy_app_instance'), "
                                              "dbus.ObjectPath('/com/test/legacy_app_instance2') ]",
                                              NULL);

        dbus_test_dbus_mock_object_add_method(mock, ljobobj, "GetInstanceByName", G_VARIANT_TYPE_STRING,
                                              G_VARIANT_TYPE("o"),
                                              "ret = dbus.ObjectPath('/com/test/legacy_app_instance')", NULL);

        DbusTestDbusMockObject* linstobj = dbus_test_dbus_mock_get_object(mock, "/com/test/legacy_app_instance",
                                                                          "com.ubuntu.Upstart0_6.Instance", NULL);
        dbus_test_dbus_mock_object_add_property(mock, linstobj, "name", G_VARIANT_TYPE_STRING,
                                                g_variant_new_string("multiple-2342345"), NULL);
        dbus_test_dbus_mock_object_add_property(mock, linstobj, "processes", G_VARIANT_TYPE("a(si)"),
                                                g_variant_new_parsed("[('main', 5678)]"), NULL);

        DbusTestDbusMockObject* linstobj2 = dbus_test_dbus_mock_get_object(mock, "/com/test/legacy_app_instance2",
                                                                           "com.ubuntu.Upstart0_6.Instance", NULL);
        dbus_test_dbus_mock_object_add_property(mock, linstobj2, "name", G_VARIANT_TYPE_STRING,
                                                g_variant_new_string("single-"), NULL);
        dbus_test_dbus_mock_object_add_property(mock, linstobj2, "processes", G_VARIANT_TYPE("a(si)"),
                                                g_variant_new_parsed("[('main', 5678)]"), NULL);

        /*  Untrusted Helper */
        DbusTestDbusMockObject* uhelperobj =
            dbus_test_dbus_mock_get_object(mock, "/com/test/untrusted/helper", "com.ubuntu.Upstart0_6.Job", NULL);

        dbus_test_dbus_mock_object_add_method(mock, uhelperobj, "Start", G_VARIANT_TYPE("(asb)"), NULL, "", NULL);

        dbus_test_dbus_mock_object_add_method(mock, uhelperobj, "Stop", G_VARIANT_TYPE("(asb)"), NULL, "", NULL);

        dbus_test_dbus_mock_object_add_method(mock, uhelperobj, "GetAllInstances", NULL, G_VARIANT_TYPE("ao"),
                                              "ret = [ dbus.ObjectPath('/com/test/untrusted/helper/instance'), "
                                              "dbus.ObjectPath('/com/test/untrusted/helper/multi_instance') ]",
                                              NULL);

        DbusTestDbusMockObject* uhelperinstance = dbus_test_dbus_mock_get_object(
            mock, "/com/test/untrusted/helper/instance", "com.ubuntu.Upstart0_6.Instance", NULL);
        dbus_test_dbus_mock_object_add_property(mock, uhelperinstance, "name", G_VARIANT_TYPE_STRING,
                                                g_variant_new_string("untrusted-type::com.foo_bar_43.23.12"), NULL);

        DbusTestDbusMockObject* unhelpermulti = dbus_test_dbus_mock_get_object(
            mock, "/com/test/untrusted/helper/multi_instance", "com.ubuntu.Upstart0_6.Instance", NULL);
        dbus_test_dbus_mock_object_add_property(
            mock, unhelpermulti, "name", G_VARIANT_TYPE_STRING,
            g_variant_new_string("untrusted-type:24034582324132:com.bar_foo_8432.13.1"), NULL);

        /* Create the cgroup manager mock */
        cgmock = dbus_test_dbus_mock_new("org.test.cgmock");
        g_setenv("UBUNTU_APP_LAUNCH_CG_MANAGER_NAME", "org.test.cgmock", TRUE);

        DbusTestDbusMockObject* cgobject = dbus_test_dbus_mock_get_object(cgmock, "/org/linuxcontainers/cgmanager",
                                                                          "org.linuxcontainers.cgmanager0_0", NULL);
        dbus_test_dbus_mock_object_add_method(cgmock, cgobject, "GetTasksRecursive", G_VARIANT_TYPE("(ss)"),
                                              G_VARIANT_TYPE("ai"), "ret = [100, 200, 300]", NULL);

        /* Put it together */
        dbus_test_service_add_task(service, DBUS_TEST_TASK(mock));
        dbus_test_service_add_task(service, DBUS_TEST_TASK(cgmock));
        dbus_test_service_start_tasks(service);

        bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
        g_dbus_connection_set_exit_on_close(bus, FALSE);
        g_object_add_weak_pointer(G_OBJECT(bus), (gpointer*)&bus);

        /* Make sure we pretend the CG manager is just on our bus */
        g_setenv("UBUNTU_APP_LAUNCH_CG_MANAGER_SESSION_BUS", "YES", TRUE);

        ASSERT_TRUE(ubuntu_app_launch_observer_add_app_focus(focus_cb, this));
        ASSERT_TRUE(ubuntu_app_launch_observer_add_app_resume(resume_cb, this));

        registry = std::make_shared<ubuntu::app_launch::Registry>();
    }

    virtual void TearDown()
    {
        ubuntu_app_launch_observer_delete_app_focus(focus_cb, this);
        ubuntu_app_launch_observer_delete_app_resume(resume_cb, this);

        registry.reset();

        g_clear_object(&mock);
        g_clear_object(&cgmock);
        g_clear_object(&service);

        g_object_unref(bus);

        EXPECT_EVENTUALLY_EQ(nullptr, bus);
    }

    GVariant* find_env(GVariant* env_array, const gchar* var)
    {
        unsigned int i;
        GVariant* retval = nullptr;

        for (i = 0; i < g_variant_n_children(env_array); i++)
        {
            GVariant* child = g_variant_get_child_value(env_array, i);
            const gchar* envvar = g_variant_get_string(child, nullptr);

            if (g_str_has_prefix(envvar, var))
            {
                if (retval != nullptr)
                {
                    g_warning("Found the env var more than once!");
                    g_variant_unref(retval);
                    return nullptr;
                }

                retval = child;
            }
            else
            {
                g_variant_unref(child);
            }
        }

        if (!retval)
        {
            gchar* envstr = g_variant_print(env_array, FALSE);
            g_warning("Unable to find '%s' in '%s'", var, envstr);
            g_free(envstr);
        }

        return retval;
    }

    bool check_env(GVariant* env_array, const gchar* var, const gchar* value)
    {
        bool found = false;
        GVariant* val = find_env(env_array, var);
        if (val == nullptr)
        {
            return false;
        }

        const gchar* envvar = g_variant_get_string(val, nullptr);

        gchar* combined = g_strdup_printf("%s=%s", var, value);
        if (g_strcmp0(envvar, combined) == 0)
        {
            found = true;
        }

        g_variant_unref(val);

        return found;
    }
};

TEST_F(LibUAL, StartApplication)
{
    DbusTestDbusMockObject* obj =
        dbus_test_dbus_mock_get_object(mock, "/com/test/application_click", "com.ubuntu.Upstart0_6.Job", NULL);

    /* Basic make sure we can send the event */
    auto appid = ubuntu::app_launch::AppID::parse("com.test.multiple_first_1.2.3");
    auto app = ubuntu::app_launch::Application::create(appid, registry);
    app->launch();

    EXPECT_EQ(1, dbus_test_dbus_mock_object_check_method_call(mock, obj, "Start", NULL, NULL));

    ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(mock, obj, NULL));

    /* Now look at the details of the call */
    app->launch();

    guint len = 0;
    const DbusTestDbusMockCall* calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Start", &len, NULL);
    EXPECT_NE(nullptr, calls);
    EXPECT_EQ(1, len);

    EXPECT_STREQ("Start", calls->name);
    EXPECT_EQ(2, g_variant_n_children(calls->params));

    GVariant* block = g_variant_get_child_value(calls->params, 1);
    EXPECT_TRUE(g_variant_get_boolean(block));
    g_variant_unref(block);

    GVariant* env = g_variant_get_child_value(calls->params, 0);
    EXPECT_TRUE(check_env(env, "APP_ID", "com.test.multiple_first_1.2.3"));
    g_variant_unref(env);

    ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(mock, obj, NULL));

    /* Let's pass some URLs */
    std::vector<ubuntu::app_launch::Application::URL> urls{
        ubuntu::app_launch::Application::URL::from_raw("http://ubuntu.com/"),
        ubuntu::app_launch::Application::URL::from_raw("https://ubuntu.com/"),
        ubuntu::app_launch::Application::URL::from_raw("file:///home/phablet/test.txt")};

    app->launch(urls);

    len = 0;
    calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Start", &len, NULL);
    EXPECT_NE(nullptr, calls);
    EXPECT_EQ(1, len);

    env = g_variant_get_child_value(calls->params, 0);
    EXPECT_TRUE(check_env(env, "APP_ID", "com.test.multiple_first_1.2.3"));
    EXPECT_TRUE(
        check_env(env, "APP_URIS", "'http://ubuntu.com/' 'https://ubuntu.com/' 'file:///home/phablet/test.txt'"));
    g_variant_unref(env);

    return;
}

TEST_F(LibUAL, StartApplicationTest)
{
    DbusTestDbusMockObject* obj =
        dbus_test_dbus_mock_get_object(mock, "/com/test/application_click", "com.ubuntu.Upstart0_6.Job", NULL);

    /* Basic make sure we can send the event */
    auto appid = ubuntu::app_launch::AppID::parse("com.test.multiple_first_1.2.3");
    auto app = ubuntu::app_launch::Application::create(appid, registry);
    app->launchTest();

    guint len = 0;
    const DbusTestDbusMockCall* calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Start", &len, NULL);
    EXPECT_NE(nullptr, calls);
    EXPECT_EQ(1, len);

    EXPECT_STREQ("Start", calls->name);
    EXPECT_EQ(2, g_variant_n_children(calls->params));

    GVariant* block = g_variant_get_child_value(calls->params, 1);
    EXPECT_TRUE(g_variant_get_boolean(block));
    g_variant_unref(block);

    GVariant* env = g_variant_get_child_value(calls->params, 0);
    EXPECT_TRUE(check_env(env, "APP_ID", "com.test.multiple_first_1.2.3"));
    EXPECT_TRUE(check_env(env, "QT_LOAD_TESTABILITY", "1"));
    g_variant_unref(env);
}

TEST_F(LibUAL, StopApplication)
{
    DbusTestDbusMockObject* obj =
        dbus_test_dbus_mock_get_object(mock, "/com/test/application_click", "com.ubuntu.Upstart0_6.Job", NULL);

    auto appid = ubuntu::app_launch::AppID::parse("com.test.good_application_1.2.3");
    auto app = ubuntu::app_launch::Application::create(appid, registry);

    ASSERT_TRUE(app->hasInstances());
    EXPECT_EQ(1, app->instances().size());

    app->instances()[0]->stop();

    ASSERT_EQ(dbus_test_dbus_mock_object_check_method_call(mock, obj, "Stop", NULL, NULL), 1);
}

/* NOTE: The fact that there is 'libertine-data' in these strings is because
   we're using one CACHE_HOME for this test suite and the libertine functions
   need to pull things from there, where these are only comparisons. It's just
   what value is in the environment variable */
TEST_F(LibUAL, ApplicationLog)
{
    auto appid = ubuntu::app_launch::AppID::parse("com.test.good_application_1.2.3");
    auto app = ubuntu::app_launch::Application::create(appid, registry);

    EXPECT_EQ(
        std::string(CMAKE_SOURCE_DIR "/libertine-data/upstart/application-click-com.test.good_application_1.2.3.log"),
        app->instances()[0]->logPath());

    appid = ubuntu::app_launch::AppID::find("single");
    app = ubuntu::app_launch::Application::create(appid, registry);

    ASSERT_LT(0, app->instances().size());

    EXPECT_EQ(std::string(CMAKE_SOURCE_DIR "/libertine-data/upstart/application-legacy-single-.log"),
              app->instances()[0]->logPath());

    appid = ubuntu::app_launch::AppID::find("multiple");
    app = ubuntu::app_launch::Application::create(appid, registry);

    EXPECT_EQ(std::string(CMAKE_SOURCE_DIR "/libertine-data/upstart/application-legacy-multiple-2342345.log"),
              app->instances()[0]->logPath());
}

TEST_F(LibUAL, ApplicationPid)
{
    /* Check bad params */
    auto appid = ubuntu::app_launch::AppID::parse("com.test.good_application_1.2.3");
    auto app = ubuntu::app_launch::Application::create(appid, registry);

    ASSERT_LT(0, app->instances().size());

    /* Look at PIDs from cgmanager */
    EXPECT_FALSE(app->instances()[0]->hasPid(1));
    EXPECT_TRUE(app->instances()[0]->hasPid(100));
    EXPECT_TRUE(app->instances()[0]->hasPid(200));
    EXPECT_TRUE(app->instances()[0]->hasPid(300));

    /* Check primary pid, which comes from Upstart */
    EXPECT_EQ(getpid(), app->instances()[0]->primaryPid());

    auto multiappid = ubuntu::app_launch::AppID::find("multiple");
    auto multiapp = ubuntu::app_launch::Application::create(multiappid, registry);
    auto instances = multiapp->instances();
    ASSERT_LT(0, instances.size());
    EXPECT_EQ(5678, instances[0]->primaryPid());

    /* Look at the full PID list from CG Manager */
    DbusTestDbusMockObject* cgobject = dbus_test_dbus_mock_get_object(cgmock, "/org/linuxcontainers/cgmanager",
                                                                      "org.linuxcontainers.cgmanager0_0", NULL);
    const DbusTestDbusMockCall* calls = NULL;
    guint len = 0;

    /* Click in the set */
    ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(cgmock, cgobject, NULL));
    EXPECT_TRUE(app->instances()[0]->hasPid(100));
    calls = dbus_test_dbus_mock_object_get_method_calls(cgmock, cgobject, "GetTasksRecursive", &len, NULL);
    EXPECT_EQ(1, len);
    EXPECT_STREQ("GetTasksRecursive", calls->name);
    EXPECT_TRUE(g_variant_equal(
        calls->params, g_variant_new("(ss)", "freezer", "upstart/application-click-com.test.good_application_1.2.3")));
    ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(cgmock, cgobject, NULL));

    /* Click out of the set */
    EXPECT_FALSE(app->instances()[0]->hasPid(101));
    calls = dbus_test_dbus_mock_object_get_method_calls(cgmock, cgobject, "GetTasksRecursive", &len, NULL);
    EXPECT_EQ(1, len);
    EXPECT_STREQ("GetTasksRecursive", calls->name);
    EXPECT_TRUE(g_variant_equal(
        calls->params, g_variant_new("(ss)", "freezer", "upstart/application-click-com.test.good_application_1.2.3")));
    ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(cgmock, cgobject, NULL));

    /* Legacy Single Instance */
    auto singleappid = ubuntu::app_launch::AppID::find("single");
    auto singleapp = ubuntu::app_launch::Application::create(singleappid, registry);

    ASSERT_LT(0, singleapp->instances().size());
    EXPECT_TRUE(singleapp->instances()[0]->hasPid(100));

    calls = dbus_test_dbus_mock_object_get_method_calls(cgmock, cgobject, "GetTasksRecursive", &len, NULL);
    EXPECT_EQ(1, len);
    EXPECT_STREQ("GetTasksRecursive", calls->name);
    EXPECT_TRUE(g_variant_equal(calls->params, g_variant_new("(ss)", "freezer", "upstart/application-legacy-single-")));
    ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(cgmock, cgobject, NULL));

    /* Legacy Multi Instance */
    EXPECT_TRUE(multiapp->instances()[0]->hasPid(100));
    calls = dbus_test_dbus_mock_object_get_method_calls(cgmock, cgobject, "GetTasksRecursive", &len, NULL);
    EXPECT_EQ(1, len);
    EXPECT_STREQ("GetTasksRecursive", calls->name);
    EXPECT_TRUE(g_variant_equal(calls->params,
                                g_variant_new("(ss)", "freezer", "upstart/application-legacy-multiple-2342345")));
    ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(cgmock, cgobject, NULL));
}

TEST_F(LibUAL, ApplicationId)
{
    g_setenv("TEST_CLICK_DB", "click-db-dir", TRUE);
    g_setenv("TEST_CLICK_USER", "test-user", TRUE);

    /* Test with current-user-version, should return the version in the manifest */
    EXPECT_EQ("com.test.good_application_1.2.3",
              (std::string)ubuntu::app_launch::AppID::discover("com.test.good", "application"));

    /* Test with version specified, shouldn't even read the manifest */
    EXPECT_EQ("com.test.good_application_1.2.4",
              (std::string)ubuntu::app_launch::AppID::discover("com.test.good", "application", "1.2.4"));

    /* Test with out a version or app, should return the version in the manifest */
    EXPECT_EQ("com.test.good_application_1.2.3", (std::string)ubuntu::app_launch::AppID::discover(
                                                     "com.test.good", "first-listed-app", "current-user-version"));

    /* Make sure we can select the app from a list correctly */
    EXPECT_EQ("com.test.multiple_first_1.2.3",
              (std::string)ubuntu::app_launch::AppID::discover(
                  "com.test.multiple", ubuntu::app_launch::AppID::ApplicationWildcard::FIRST_LISTED));
    EXPECT_EQ("com.test.multiple_first_1.2.3", (std::string)ubuntu::app_launch::AppID::discover("com.test.multiple"));
    EXPECT_EQ("com.test.multiple_fifth_1.2.3",
              (std::string)ubuntu::app_launch::AppID::discover(
                  "com.test.multiple", ubuntu::app_launch::AppID::ApplicationWildcard::LAST_LISTED));
    EXPECT_EQ("", (std::string)ubuntu::app_launch::AppID::discover(
                      "com.test.multiple", ubuntu::app_launch::AppID::ApplicationWildcard::ONLY_LISTED));
    EXPECT_EQ("com.test.good_application_1.2.3",
              (std::string)ubuntu::app_launch::AppID::discover(
                  "com.test.good", ubuntu::app_launch::AppID::ApplicationWildcard::ONLY_LISTED));

    /* A bunch that should be NULL */
    EXPECT_EQ("", (std::string)ubuntu::app_launch::AppID::discover("com.test.no-hooks"));
    EXPECT_EQ("", (std::string)ubuntu::app_launch::AppID::discover("com.test.no-json"));
    EXPECT_EQ("", (std::string)ubuntu::app_launch::AppID::discover("com.test.no-object"));
    EXPECT_EQ("", (std::string)ubuntu::app_launch::AppID::discover("com.test.no-version"));

    /* Libertine tests */
    EXPECT_EQ("", (std::string)ubuntu::app_launch::AppID::discover("container-name"));
    EXPECT_EQ("", (std::string)ubuntu::app_launch::AppID::discover("container-name", "not-exist"));
    EXPECT_EQ("container-name_test_0.0", (std::string)ubuntu::app_launch::AppID::discover("container-name", "test"));
    EXPECT_EQ("container-name_user-app_0.0",
              (std::string)ubuntu::app_launch::AppID::discover("container-name", "user-app"));
}

TEST_F(LibUAL, AppIdParse)
{
    EXPECT_FALSE(ubuntu::app_launch::AppID::parse("com.ubuntu.test_test_123").empty());
    EXPECT_FALSE(ubuntu::app_launch::AppID::find("inkscape").empty());
    EXPECT_FALSE(ubuntu::app_launch::AppID::parse("chatter.robert-ancell_chatter_2").empty());
    EXPECT_FALSE(ubuntu::app_launch::AppID::find("chatter.robert-ancell_chatter").empty());

    auto id = ubuntu::app_launch::AppID::parse("com.ubuntu.test_test_123");

    ASSERT_FALSE(id.empty());
    EXPECT_EQ("com.ubuntu.test", id.package.value());
    EXPECT_EQ("test", id.appname.value());
    EXPECT_EQ("123", id.version.value());

    return;
}

TEST_F(LibUAL, ApplicationList)
{
    auto apps = ubuntu::app_launch::Registry::runningApps(registry);

    ASSERT_EQ(3, apps.size());

    apps.sort([](const std::shared_ptr<ubuntu::app_launch::Application>& a,
                 const std::shared_ptr<ubuntu::app_launch::Application>& b) {
        std::string sa = a->appId();
        std::string sb = b->appId();

        return sa < sb;
    });

    EXPECT_EQ("com.test.good_application_1.2.3", (std::string)apps.front()->appId());
    EXPECT_EQ("single", (std::string)apps.back()->appId());
}

typedef struct
{
    unsigned int count;
    const gchar* name;
} observer_data_t;

static void observer_cb(const gchar* appid, gpointer user_data)
{
    observer_data_t* data = (observer_data_t*)user_data;

    if (data->name == NULL)
    {
        data->count++;
    }
    else if (g_strcmp0(data->name, appid) == 0)
    {
        data->count++;
    }
}

TEST_F(LibUAL, StartStopObserver)
{
    observer_data_t start_data = {.count = 0, .name = nullptr};
    observer_data_t stop_data = {.count = 0, .name = nullptr};

    ASSERT_TRUE(ubuntu_app_launch_observer_add_app_started(observer_cb, &start_data));
    ASSERT_TRUE(ubuntu_app_launch_observer_add_app_stop(observer_cb, &stop_data));

    DbusTestDbusMockObject* obj =
        dbus_test_dbus_mock_get_object(mock, "/com/ubuntu/Upstart", "com.ubuntu.Upstart0_6", NULL);

    /* Basic start */
    dbus_test_dbus_mock_object_emit_signal(
        mock, obj, "EventEmitted", G_VARIANT_TYPE("(sas)"),
        g_variant_new_parsed("('started', ['JOB=application-click', 'INSTANCE=com.test.good_application_1.2.3'])"),
        NULL);

    EXPECT_EVENTUALLY_EQ(1, start_data.count);

    /* Basic stop */
    dbus_test_dbus_mock_object_emit_signal(
        mock, obj, "EventEmitted", G_VARIANT_TYPE("(sas)"),
        g_variant_new_parsed("('stopped', ['JOB=application-click', 'INSTANCE=com.test.good_application_1.2.3'])"),
        NULL);

    EXPECT_EVENTUALLY_EQ(1, stop_data.count);

    /* Start legacy */
    start_data.count = 0;
    start_data.name = "multiple";

    dbus_test_dbus_mock_object_emit_signal(
        mock, obj, "EventEmitted", G_VARIANT_TYPE("(sas)"),
        g_variant_new_parsed("('started', ['JOB=application-legacy', 'INSTANCE=multiple-234235'])"), NULL);

    EXPECT_EVENTUALLY_EQ(1, start_data.count);

    /* Legacy stop */
    stop_data.count = 0;
    stop_data.name = "bar";

    dbus_test_dbus_mock_object_emit_signal(
        mock, obj, "EventEmitted", G_VARIANT_TYPE("(sas)"),
        g_variant_new_parsed("('stopped', ['JOB=application-legacy', 'INSTANCE=bar-9344321'])"), NULL);

    EXPECT_EVENTUALLY_EQ(1, stop_data.count);

    /* Test Noise Start */
    start_data.count = 0;
    start_data.name = "com.test.good_application_1.2.3";
    stop_data.count = 0;
    stop_data.name = "com.test.good_application_1.2.3";

    /* A full lifecycle */
    dbus_test_dbus_mock_object_emit_signal(
        mock, obj, "EventEmitted", G_VARIANT_TYPE("(sas)"),
        g_variant_new_parsed("('starting', ['JOB=application-click', 'INSTANCE=com.test.good_application_1.2.3'])"),
        NULL);
    dbus_test_dbus_mock_object_emit_signal(
        mock, obj, "EventEmitted", G_VARIANT_TYPE("(sas)"),
        g_variant_new_parsed("('started', ['JOB=application-click', 'INSTANCE=com.test.good_application_1.2.3'])"),
        NULL);
    dbus_test_dbus_mock_object_emit_signal(
        mock, obj, "EventEmitted", G_VARIANT_TYPE("(sas)"),
        g_variant_new_parsed("('stopping', ['JOB=application-click', 'INSTANCE=com.test.good_application_1.2.3'])"),
        NULL);
    dbus_test_dbus_mock_object_emit_signal(
        mock, obj, "EventEmitted", G_VARIANT_TYPE("(sas)"),
        g_variant_new_parsed("('stopped', ['JOB=application-click', 'INSTANCE=com.test.good_application_1.2.3'])"),
        NULL);

    /* Ensure we just signaled once for each */
    EXPECT_EVENTUALLY_EQ(1, start_data.count);
    EXPECT_EVENTUALLY_EQ(1, stop_data.count);

    /* Remove */
    ASSERT_TRUE(ubuntu_app_launch_observer_delete_app_started(observer_cb, &start_data));
    ASSERT_TRUE(ubuntu_app_launch_observer_delete_app_stop(observer_cb, &stop_data));
}

static GDBusMessage* filter_starting(GDBusConnection* conn,
                                     GDBusMessage* message,
                                     gboolean incomming,
                                     gpointer user_data)
{
    if (g_strcmp0(g_dbus_message_get_member(message), "UnityStartingSignal") == 0)
    {
        unsigned int* count = static_cast<unsigned int*>(user_data);
        (*count)++;
        g_object_unref(message);
        return NULL;
    }

    return message;
}

static void starting_observer(const gchar* appid, gpointer user_data)
{
    std::string* last = static_cast<std::string*>(user_data);
    *last = appid;
    return;
}

TEST_F(LibUAL, StartingResponses)
{
    std::string last_observer;
    unsigned int starting_count = 0;
    GDBusConnection* session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
    guint filter = g_dbus_connection_add_filter(session, filter_starting, &starting_count, NULL);

    EXPECT_TRUE(ubuntu_app_launch_observer_add_app_starting(starting_observer, &last_observer));

    g_dbus_connection_emit_signal(session, NULL,                                           /* destination */
                                  "/",                                                     /* path */
                                  "com.canonical.UbuntuAppLaunch",                         /* interface */
                                  "UnityStartingBroadcast",                                /* signal */
                                  g_variant_new("(s)", "com.test.good_application_1.2.3"), /* params, the same */
                                  NULL);

    EXPECT_EVENTUALLY_EQ("com.test.good_application_1.2.3", last_observer);
    EXPECT_EVENTUALLY_EQ(1, starting_count);

    EXPECT_TRUE(ubuntu_app_launch_observer_delete_app_starting(starting_observer, &last_observer));

    g_dbus_connection_remove_filter(session, filter);
    g_object_unref(session);
}

TEST_F(LibUAL, AppIdTest)
{
    auto appid = ubuntu::app_launch::AppID::parse("com.test.good_application_1.2.3");
    auto app = ubuntu::app_launch::Application::create(appid, registry);
    app->launch();

    EXPECT_EVENTUALLY_EQ("com.test.good_application_1.2.3", this->last_focus_appid);
    EXPECT_EVENTUALLY_EQ("com.test.good_application_1.2.3", this->last_resume_appid);
}

GDBusMessage* filter_func_good(GDBusConnection* conn, GDBusMessage* message, gboolean incomming, gpointer user_data)
{
    if (!incomming)
    {
        return message;
    }

    if (g_strcmp0(g_dbus_message_get_path(message), (gchar*)user_data) == 0)
    {
        GDBusMessage* reply = g_dbus_message_new_method_reply(message);
        g_dbus_connection_send_message(conn, reply, G_DBUS_SEND_MESSAGE_FLAGS_NONE, NULL, NULL);
        g_object_unref(message);
        return NULL;
    }

    return message;
}

TEST_F(LibUAL, UrlSendTest)
{
    GDBusConnection* session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
    guint filter = g_dbus_connection_add_filter(session, filter_func_good,
                                                (gpointer) "/com_2etest_2egood_5fapplication_5f1_2e2_2e3", NULL);

    auto appid = ubuntu::app_launch::AppID::parse("com.test.good_application_1.2.3");
    auto app = ubuntu::app_launch::Application::create(appid, registry);
    std::vector<ubuntu::app_launch::Application::URL> uris = {
        ubuntu::app_launch::Application::URL::from_raw("http://www.test.com")};

    app->launch(uris);

    EXPECT_EVENTUALLY_EQ("com.test.good_application_1.2.3", this->last_focus_appid);
    EXPECT_EVENTUALLY_EQ("com.test.good_application_1.2.3", this->last_resume_appid);

    g_dbus_connection_remove_filter(session, filter);

    /* Send multiple resume responses to ensure we unsubscribe */
    /* Multiple to increase our chance of hitting a bad free in the middle,
       fun with async! */
    int i;
    for (i = 0; i < 5; i++)
    {
        g_dbus_connection_emit_signal(session, NULL,                                           /* destination */
                                      "/",                                                     /* path */
                                      "com.canonical.UbuntuAppLaunch",                         /* interface */
                                      "UnityResumeResponse",                                   /* signal */
                                      g_variant_new("(s)", "com.test.good_application_1.2.3"), /* params, the same */
                                      NULL);

        pause(50); /* Ensure all the events come through */
    }

    g_object_unref(session);
}

TEST_F(LibUAL, UrlSendNoObjectTest)
{
    auto appid = ubuntu::app_launch::AppID::parse("com.test.good_application_1.2.3");
    auto app = ubuntu::app_launch::Application::create(appid, registry);
    std::vector<ubuntu::app_launch::Application::URL> uris = {
        ubuntu::app_launch::Application::URL::from_raw("http://www.test.com")};

    app->launch(uris);

    EXPECT_EVENTUALLY_EQ("com.test.good_application_1.2.3", this->last_focus_appid);
    EXPECT_EVENTUALLY_EQ("com.test.good_application_1.2.3", this->last_resume_appid);
}

TEST_F(LibUAL, UnityTimeoutTest)
{
    this->resume_timeout = 100;

    auto appid = ubuntu::app_launch::AppID::parse("com.test.good_application_1.2.3");
    auto app = ubuntu::app_launch::Application::create(appid, registry);

    app->launch();

    EXPECT_EVENTUALLY_EQ("com.test.good_application_1.2.3", this->last_resume_appid);
    EXPECT_EVENTUALLY_EQ("com.test.good_application_1.2.3", this->last_focus_appid);
}

TEST_F(LibUAL, UnityTimeoutUriTest)
{
    this->resume_timeout = 200;

    auto appid = ubuntu::app_launch::AppID::parse("com.test.good_application_1.2.3");
    auto app = ubuntu::app_launch::Application::create(appid, registry);
    std::vector<ubuntu::app_launch::Application::URL> uris = {
        ubuntu::app_launch::Application::URL::from_raw("http://www.test.com")};

    app->launch(uris);

    EXPECT_EVENTUALLY_EQ("com.test.good_application_1.2.3", this->last_focus_appid);
    EXPECT_EVENTUALLY_EQ("com.test.good_application_1.2.3", this->last_resume_appid);
}

GDBusMessage* filter_respawn(GDBusConnection* conn, GDBusMessage* message, gboolean incomming, gpointer user_data)
{
    if (g_strcmp0(g_dbus_message_get_member(message), "UnityResumeResponse") == 0)
    {
        g_object_unref(message);
        return NULL;
    }

    return message;
}

TEST_F(LibUAL, UnityLostTest)
{
    GDBusConnection* session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
    guint filter = g_dbus_connection_add_filter(session, filter_respawn, NULL, NULL);

    guint start = g_get_monotonic_time();

    auto appid = ubuntu::app_launch::AppID::parse("com.test.good_application_1.2.3");
    auto app = ubuntu::app_launch::Application::create(appid, registry);
    std::vector<ubuntu::app_launch::Application::URL> uris = {
        ubuntu::app_launch::Application::URL::from_raw("http://www.test.com")};

    app->launch(uris);

    guint end = g_get_monotonic_time();

    g_debug("Start call time: %d ms", (end - start) / 1000);
    EXPECT_LT(end - start, 2000 * 1000);

    EXPECT_EVENTUALLY_EQ("com.test.good_application_1.2.3", this->last_focus_appid);
    EXPECT_EVENTUALLY_EQ("com.test.good_application_1.2.3", this->last_resume_appid);

    g_dbus_connection_remove_filter(session, filter);
    g_object_unref(session);
}

TEST_F(LibUAL, LegacySingleInstance)
{
    DbusTestDbusMockObject* obj =
        dbus_test_dbus_mock_get_object(mock, "/com/test/application_legacy", "com.ubuntu.Upstart0_6.Job", NULL);

    /* Check for a single-instance app */
    auto singleappid = ubuntu::app_launch::AppID::find("single");
    auto singleapp = ubuntu::app_launch::Application::create(singleappid, registry);

    singleapp->launch();

    guint len = 0;
    const DbusTestDbusMockCall* calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Start", &len, NULL);
    EXPECT_NE(nullptr, calls);
    EXPECT_EQ(1, len);

    EXPECT_STREQ("Start", calls->name);
    EXPECT_EQ(2, g_variant_n_children(calls->params));

    GVariant* block = g_variant_get_child_value(calls->params, 1);
    EXPECT_TRUE(g_variant_get_boolean(block));
    g_variant_unref(block);

    GVariant* env = g_variant_get_child_value(calls->params, 0);
    EXPECT_TRUE(check_env(env, "APP_ID", "single"));
    EXPECT_TRUE(check_env(env, "INSTANCE_ID", ""));
    g_variant_unref(env);

    ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(mock, obj, NULL));

    /* Check for a multi-instance app */
    auto multipleappid = ubuntu::app_launch::AppID::find("multiple");
    auto multipleapp = ubuntu::app_launch::Application::create(multipleappid, registry);

    multipleapp->launch();

    len = 0;
    calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Start", &len, NULL);
    EXPECT_NE(nullptr, calls);
    EXPECT_EQ(1, len);

    EXPECT_STREQ("Start", calls->name);
    EXPECT_EQ(2, g_variant_n_children(calls->params));

    block = g_variant_get_child_value(calls->params, 1);
    EXPECT_TRUE(g_variant_get_boolean(block));
    g_variant_unref(block);

    env = g_variant_get_child_value(calls->params, 0);
    EXPECT_TRUE(check_env(env, "APP_ID", "multiple"));
    EXPECT_FALSE(check_env(env, "INSTANCE_ID", ""));
    g_variant_unref(env);
}

static void failed_observer(const gchar* appid, UbuntuAppLaunchAppFailed reason, gpointer user_data)
{
    if (reason == UBUNTU_APP_LAUNCH_APP_FAILED_CRASH)
    {
        std::string* last = static_cast<std::string*>(user_data);
        *last = appid;
    }
    return;
}

TEST_F(LibUAL, FailingObserver)
{
    std::string last_observer;
    GDBusConnection* session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);

    EXPECT_TRUE(ubuntu_app_launch_observer_add_app_failed(failed_observer, &last_observer));

    g_dbus_connection_emit_signal(
        session, NULL,                                                     /* destination */
        "/",                                                               /* path */
        "com.canonical.UbuntuAppLaunch",                                   /* interface */
        "ApplicationFailed",                                               /* signal */
        g_variant_new("(ss)", "com.test.good_application_1.2.3", "crash"), /* params, the same */
        NULL);

    EXPECT_EVENTUALLY_EQ("com.test.good_application_1.2.3", last_observer);

    last_observer.clear();

    g_dbus_connection_emit_signal(
        session, NULL,                                                        /* destination */
        "/",                                                                  /* path */
        "com.canonical.UbuntuAppLaunch",                                      /* interface */
        "ApplicationFailed",                                                  /* signal */
        g_variant_new("(ss)", "com.test.good_application_1.2.3", "blahblah"), /* params, the same */
        NULL);

    EXPECT_EVENTUALLY_EQ("com.test.good_application_1.2.3", last_observer);

    last_observer.clear();

    g_dbus_connection_emit_signal(
        session, NULL,                                                             /* destination */
        "/",                                                                       /* path */
        "com.canonical.UbuntuAppLaunch",                                           /* interface */
        "ApplicationFailed",                                                       /* signal */
        g_variant_new("(ss)", "com.test.good_application_1.2.3", "start-failure"), /* params, the same */
        NULL);

    EXPECT_EVENTUALLY_EQ(true, last_observer.empty());

    EXPECT_TRUE(ubuntu_app_launch_observer_delete_app_failed(failed_observer, &last_observer));

    g_object_unref(session);
}

TEST_F(LibUAL, StartHelper)
{
    DbusTestDbusMockObject* obj =
        dbus_test_dbus_mock_get_object(mock, "/com/test/untrusted/helper", "com.ubuntu.Upstart0_6.Job", NULL);

    auto untrusted = ubuntu::app_launch::Helper::Type::from_raw("untrusted-type");

    /* Basic make sure we can send the event */
    auto appid = ubuntu::app_launch::AppID::parse("com.test.multiple_first_1.2.3");
    auto helper = ubuntu::app_launch::Helper::create(untrusted, appid, registry);

    helper->launch();

    EXPECT_EQ(1, dbus_test_dbus_mock_object_check_method_call(mock, obj, "Start", NULL, NULL));

    ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(mock, obj, NULL));

    /* Now check a multi out */
    helper->launch();

    guint len = 0;
    auto calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Start", &len, NULL);
    EXPECT_NE(nullptr, calls);
    EXPECT_EQ(1, len);

    EXPECT_STREQ("Start", calls->name);
    EXPECT_EQ(2, g_variant_n_children(calls->params));

    auto block = g_variant_get_child_value(calls->params, 1);
    EXPECT_TRUE(g_variant_get_boolean(block));
    g_variant_unref(block);

    auto env = g_variant_get_child_value(calls->params, 0);
    EXPECT_TRUE(check_env(env, "APP_ID", "com.test.multiple_first_1.2.3"));
    EXPECT_TRUE(check_env(env, "HELPER_TYPE", "untrusted-type"));
    g_variant_unref(env);

    ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(mock, obj, NULL));

    /* Let's pass some URLs */
    std::vector<ubuntu::app_launch::Helper::URL> urls = {
        ubuntu::app_launch::Helper::URL::from_raw("http://ubuntu.com/"),
        ubuntu::app_launch::Helper::URL::from_raw("https://ubuntu.com/"),
        ubuntu::app_launch::Helper::URL::from_raw("file:///home/phablet/test.txt")};
    helper->launch(urls);

    len = 0;
    calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Start", &len, NULL);
    EXPECT_NE(nullptr, calls);
    EXPECT_EQ(1, len);

    env = g_variant_get_child_value(calls->params, 0);
    EXPECT_TRUE(check_env(env, "APP_ID", "com.test.multiple_first_1.2.3"));
    EXPECT_TRUE(
        check_env(env, "APP_URIS", "'http://ubuntu.com/' 'https://ubuntu.com/' 'file:///home/phablet/test.txt'"));
    EXPECT_TRUE(check_env(env, "HELPER_TYPE", "untrusted-type"));
    EXPECT_FALSE(check_env(env, "INSTANCE_ID", NULL));
    g_variant_unref(env);

    ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(mock, obj, NULL));

    /* Not sure why, but this makes this test better, hopefully we can
       clean this up when we move to the C++ API can use a cancellable */
    pause(100);

    return;
}

TEST_F(LibUAL, StopHelper)
{
    DbusTestDbusMockObject* obj =
        dbus_test_dbus_mock_get_object(mock, "/com/test/untrusted/helper", "com.ubuntu.Upstart0_6.Job", NULL);

    /* Multi helper */
    auto untrusted = ubuntu::app_launch::Helper::Type::from_raw("untrusted-type");

    auto appid = ubuntu::app_launch::AppID::parse("com.bar_foo_8432.13.1");
    auto helper = ubuntu::app_launch::Helper::create(untrusted, appid, registry);

    ASSERT_TRUE(helper->hasInstances());

    auto instances = helper->instances();

    EXPECT_EQ(1, instances.size());

    instances[0]->stop();

    ASSERT_EQ(dbus_test_dbus_mock_object_check_method_call(mock, obj, "Stop", NULL, NULL), 1);

    guint len = 0;
    auto calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Stop", &len, NULL);
    EXPECT_NE(nullptr, calls);
    EXPECT_EQ(1, len);

    EXPECT_STREQ("Stop", calls->name);
    EXPECT_EQ(2, g_variant_n_children(calls->params));

    auto block = g_variant_get_child_value(calls->params, 1);
    EXPECT_TRUE(g_variant_get_boolean(block));
    g_variant_unref(block);

    auto env = g_variant_get_child_value(calls->params, 0);
    EXPECT_TRUE(check_env(env, "APP_ID", "com.bar_foo_8432.13.1"));
    EXPECT_TRUE(check_env(env, "HELPER_TYPE", "untrusted-type"));
    EXPECT_TRUE(check_env(env, "INSTANCE_ID", "24034582324132"));
    g_variant_unref(env);

    ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(mock, obj, NULL));

    /* Not sure why, but this makes this test better, hopefully we can
       clean this up when we move to the C++ API can use a cancellable */
    pause(100);

    return;
}

TEST_F(LibUAL, HelperList)
{
    auto nothelper = ubuntu::app_launch::Helper::Type::from_raw("not-a-type");
    auto notlist = ubuntu::app_launch::Registry::runningHelpers(nothelper, registry);

    EXPECT_EQ(0, notlist.size());

    auto goodhelper = ubuntu::app_launch::Helper::Type::from_raw("untrusted-type");
    auto goodlist = ubuntu::app_launch::Registry::runningHelpers(goodhelper, registry);

    ASSERT_EQ(2, goodlist.size());

    goodlist.sort(
        [](const std::shared_ptr<ubuntu::app_launch::Helper>& a, const std::shared_ptr<ubuntu::app_launch::Helper>& b) {
            std::string sa = a->appId();
            std::string sb = b->appId();

            return sa < sb;
        });

    EXPECT_EQ("com.bar_foo_8432.13.1", (std::string)goodlist.front()->appId());
    EXPECT_EQ("com.foo_bar_43.23.12", (std::string)goodlist.back()->appId());

    EXPECT_TRUE(goodlist.front()->hasInstances());
    EXPECT_TRUE(goodlist.back()->hasInstances());

    EXPECT_EQ(1, goodlist.front()->instances().size());
    EXPECT_EQ(1, goodlist.back()->instances().size());

    EXPECT_TRUE(goodlist.front()->instances()[0]->isRunning());
    EXPECT_TRUE(goodlist.back()->instances()[0]->isRunning());
}

typedef struct
{
    unsigned int count;
    const gchar* appid;
    const gchar* type;
    const gchar* instance;
} helper_observer_data_t;

static void helper_observer_cb(const gchar* appid, const gchar* instance, const gchar* type, gpointer user_data)
{
    helper_observer_data_t* data = (helper_observer_data_t*)user_data;

    if (g_strcmp0(data->appid, appid) == 0 && g_strcmp0(data->type, type) == 0 &&
        g_strcmp0(data->instance, instance) == 0)
    {
        data->count++;
    }
}

TEST_F(LibUAL, StartStopHelperObserver)
{
    helper_observer_data_t start_data = {
        .count = 0, .appid = "com.foo_foo_1.2.3", .type = "my-type-is-scorpio", .instance = nullptr};
    helper_observer_data_t stop_data = {
        .count = 0, .appid = "com.bar_bar_44.32", .type = "my-type-is-libra", .instance = "1234"};

    ASSERT_TRUE(ubuntu_app_launch_observer_add_helper_started(helper_observer_cb, "my-type-is-scorpio", &start_data));
    ASSERT_TRUE(ubuntu_app_launch_observer_add_helper_stop(helper_observer_cb, "my-type-is-libra", &stop_data));

    DbusTestDbusMockObject* obj =
        dbus_test_dbus_mock_get_object(mock, "/com/ubuntu/Upstart", "com.ubuntu.Upstart0_6", NULL);

    /* Basic start */
    dbus_test_dbus_mock_object_emit_signal(
        mock, obj, "EventEmitted", G_VARIANT_TYPE("(sas)"),
        g_variant_new_parsed("('started', ['JOB=untrusted-helper', 'INSTANCE=my-type-is-scorpio::com.foo_foo_1.2.3'])"),
        NULL);

    EXPECT_EVENTUALLY_EQ(1, start_data.count);

    /* Basic stop */
    dbus_test_dbus_mock_object_emit_signal(
        mock, obj, "EventEmitted", G_VARIANT_TYPE("(sas)"),
        g_variant_new_parsed(
            "('stopped', ['JOB=untrusted-helper', 'INSTANCE=my-type-is-libra:1234:com.bar_bar_44.32'])"),
        NULL);

    EXPECT_EVENTUALLY_EQ(1, stop_data.count);

    /* Remove */
    ASSERT_TRUE(
        ubuntu_app_launch_observer_delete_helper_started(helper_observer_cb, "my-type-is-scorpio", &start_data));
    ASSERT_TRUE(ubuntu_app_launch_observer_delete_helper_stop(helper_observer_cb, "my-type-is-libra", &stop_data));
}

class SpewMaster
{
public:
    SpewMaster()
        : thread(
              [this]() {
                  gint spewstdout = 0;
                  std::array<const gchar*, 2> spewline{SPEW_UTILITY, nullptr};
                  ASSERT_TRUE(g_spawn_async_with_pipes(NULL,                    /* directory */
                                                       (char**)spewline.data(), /* command line */
                                                       NULL,                    /* environment */
                                                       G_SPAWN_DEFAULT,         /* flags */
                                                       NULL,                    /* child setup */
                                                       NULL,                    /* child setup */
                                                       &pid_,                   /* pid */
                                                       NULL,                    /* stdin */
                                                       &spewstdout,             /* stdout */
                                                       NULL,                    /* stderr */
                                                       NULL));                  /* error */

                  spewoutchan = g_io_channel_unix_new(spewstdout);
                  g_io_channel_set_flags(spewoutchan, G_IO_FLAG_NONBLOCK, NULL);

                  iosource = g_io_create_watch(spewoutchan, G_IO_IN);
                  g_source_set_callback(iosource, (GSourceFunc)datain, this, nullptr);
                  g_source_attach(iosource, g_main_context_get_thread_default());

                  /* Setup our OOM adjust file */
                  gchar* procdir = g_strdup_printf(CMAKE_BINARY_DIR "/libual-proc/%d", pid_);
                  ASSERT_EQ(0, g_mkdir_with_parents(procdir, 0700));
                  oomadjfile = g_strdup_printf("%s/oom_score_adj", procdir);
                  g_free(procdir);
                  ASSERT_TRUE(g_file_set_contents(oomadjfile, "0", -1, NULL));
              },
              [this]() {
                  /* Clean up */
                  gchar* killstr = g_strdup_printf("kill -9 %d", pid_);
                  ASSERT_TRUE(g_spawn_command_line_sync(killstr, NULL, NULL, NULL, NULL));
                  g_free(killstr);

                  g_source_destroy(iosource);
                  g_io_channel_unref(spewoutchan);
                  g_clear_pointer(&oomadjfile, g_free);
              })
    {
        datacnt_ = 0;
    }

    ~SpewMaster()
    {
    }

    std::string oomScore()
    {
        gchar* oomvalue = nullptr;
        g_file_get_contents(oomadjfile, &oomvalue, nullptr, nullptr);
        if (oomvalue != nullptr)
        {
            return std::string(oomvalue);
        }
        else
        {
            return {};
        }
    }

    GPid pid()
    {
        return pid_;
    }

    gsize dataCnt()
    {
        g_debug("Data Count for %d: %d", pid_, int(datacnt_));
        return datacnt_;
    }

    void reset()
    {
        bool endofqueue = thread.executeOnThread<bool>([this]() {
            while (G_IO_STATUS_AGAIN == g_io_channel_flush(spewoutchan, nullptr))
                ;
            return true; /* the main loop has processed */
        });
        g_debug("Reset %d", pid_);
        if (endofqueue)
            datacnt_ = 0;
        else
            g_warning("Unable to clear mainloop on reset");
    }

private:
    std::atomic<gsize> datacnt_;
    GPid pid_ = 0;
    gchar* oomadjfile = nullptr;
    GIOChannel* spewoutchan = nullptr;
    GSource* iosource = nullptr;
    GLib::ContextThread thread;

    static gboolean datain(GIOChannel* source, GIOCondition cond, gpointer data)
    {
        auto spew = static_cast<SpewMaster*>(data);
        gchar* str = NULL;
        gsize len = 0;
        GError* error = NULL;

        g_io_channel_read_line(source, &str, &len, NULL, &error);
        g_free(str);

        if (error != NULL)
        {
            g_warning("Unable to read from channel: %s", error->message);
            g_error_free(error);
        }

        spew->datacnt_ += len;

        return TRUE;
    }
};

static void signal_increment(GDBusConnection* connection,
                             const gchar* sender,
                             const gchar* path,
                             const gchar* interface,
                             const gchar* signal,
                             GVariant* params,
                             gpointer user_data)
{
    guint* count = (guint*)user_data;
    g_debug("Count incremented to: %d", *count + 1);
    *count = *count + 1;
}

// DISABLED: Skipping these tests to not block on bug #1584849
TEST_F(LibUAL, DISABLED_PauseResume)
{
    g_setenv("UBUNTU_APP_LAUNCH_OOM_PROC_PATH", CMAKE_BINARY_DIR "/libual-proc", 1);

    /* Setup some spew */
    SpewMaster spew;

    /* Setup the cgroup */
    g_setenv("UBUNTU_APP_LAUNCH_CG_MANAGER_NAME", "org.test.cgmock2", TRUE);
    DbusTestDbusMock* cgmock2 = dbus_test_dbus_mock_new("org.test.cgmock2");
    DbusTestDbusMockObject* cgobject = dbus_test_dbus_mock_get_object(cgmock2, "/org/linuxcontainers/cgmanager",
                                                                      "org.linuxcontainers.cgmanager0_0", NULL);
    gchar* pypids = g_strdup_printf("ret = [%d]", spew.pid());
    dbus_test_dbus_mock_object_add_method(cgmock, cgobject, "GetTasksRecursive", G_VARIANT_TYPE("(ss)"),
                                          G_VARIANT_TYPE("ai"), pypids, NULL);
    g_free(pypids);

    dbus_test_service_add_task(service, DBUS_TEST_TASK(cgmock2));
    dbus_test_task_run(DBUS_TEST_TASK(cgmock2));
    g_object_unref(G_OBJECT(cgmock2));

    /* Setup ZG Mock */
    DbusTestDbusMock* zgmock = dbus_test_dbus_mock_new("org.gnome.zeitgeist.Engine");
    DbusTestDbusMockObject* zgobj =
        dbus_test_dbus_mock_get_object(zgmock, "/org/gnome/zeitgeist/log/activity", "org.gnome.zeitgeist.Log", NULL);

    dbus_test_dbus_mock_object_add_method(zgmock, zgobj, "InsertEvents", G_VARIANT_TYPE("a(asaasay)"),
                                          G_VARIANT_TYPE("au"), "ret = [ 0 ]", NULL);

    dbus_test_service_add_task(service, DBUS_TEST_TASK(zgmock));
    dbus_test_task_run(DBUS_TEST_TASK(zgmock));
    g_object_unref(G_OBJECT(zgmock));

    /* Give things a chance to start */
    EXPECT_EVENTUALLY_EQ(DBUS_TEST_TASK_STATE_RUNNING, dbus_test_task_get_state(DBUS_TEST_TASK(cgmock2)));
    EXPECT_EVENTUALLY_EQ(DBUS_TEST_TASK_STATE_RUNNING, dbus_test_task_get_state(DBUS_TEST_TASK(zgmock)));

    /* Setup signal handling */
    guint paused_count = 0;
    guint resumed_count = 0;
    guint paused_signal =
        g_dbus_connection_signal_subscribe(bus, nullptr, "com.canonical.UbuntuAppLaunch", "ApplicationPaused", "/",
                                           nullptr, G_DBUS_SIGNAL_FLAGS_NONE, signal_increment, &paused_count, nullptr);
    guint resumed_signal = g_dbus_connection_signal_subscribe(
        bus, nullptr, "com.canonical.UbuntuAppLaunch", "ApplicationResumed", "/", nullptr, G_DBUS_SIGNAL_FLAGS_NONE,
        signal_increment, &resumed_count, nullptr);

    /* Get our app object */
    auto appid = ubuntu::app_launch::AppID::find("com.test.good_application_1.2.3");
    auto app = ubuntu::app_launch::Application::create(appid, registry);

    ASSERT_EQ(1, app->instances().size());

    auto instance = app->instances()[0];

    /* Test it */
    EXPECT_NE(0, spew.dataCnt());
    paused_count = 0;

    /* Pause the app */
    instance->pause();

    pause(0);     /* Flush queued events */
    spew.reset(); /* clear it */

    /* Check data coming out */
    EXPECT_EVENTUALLY_EQ(1, paused_count);
    EXPECT_EQ(0, spew.dataCnt());

    /* Check to make sure we sent the event to ZG */
    guint numcalls = 0;
    const DbusTestDbusMockCall* calls =
        dbus_test_dbus_mock_object_get_method_calls(zgmock, zgobj, "InsertEvents", &numcalls, NULL);

    EXPECT_NE(nullptr, calls);
    EXPECT_EQ(1, numcalls);

    dbus_test_dbus_mock_object_clear_method_calls(zgmock, zgobj, NULL);

    /* Check to ensure we set the OOM score */
    EXPECT_EQ("900", spew.oomScore());

    resumed_count = 0;

    /* Now Resume the App */
    instance->resume();

    EXPECT_EVENTUALLY_EQ(1, resumed_count);
    EXPECT_NE(0, spew.dataCnt());

    /* Check to make sure we sent the event to ZG */
    numcalls = 0;
    calls = dbus_test_dbus_mock_object_get_method_calls(zgmock, zgobj, "InsertEvents", &numcalls, NULL);

    EXPECT_NE(nullptr, calls);
    EXPECT_EQ(1, numcalls);

    /* Check to ensure we set the OOM score */
    EXPECT_EQ("100", spew.oomScore());

    g_spawn_command_line_sync("rm -rf " CMAKE_BINARY_DIR "/libual-proc", NULL, NULL, NULL, NULL);

    g_dbus_connection_signal_unsubscribe(bus, paused_signal);
    g_dbus_connection_signal_unsubscribe(bus, resumed_signal);
}

TEST_F(LibUAL, MultiPause)
{
    g_setenv("UBUNTU_APP_LAUNCH_OOM_PROC_PATH", CMAKE_BINARY_DIR "/libual-proc", 1);

    /* Setup some A TON OF spew */
    std::array<SpewMaster, 50> spews;

    /* Setup the cgroup */
    g_setenv("UBUNTU_APP_LAUNCH_CG_MANAGER_NAME", "org.test.cgmock2", TRUE);
    DbusTestDbusMock* cgmock2 = dbus_test_dbus_mock_new("org.test.cgmock2");
    DbusTestDbusMockObject* cgobject = dbus_test_dbus_mock_get_object(cgmock2, "/org/linuxcontainers/cgmanager",
                                                                      "org.linuxcontainers.cgmanager0_0", NULL);

    std::string pypids = "ret = [ " + std::accumulate(spews.begin(), spews.end(), std::string{},
                                                      [](const std::string& accum, SpewMaster& spew) {
                                                          return accum.empty() ?
                                                                     std::to_string(spew.pid()) :
                                                                     accum + ", " + std::to_string(spew.pid());
                                                      }) +
                         "]";
    dbus_test_dbus_mock_object_add_method(cgmock, cgobject, "GetTasksRecursive", G_VARIANT_TYPE("(ss)"),
                                          G_VARIANT_TYPE("ai"), pypids.c_str(), NULL);

    dbus_test_service_add_task(service, DBUS_TEST_TASK(cgmock2));
    dbus_test_task_run(DBUS_TEST_TASK(cgmock2));
    g_object_unref(G_OBJECT(cgmock2));

    /* Setup ZG Mock */
    DbusTestDbusMock* zgmock = dbus_test_dbus_mock_new("org.gnome.zeitgeist.Engine");
    DbusTestDbusMockObject* zgobj =
        dbus_test_dbus_mock_get_object(zgmock, "/org/gnome/zeitgeist/log/activity", "org.gnome.zeitgeist.Log", NULL);

    dbus_test_dbus_mock_object_add_method(zgmock, zgobj, "InsertEvents", G_VARIANT_TYPE("a(asaasay)"),
                                          G_VARIANT_TYPE("au"), "ret = [ 0 ]", NULL);

    dbus_test_service_add_task(service, DBUS_TEST_TASK(zgmock));
    dbus_test_task_run(DBUS_TEST_TASK(zgmock));
    g_object_unref(G_OBJECT(zgmock));

    /* Give things a chance to start */
    do
    {
        g_debug("Giving mocks a chance to start");
        pause(200);
    } while (dbus_test_task_get_state(DBUS_TEST_TASK(cgmock2)) != DBUS_TEST_TASK_STATE_RUNNING &&
             dbus_test_task_get_state(DBUS_TEST_TASK(zgmock)) != DBUS_TEST_TASK_STATE_RUNNING);

    /* Get our app object */
    auto appid = ubuntu::app_launch::AppID::find("com.test.good_application_1.2.3");
    auto app = ubuntu::app_launch::Application::create(appid, registry);

    ASSERT_EQ(1, app->instances().size());

    auto instance = app->instances()[0];

    /* Test it */
    EXPECT_NE(0, std::accumulate(spews.begin(), spews.end(), int{0},
                                 [](const int& acc, SpewMaster& spew) { return acc + spew.dataCnt(); }));

    /* Pause the app */
    instance->pause();

    std::for_each(spews.begin(), spews.end(), [](SpewMaster& spew) { spew.reset(); });
    pause(50);

    /* Check data coming out */
    EXPECT_EQ(0, std::accumulate(spews.begin(), spews.end(), int{0},
                                 [](const int& acc, SpewMaster& spew) { return acc + spew.dataCnt(); }));

    /* Now Resume the App */
    instance->resume();

    pause(50);

    EXPECT_NE(0, std::accumulate(spews.begin(), spews.end(), int{0},
                                 [](const int& acc, SpewMaster& spew) { return acc + spew.dataCnt(); }));

    /* Pause the app */
    instance->pause();

    std::for_each(spews.begin(), spews.end(), [](SpewMaster& spew) { spew.reset(); });
    pause(50);

    /* Check data coming out */
    EXPECT_EQ(0, std::accumulate(spews.begin(), spews.end(), int{0},
                                 [](const int& acc, SpewMaster& spew) { return acc + spew.dataCnt(); }));

    /* Now Resume the App */
    instance->resume();

    pause(50);

    EXPECT_NE(0, std::accumulate(spews.begin(), spews.end(), int{0},
                                 [](const int& acc, SpewMaster& spew) { return acc + spew.dataCnt(); }));

    g_spawn_command_line_sync("rm -rf " CMAKE_BINARY_DIR "/libual-proc", NULL, NULL, NULL, NULL);
}

TEST_F(LibUAL, OOMSet)
{
    g_setenv("UBUNTU_APP_LAUNCH_OOM_PROC_PATH", CMAKE_BINARY_DIR "/libual-proc", 1);

    GPid testpid = getpid();

    /* Setup our OOM adjust file */
    gchar* procdir = g_strdup_printf(CMAKE_BINARY_DIR "/libual-proc/%d", testpid);
    ASSERT_EQ(0, g_mkdir_with_parents(procdir, 0700));
    gchar* oomadjfile = g_strdup_printf("%s/oom_score_adj", procdir);
    g_free(procdir);
    ASSERT_TRUE(g_file_set_contents(oomadjfile, "0", -1, NULL));

    /* Setup the cgroup */
    g_setenv("UBUNTU_APP_LAUNCH_CG_MANAGER_NAME", "org.test.cgmock2", TRUE);
    DbusTestDbusMock* cgmock2 = dbus_test_dbus_mock_new("org.test.cgmock2");
    DbusTestDbusMockObject* cgobject = dbus_test_dbus_mock_get_object(cgmock2, "/org/linuxcontainers/cgmanager",
                                                                      "org.linuxcontainers.cgmanager0_0", NULL);
    gchar* pypids = g_strdup_printf("ret = [%d]", testpid);
    dbus_test_dbus_mock_object_add_method(cgmock, cgobject, "GetTasksRecursive", G_VARIANT_TYPE("(ss)"),
                                          G_VARIANT_TYPE("ai"), pypids, NULL);
    g_free(pypids);

    dbus_test_service_add_task(service, DBUS_TEST_TASK(cgmock2));
    dbus_test_task_run(DBUS_TEST_TASK(cgmock2));
    g_object_unref(G_OBJECT(cgmock2));

    /* Give things a chance to start */
    EXPECT_EVENTUALLY_EQ(DBUS_TEST_TASK_STATE_RUNNING, dbus_test_task_get_state(DBUS_TEST_TASK(cgmock2)));

    /* Get our app object */
    auto appid = ubuntu::app_launch::AppID::find("com.test.good_application_1.2.3");
    auto app = ubuntu::app_launch::Application::create(appid, registry);

    ASSERT_EQ(1, app->instances().size());

    auto instance = app->instances()[0];

    /* Set the OOM Score */
    instance->setOomAdjustment(ubuntu::app_launch::oom::paused());

    /* Check to ensure we set the OOM score */
    gchar* oomscore = NULL;
    ASSERT_TRUE(g_file_get_contents(oomadjfile, &oomscore, NULL, NULL));
    EXPECT_STREQ("900", oomscore);
    g_free(oomscore);

    /* Set the OOM Score */
    instance->setOomAdjustment(ubuntu::app_launch::oom::focused());

    /* Check to ensure we set the OOM score */
    ASSERT_TRUE(g_file_get_contents(oomadjfile, &oomscore, NULL, NULL));
    EXPECT_STREQ("100", oomscore);
    g_free(oomscore);

    /* Custom Score */
    auto custom = ubuntu::app_launch::oom::fromLabelAndValue(432, "Custom");
    instance->setOomAdjustment(custom);

    /* Check to ensure we set the OOM score */
    ASSERT_TRUE(g_file_get_contents(oomadjfile, &oomscore, NULL, NULL));
    EXPECT_STREQ("432", oomscore);
    g_free(oomscore);

    /* Check we can read it too! */
    EXPECT_EQ(custom, instance->getOomAdjustment());

    /* Remove write access from it */
    auto nowrite = std::string("chmod -w ") + oomadjfile;
    g_spawn_command_line_sync(nowrite.c_str(), nullptr, nullptr, nullptr, nullptr);
    instance->setOomAdjustment(ubuntu::app_launch::oom::focused());

    /* Cleanup */
    g_spawn_command_line_sync("rm -rf " CMAKE_BINARY_DIR "/libual-proc", NULL, NULL, NULL, NULL);

    /* Test no entry */
    instance->setOomAdjustment(ubuntu::app_launch::oom::focused());

    g_free(oomadjfile);
}

TEST_F(LibUAL, StartSessionHelper)
{
    DbusTestDbusMockObject* obj =
        dbus_test_dbus_mock_get_object(mock, "/com/test/untrusted/helper", "com.ubuntu.Upstart0_6.Job", NULL);
    MirConnection* conn = mir_connect_sync("libual-test", "start-session-helper");  // Mocked, doesn't need cleaning up
    MirPromptSession* msession = mir_connection_create_prompt_session_sync(conn, 5, nullptr, nullptr);

    /* Building a temporary file and making an FD for it */
    const char* filedata = "This is some data that we should get on the other side\n";
    ASSERT_TRUE(g_file_set_contents(SESSION_TEMP_FILE, filedata, strlen(filedata), nullptr) == TRUE);
    int mirfd = open(SESSION_TEMP_FILE, 0);
    mir_mock_set_trusted_fd(mirfd);

    /* Basic make sure we can send the event */
    auto untrusted = ubuntu::app_launch::Helper::Type::from_raw("untrusted-type");
    auto appid = ubuntu::app_launch::AppID::parse("com.test.multiple_first_1.2.3");
    auto helper = ubuntu::app_launch::Helper::create(untrusted, appid, registry);

    helper->launch(msession);

    guint len = 0;
    const DbusTestDbusMockCall* calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "Start", &len, NULL);
    EXPECT_NE(nullptr, calls);
    EXPECT_EQ(1, len);

    EXPECT_STREQ("Start", calls->name);
    EXPECT_EQ(2, g_variant_n_children(calls->params));

    GVariant* block = g_variant_get_child_value(calls->params, 1);
    EXPECT_TRUE(g_variant_get_boolean(block));
    g_variant_unref(block);

    /* Check the environment */
    GVariant* env = g_variant_get_child_value(calls->params, 0);
    EXPECT_TRUE(check_env(env, "APP_ID", "com.test.multiple_first_1.2.3"));
    EXPECT_TRUE(check_env(env, "HELPER_TYPE", "untrusted-type"));

    GVariant* mnamev = find_env(env, "UBUNTU_APP_LAUNCH_DEMANGLE_NAME");
    ASSERT_NE(nullptr, mnamev); /* Have to assert because, eh, GVariant */
    EXPECT_STREQ(g_dbus_connection_get_unique_name(bus),
                 g_variant_get_string(mnamev, nullptr) + strlen("UBUNTU_APP_LAUNCH_DEMANGLE_NAME="));
    GVariant* mpathv = find_env(env, "UBUNTU_APP_LAUNCH_DEMANGLE_PATH");
    ASSERT_NE(nullptr, mpathv); /* Have to assert because, eh, GVariant */

    g_variant_unref(env);

    /* Setup environment for call */
    const gchar* mname = g_variant_get_string(mnamev, nullptr);
    mname += strlen("UBUNTU_APP_LAUNCH_DEMANGLE_NAME=");
    g_setenv("UBUNTU_APP_LAUNCH_DEMANGLE_NAME", mname, TRUE);
    g_variant_unref(mnamev);

    const gchar* mpath = g_variant_get_string(mpathv, nullptr);
    mpath += strlen("UBUNTU_APP_LAUNCH_DEMANGLE_PATH=");
    g_setenv("UBUNTU_APP_LAUNCH_DEMANGLE_PATH", mpath, TRUE);
    g_variant_unref(mpathv);

    /* Exec our tool */
    std::promise<std::string> outputpromise;
    std::thread t([&outputpromise]() {
        gchar* socketstdout = nullptr;
        GError* error = nullptr;
        g_unsetenv("G_MESSAGES_DEBUG");

        g_spawn_command_line_sync(SOCKET_DEMANGLER " " SOCKET_TOOL, &socketstdout, nullptr, nullptr, &error);

        if (error != nullptr)
        {
            fprintf(stderr, "Unable to spawn '" SOCKET_DEMANGLER " " SOCKET_TOOL "': %s\n", error->message);
            g_error_free(error);
            outputpromise.set_value(std::string(""));
        }
        else
        {
            outputpromise.set_value(std::string(socketstdout));
            g_free(socketstdout);
        }
    });
    t.detach();

    auto outputfuture = outputpromise.get_future();
    while (outputfuture.wait_for(std::chrono::milliseconds{1}) != std::future_status::ready)
    {
        pause();
    }

    ASSERT_STREQ(filedata, outputfuture.get().c_str());

    ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(mock, obj, NULL));

    return;
}

TEST_F(LibUAL, SetExec)
{
    DbusTestDbusMockObject* obj =
        dbus_test_dbus_mock_get_object(mock, "/com/ubuntu/Upstart", "com.ubuntu.Upstart0_6", NULL);

    const char* exec = "lets exec this";

    g_setenv("UPSTART_JOB", "fubar", TRUE);
    g_unsetenv("UBUNTU_APP_LAUNCH_DEMANGLE_NAME");
    EXPECT_TRUE(ubuntu_app_launch_helper_set_exec(exec, NULL));

    guint len = 0;
    const DbusTestDbusMockCall* calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "SetEnv", &len, NULL);
    ASSERT_NE(nullptr, calls);
    EXPECT_EQ(1, len);

    gchar* appexecstr = g_strdup_printf("APP_EXEC=%s", exec);
    GVariant* appexecenv = g_variant_get_child_value(calls[0].params, 1);
    EXPECT_STREQ(appexecstr, g_variant_get_string(appexecenv, nullptr));
    g_variant_unref(appexecenv);
    g_free(appexecstr);

    ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(mock, obj, NULL));

    /* Now check for the demangler */
    g_setenv("UBUNTU_APP_LAUNCH_DEMANGLE_NAME", g_dbus_connection_get_unique_name(bus), TRUE);
    EXPECT_TRUE(ubuntu_app_launch_helper_set_exec(exec, NULL));

    calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "SetEnv", &len, NULL);
    ASSERT_NE(nullptr, calls);
    EXPECT_EQ(1, len);

    gchar* demangleexecstr = g_strdup_printf("APP_EXEC=%s %s", SOCKET_DEMANGLER_INSTALL, exec);
    appexecenv = g_variant_get_child_value(calls[0].params, 1);
    EXPECT_STREQ(demangleexecstr, g_variant_get_string(appexecenv, nullptr));
    g_variant_unref(appexecenv);
    g_free(demangleexecstr);

    ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(mock, obj, NULL));

    /* Now check for the directory */
    g_setenv("UBUNTU_APP_LAUNCH_DEMANGLE_NAME", g_dbus_connection_get_unique_name(bus), TRUE);
    EXPECT_TRUE(ubuntu_app_launch_helper_set_exec(exec, "/not/a/real/directory"));

    calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "SetEnv", &len, NULL);
    ASSERT_NE(nullptr, calls);
    EXPECT_EQ(2, len);

    appexecenv = g_variant_get_child_value(calls[1].params, 1);
    EXPECT_STREQ("APP_DIR=/not/a/real/directory", g_variant_get_string(appexecenv, nullptr));
    g_variant_unref(appexecenv);

    ASSERT_TRUE(dbus_test_dbus_mock_object_clear_method_calls(mock, obj, NULL));
}

TEST_F(LibUAL, AppInfo)
{
    g_setenv("TEST_CLICK_DB", "click-db-dir", TRUE);
    g_setenv("TEST_CLICK_USER", "test-user", TRUE);

    /* Correct values from a click */
    auto appid = ubuntu::app_launch::AppID::parse("com.test.good_application_1.2.4");
    auto app = ubuntu::app_launch::Application::create(appid, registry);

    EXPECT_TRUE((bool)app->info());
    EXPECT_EQ("Application", app->info()->name().value());

    /* Correct values from a legacy */
    auto barid = ubuntu::app_launch::AppID::find("bar");
    EXPECT_THROW(ubuntu::app_launch::Application::create(barid, registry), std::runtime_error);

    /* Correct values for libertine */
    auto libertineid = ubuntu::app_launch::AppID::parse("container-name_test_0.0");
    auto libertine = ubuntu::app_launch::Application::create(libertineid, registry);

    auto info = libertine->info();
    EXPECT_TRUE((bool)info);

    EXPECT_EQ("Test", info->name().value());
}
