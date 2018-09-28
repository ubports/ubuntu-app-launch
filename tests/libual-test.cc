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

#include <fcntl.h>
#include <future>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <gtest/gtest.h>
#include <libdbustest/dbus-test.h>
#include <thread>
#include <zeitgeist.h>

#include "application.h"
#include "helper.h"
#include "registry.h"
#include "ubuntu-app-launch.h"

#include "eventually-fixture.h"
#include "libertine-service.h"
#include "mir-mock.h"
#include "snapd-mock.h"
#include "systemd-mock.h"

#define LOCAL_SNAPD_TEST_SOCKET (SNAPD_TEST_SOCKET "-libual-test")
#define CGROUP_DIR (CMAKE_BINARY_DIR "/systemd-libual-cgroups")

class LibUAL : public EventuallyFixture
{
protected:
    DbusTestService *service = NULL;
    DbusTestDbusMock *mock = NULL;
    DbusTestDbusMock *cgmock = NULL;
    std::shared_ptr<LibertineService> libertine;
    std::shared_ptr<SystemdMock> systemd;
    GDBusConnection *bus = NULL;
    std::string last_focus_appid;
    std::string last_resume_appid;
    guint resume_timeout = 0;

private:
    static void focus_cb(const gchar *appid, gpointer user_data)
    {
        g_debug("Focus Callback: %s", appid);
        LibUAL *_this = static_cast<LibUAL *>(user_data);
        _this->last_focus_appid = appid;
    }

    static void resume_cb(const gchar *appid, gpointer user_data)
    {
        g_debug("Resume Callback: %s", appid);
        LibUAL *_this = static_cast<LibUAL *>(user_data);
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
            return;

        DbusTestBustle *bustle = dbus_test_bustle_new("test.bustle");
        dbus_test_service_add_task(service, DBUS_TEST_TASK(bustle));
        g_object_unref(bustle);

        DbusTestProcess *monitor = dbus_test_process_new("dbus-monitor");
        dbus_test_service_add_task(service, DBUS_TEST_TASK(monitor));
        g_object_unref(monitor);
    }

    virtual void SetUp()
    {
        g_setenv("XDG_DATA_DIRS", CMAKE_SOURCE_DIR, TRUE);
        g_setenv("XDG_CACHE_HOME", CMAKE_SOURCE_DIR "/libertine-data", TRUE);
        g_setenv("XDG_DATA_HOME", CMAKE_SOURCE_DIR "/libertine-home", TRUE);

        g_setenv("UBUNTU_APP_LAUNCH_SNAPD_SOCKET", LOCAL_SNAPD_TEST_SOCKET, TRUE);
        g_setenv("UBUNTU_APP_LAUNCH_SNAP_BASEDIR", SNAP_BASEDIR, TRUE);
        g_setenv("UBUNTU_APP_LAUNCH_DISABLE_SNAPD_TIMEOUT", "You betcha!", TRUE);

        g_setenv("UBUNTU_APP_LAUNCH_SYSTEMD_PATH", "/this/should/not/exist", TRUE);
        g_setenv("UBUNTU_APP_LAUNCH_SYSTEMD_CGROUP_ROOT", CGROUP_DIR, TRUE);

        g_unlink(LOCAL_SNAPD_TEST_SOCKET);

        service = dbus_test_service_new(NULL);

        debugConnection();

        systemd = std::make_shared<SystemdMock>(
            std::list<SystemdMock::Instance>{
                {"application-snap", "unity8-package_foo_x123", {}, getpid(), {100, 200, 300}},
                {"application-legacy", "multiple", "2342345", 5678, {100, 200, 300}},
                {"application-legacy", "single", {}, getpid(), {getpid()}},
                {"untrusted-helper", "com.foo_bar_43.23.12", {}, 1, {100, 200, 300}},
                {"untrusted-helper", "com.bar_foo_8432.13.1", "24034582324132", 1, {100, 200, 300}}},
            CGROUP_DIR);

        /* Put it together */
        dbus_test_service_add_task(service, *systemd);

        /* Add in Libertine */
        libertine = std::make_shared<LibertineService>();
        dbus_test_service_add_task(service, *libertine);

        dbus_test_service_start_tasks(service);

        bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
        g_dbus_connection_set_exit_on_close(bus, FALSE);
        g_object_add_weak_pointer(G_OBJECT(bus), (gpointer *)&bus);

        ASSERT_EVENTUALLY_FUNC_EQ(false, std::function<bool()>{[&] { return libertine->getUniqueName().empty(); }});

        ASSERT_TRUE(ubuntu_app_launch_observer_add_app_focus(focus_cb, this));
        ASSERT_TRUE(ubuntu_app_launch_observer_add_app_resume(resume_cb, this));
    }

    virtual void TearDown()
    {
        ubuntu_app_launch_observer_delete_app_focus(focus_cb, this);
        ubuntu_app_launch_observer_delete_app_resume(resume_cb, this);

        ubuntu::app_launch::Registry::clearDefault();

        systemd.reset();
        libertine.reset();
        g_clear_object(&service);

        g_object_unref(bus);

        ASSERT_EVENTUALLY_EQ(nullptr, bus);

        g_unlink(LOCAL_SNAPD_TEST_SOCKET);
    }

    static std::string find_env(std::set<std::string> &envs, std::string var)
    {
        auto iter =
            std::find_if(envs.begin(), envs.end(), [var](std::string value) { return split_env(value).first == var; });

        if (iter == envs.end())
        {
            return {};
        }
        else
        {
            return *iter;
        }
    }

    static std::pair<std::string, std::string> split_env(const std::string &env)
    {
        auto eq = std::find(env.begin(), env.end(), '=');
        if (eq == env.end())
        {
            throw std::runtime_error{"Environment value is invalid: " + env};
        }

        return std::make_pair(std::string(env.begin(), eq), std::string(eq + 1, env.end()));
    }

    static bool check_env(std::set<std::string> &envs, const std::string &key, const std::string &value)
    {
        auto val = find_env(envs, key);
        if (val.empty())
        {
            return false;
        }
        return split_env(val).second == value;
    }
};

#define TASK_STATE(task)                                                   \
    std::function<DbusTestTaskState()>                                     \
    {                                                                      \
        [&task] { return dbus_test_task_get_state(DBUS_TEST_TASK(task)); } \
    }

/* Snapd mock data */
static std::pair<std::string, std::string> interfaces{
    "GET /v2/interfaces HTTP/1.1\r\nHost: snapd\r\nAccept: */*\r\n\r\n",
    SnapdMock::httpJsonResponse(SnapdMock::snapdOkay(SnapdMock::interfacesJson(
        {{"unity8", "unity8-package", {"foo", "single"}}, {"mir", "unity8-package", {"foo"}}})))};
static std::pair<std::string, std::string> u8Package{
    "GET /v2/snaps/unity8-package HTTP/1.1\r\nHost: snapd\r\nAccept: */*\r\n\r\n",
    SnapdMock::httpJsonResponse(SnapdMock::snapdOkay(SnapdMock::packageJson(
        "unity8-package", "active", "app", "1.2.3.4", "x123", {"foo", "single"})))};

TEST_F(LibUAL, StartApplication)
{
    /* Basic make sure we can send the event */
    ASSERT_TRUE(ubuntu_app_launch_start_application("single", NULL));

    std::list<SystemdMock::TransientUnit> calls;
    ASSERT_EVENTUALLY_FUNC_LT(0u, std::function<unsigned int(void)>([&]() {
                                  calls = systemd->unitCalls();
                                  return calls.size();
                              }));
    EXPECT_EQ(SystemdMock::instanceName({"application-legacy", "single", {}, 0, {}}), calls.begin()->name);

    systemd->managerClear();

    /* Let's pass some URLs */
    const gchar *urls[] = {"http://ubuntu.com/", "https://ubuntu.com/", "file:///home/phablet/test.txt", NULL};
    ASSERT_TRUE(ubuntu_app_launch_start_application("foo", urls));

    ASSERT_EVENTUALLY_FUNC_LT(0u, std::function<unsigned int(void)>([&]() {
                                  calls = systemd->unitCalls();
                                  return calls.size();
                              }));

    EXPECT_EQ("file:///home/phablet/test.txt", *(calls.begin()->execline.rbegin()));
    EXPECT_EQ("https://ubuntu.com/", *(++calls.begin()->execline.rbegin()));
    EXPECT_EQ("http://ubuntu.com/", *(++(++calls.begin()->execline.rbegin())));

    return;
}

TEST_F(LibUAL, StartApplicationTest)
{
    ASSERT_TRUE(ubuntu_app_launch_start_application_test("foo", nullptr));

    std::list<SystemdMock::TransientUnit> calls;
    ASSERT_EVENTUALLY_FUNC_LT(0u, std::function<unsigned int(void)>([&]() {
                                  calls = systemd->unitCalls();
                                  return calls.size();
                              }));

    EXPECT_TRUE(check_env(calls.begin()->environment, "QT_LOAD_TESTABILITY", "1"));
}

TEST_F(LibUAL, StopApplication)
{
    ASSERT_TRUE(ubuntu_app_launch_stop_application("single"));

    std::list<std::string> calls;
    ASSERT_EVENTUALLY_FUNC_LT(0u, std::function<unsigned int(void)>([&]() {
                                  calls = systemd->stopCalls();
                                  return calls.size();
                              }));

    EXPECT_EQ(SystemdMock::instanceName({"application-legacy", "single", {}, 0, {}}), *calls.begin());
}

TEST_F(LibUAL, ApplicationPid)
{
    /* Check bad params */
    EXPECT_EQ(0, ubuntu_app_launch_get_primary_pid(nullptr));
    EXPECT_FALSE(ubuntu_app_launch_pid_in_app_id(0, "multiple"));
    EXPECT_FALSE(ubuntu_app_launch_pid_in_app_id(100, nullptr));

    /* Check primary pid, which comes from Systemd */
    EXPECT_EQ(getpid(), ubuntu_app_launch_get_primary_pid("single"));
    EXPECT_EQ(5678, ubuntu_app_launch_get_primary_pid("multiple"));

    /* Click in the set */
    EXPECT_TRUE(ubuntu_app_launch_pid_in_app_id(100, "multiple"));

    /* Click out of the set */
    EXPECT_FALSE(ubuntu_app_launch_pid_in_app_id(101, "multiple"));

    /* Legacy Single Instance */
    EXPECT_TRUE(ubuntu_app_launch_pid_in_app_id(getpid(), "single"));

    /* Legacy Multi Instance */
    EXPECT_TRUE(ubuntu_app_launch_pid_in_app_id(100, "multiple"));

    /* Check non running app */
    EXPECT_EQ(0, ubuntu_app_launch_get_primary_pid("chatter.robert-ancell_chatter_2"));
}

TEST_F(LibUAL, ApplicationId)
{
    SnapdMock snapd{LOCAL_SNAPD_TEST_SOCKET,
                    {u8Package, u8Package, u8Package, u8Package, u8Package, u8Package, u8Package, u8Package, u8Package,
                     u8Package, u8Package, u8Package, u8Package, u8Package}};
    ubuntu::app_launch::Registry::clearDefault();

    /* Test with current-user-version, should return the version in the manifest */
    EXPECT_STREQ("unity8-package_single_x123",
                 ubuntu_app_launch_triplet_to_app_id("unity8-package", "single", "current-user-version"));

    /* Test with version specified, shouldn't even read the manifest */
    EXPECT_STREQ("unity8-package_single_x123", ubuntu_app_launch_triplet_to_app_id("unity8-package", "single", "x123"));

    /* Test with out a version or app, should return the version in the manifest */
    EXPECT_STREQ("unity8-package_foo_x123",
                 ubuntu_app_launch_triplet_to_app_id("unity8-package", "first-listed-app", "current-user-version"));

    /* Test with a version or but wildcard app, should return the version in the manifest */
    EXPECT_STREQ("unity8-package_xmir_x123",
                 ubuntu_app_launch_triplet_to_app_id("unity8-package", "last-listed-app", "x123"));

    EXPECT_EQ(nullptr, ubuntu_app_launch_triplet_to_app_id("unity8-package", "only-listed-app", NULL));

    /* A bunch that should be NULL */
    EXPECT_EQ(nullptr, ubuntu_app_launch_triplet_to_app_id("com.test.no-hooks", NULL, NULL));
    EXPECT_EQ(nullptr, ubuntu_app_launch_triplet_to_app_id("com.test.no-json", NULL, NULL));
    EXPECT_EQ(nullptr, ubuntu_app_launch_triplet_to_app_id("com.test.no-object", NULL, NULL));
    EXPECT_EQ(nullptr, ubuntu_app_launch_triplet_to_app_id("com.test.no-version", NULL, NULL));

    /* Libertine tests */
    EXPECT_EQ(nullptr, ubuntu_app_launch_triplet_to_app_id("container-name", NULL, NULL));
    EXPECT_EQ(nullptr, ubuntu_app_launch_triplet_to_app_id("container-name", "not-exist", NULL));
    EXPECT_STREQ("container-name_test_0.0", ubuntu_app_launch_triplet_to_app_id("container-name", "test", NULL));
    EXPECT_STREQ("container-name_user-app_0.0",
                 ubuntu_app_launch_triplet_to_app_id("container-name", "user-app", NULL));

    snapd.result();
}

TEST_F(LibUAL, AppIdParse)
{
    EXPECT_TRUE(ubuntu_app_launch_app_id_parse("com.ubuntu.test_test_123", NULL, NULL, NULL));
    EXPECT_FALSE(ubuntu_app_launch_app_id_parse("inkscape", NULL, NULL, NULL));
    EXPECT_FALSE(ubuntu_app_launch_app_id_parse("music-app", NULL, NULL, NULL));

    gchar *pkg;
    gchar *app;
    gchar *version;

    ASSERT_TRUE(ubuntu_app_launch_app_id_parse("com.ubuntu.test_test_123", &pkg, &app, &version));
    EXPECT_STREQ("com.ubuntu.test", pkg);
    EXPECT_STREQ("test", app);
    EXPECT_STREQ("123", version);

    g_free(pkg);
    g_free(app);
    g_free(version);

    return;
}

TEST_F(LibUAL, ApplicationList)
{
    SnapdMock snapd{LOCAL_SNAPD_TEST_SOCKET, {u8Package, interfaces, u8Package}};
    ubuntu::app_launch::Registry::clearDefault();

    gchar **apps = ubuntu_app_launch_list_running_apps();

    ASSERT_NE(apps, nullptr);
    ASSERT_EQ(3u, g_strv_length(apps));

    /* Not enforcing order, but wanting to use the GTest functions
       for "actually testing" so the errors look right. */
    if (g_strcmp0(apps[0], "multiple") == 0)
    {
        ASSERT_STREQ("multiple", apps[0]);
        ASSERT_STREQ("unity8-package_foo_x123", apps[2]);
    }
    else
    {
        ASSERT_STREQ("unity8-package_foo_x123", apps[0]);
        ASSERT_STREQ("multiple", apps[2]);
    }

    g_strfreev(apps);
}

typedef struct
{
    int count;
    const gchar *name;
} observer_data_t;

static void observer_cb(const gchar *appid, gpointer user_data)
{
    observer_data_t *data = (observer_data_t *)user_data;
    g_debug("Observer called for: %s", appid);

    if (data->name == nullptr)
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

    /* Basic start */
    systemd->managerEmitNew(SystemdMock::instanceName({"application-legacy", "foo", {}, 0, {}}), "/foo");

    EXPECT_EVENTUALLY_EQ(1, start_data.count);

    /* Basic stop */
    systemd->managerEmitRemoved(SystemdMock::instanceName({"application-legacy", "foo", {}, 0, {}}), "/foo");

    EXPECT_EVENTUALLY_EQ(1, stop_data.count);

    /* Test Noise Start */
    start_data.count = 0;
    start_data.name = "multiple";
    stop_data.count = 0;
    stop_data.name = "multiple";

    systemd->managerEmitNew(SystemdMock::instanceName({"application-legacy", "foobar", {}, 0, {}}), "/foo");
    systemd->managerEmitRemoved(SystemdMock::instanceName({"application-legacy", "foobar", {}, 0, {}}), "/foo");
    systemd->managerEmitNew(SystemdMock::instanceName({"application-legacy", "elephant", {}, 0, {}}), "/foo");
    systemd->managerEmitNew(SystemdMock::instanceName({"application-legacy", "single", {}, 0, {}}), "/foo");
    systemd->managerEmitRemoved(SystemdMock::instanceName({"application-legacy", "giraffe", {}, 0, {}}), "/foo");
    systemd->managerEmitNew(SystemdMock::instanceName({"application-legacy", "multiple", {}, 0, {}}), "/foo");
    systemd->managerEmitRemoved(SystemdMock::instanceName({"application-legacy", "single", {}, 0, {}}), "/foo");
    systemd->managerEmitRemoved(SystemdMock::instanceName({"application-legacy", "circus", {}, 0, {}}), "/foo");
    systemd->managerEmitRemoved(SystemdMock::instanceName({"application-legacy", "multiple", {}, 0, {}}), "/foo");

    /* Ensure we just signaled once for each */
    EXPECT_EVENTUALLY_EQ(1, start_data.count);
    EXPECT_EVENTUALLY_EQ(1, stop_data.count);

    /* Remove */
    ASSERT_TRUE(ubuntu_app_launch_observer_delete_app_started(observer_cb, &start_data));
    ASSERT_TRUE(ubuntu_app_launch_observer_delete_app_stop(observer_cb, &stop_data));
}

static GDBusMessage *filter_starting(GDBusConnection *conn,
                                     GDBusMessage *message,
                                     gboolean incomming,
                                     gpointer user_data)
{
    if (g_strcmp0(g_dbus_message_get_member(message), "UnityStartingSignal") == 0)
    {
        auto count = static_cast<int *>(user_data);
        (*count)++;
        g_object_unref(message);
        return NULL;
    }

    return message;
}

static void starting_observer(const gchar *appid, gpointer user_data)
{
    std::string *last = static_cast<std::string *>(user_data);
    *last = appid;
    return;
}

TEST_F(LibUAL, StartingResponses)
{
    std::string last_observer;
    int starting_count = 0;
    GDBusConnection *session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
    guint filter = g_dbus_connection_add_filter(session, filter_starting, &starting_count, NULL);

    EXPECT_TRUE(ubuntu_app_launch_observer_add_app_starting(starting_observer, &last_observer));

    /* Ensure some bad data doesn't bother us */
    g_dbus_connection_emit_signal(
        session, NULL,                                                          /* destination */
        "/",                                                                    /* path */
        "com.canonical.UbuntuAppLaunch",                                        /* interface */
        "UnityStartingBroadcast",                                               /* signal */
        g_variant_new("(ss)", "com.test.bad_application_1.2.3", "badinstance"), /* params, the same */
        NULL);

    g_dbus_connection_emit_signal(session, NULL,                                     /* destination */
                                  "/",                                               /* path */
                                  "com.canonical.UbuntuAppLaunch",                   /* interface */
                                  "UnityStartingBroadcast",                          /* signal */
                                  g_variant_new("(ss)", "multiple", "goodinstance"), /* params, the same */
                                  NULL);

    EXPECT_EVENTUALLY_EQ("multiple", last_observer);
    EXPECT_EVENTUALLY_EQ(1, starting_count);

    EXPECT_TRUE(ubuntu_app_launch_observer_delete_app_starting(starting_observer, &last_observer));

    g_dbus_connection_remove_filter(session, filter);
    g_object_unref(session);
}

TEST_F(LibUAL, AppIdTest)
{
    ASSERT_TRUE(ubuntu_app_launch_start_application("single", NULL));
    EXPECT_EVENTUALLY_EQ("single", this->last_focus_appid);
    EXPECT_EQ("single", this->last_resume_appid);
}

GDBusMessage *filter_func_good(GDBusConnection *conn, GDBusMessage *message, gboolean incomming, gpointer user_data)
{
    if (!incomming)
    {
        return message;
    }

    if (g_strcmp0(g_dbus_message_get_path(message), (gchar *)user_data) == 0)
    {
        GDBusMessage *reply = g_dbus_message_new_method_reply(message);
        g_dbus_connection_send_message(conn, reply, G_DBUS_SEND_MESSAGE_FLAGS_NONE, NULL, NULL);
        g_object_unref(message);
        return NULL;
    }

    return message;
}

TEST_F(LibUAL, UrlSendTest)
{
    GDBusConnection *session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
    guint filter = g_dbus_connection_add_filter(session, filter_func_good, (gpointer) "/single", NULL);

    const gchar *uris[] = {"http://www.test.com", NULL};
    ASSERT_TRUE(ubuntu_app_launch_start_application("single", uris));

    EXPECT_EVENTUALLY_EQ("single", this->last_resume_appid);
    EXPECT_EVENTUALLY_EQ("single", this->last_focus_appid);

    g_dbus_connection_remove_filter(session, filter);
    g_object_unref(session);
}

TEST_F(LibUAL, UrlSendNoObjectTest)
{
    const gchar *uris[] = {"http://www.test.com", NULL};

    ASSERT_TRUE(ubuntu_app_launch_start_application("single", uris));

    EXPECT_EVENTUALLY_EQ("single", this->last_focus_appid);
    EXPECT_EVENTUALLY_EQ("single", this->last_resume_appid);
}

TEST_F(LibUAL, UnityTimeoutTest)
{
    this->resume_timeout = 100;

    ASSERT_TRUE(ubuntu_app_launch_start_application("single", NULL));

    EXPECT_EVENTUALLY_EQ("single", this->last_resume_appid);
    EXPECT_EVENTUALLY_EQ("single", this->last_focus_appid);
}

TEST_F(LibUAL, UnityTimeoutUriTest)
{
    this->resume_timeout = 200;

    const gchar *uris[] = {"http://www.test.com", NULL};

    ASSERT_TRUE(ubuntu_app_launch_start_application("single", uris));

    EXPECT_EVENTUALLY_EQ("single", this->last_focus_appid);
    EXPECT_EVENTUALLY_EQ("single", this->last_resume_appid);
}

GDBusMessage *filter_respawn(GDBusConnection *conn, GDBusMessage *message, gboolean incomming, gpointer user_data)
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
    GDBusConnection *session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
    guint filter = g_dbus_connection_add_filter(session, filter_respawn, NULL, NULL);

    guint start = g_get_monotonic_time();

    const gchar *uris[] = {"http://www.test.com", NULL};

    ASSERT_TRUE(ubuntu_app_launch_start_application("single", uris));

    guint end = g_get_monotonic_time();

    g_debug("Start call time: %d ms", (end - start) / 1000);
    EXPECT_LT(end - start, 2000u * 1000u);

    EXPECT_EVENTUALLY_EQ("single", this->last_focus_appid);
    EXPECT_EVENTUALLY_EQ("single", this->last_resume_appid);

    g_dbus_connection_remove_filter(session, filter);
    g_object_unref(session);
}

static void failed_observer(const gchar *appid, UbuntuAppLaunchAppFailed reason, gpointer user_data)
{
    g_debug("Failed observer called for: '%s' reason %d", appid, reason);

    if (reason == UBUNTU_APP_LAUNCH_APP_FAILED_CRASH)
    {
        std::string *last = static_cast<std::string *>(user_data);
        *last = appid;
    }
    else
    {
        std::string *last = static_cast<std::string *>(user_data);
        last->clear();
    }
    return;
}

TEST_F(LibUAL, FailingObserver)
{
    std::string last_observer;

    EXPECT_TRUE(ubuntu_app_launch_observer_add_app_failed(failed_observer, &last_observer));

    systemd->managerEmitFailed(SystemdMock::Instance{"application-legacy", "multiple", "2342345", 0, {}}, "core");

    EXPECT_EVENTUALLY_EQ("multiple", last_observer);

    last_observer.clear();

    systemd->managerEmitFailed(SystemdMock::Instance{"application-legacy", "multiple", "2342345", 0, {}}, "blahblah");

    EXPECT_EVENTUALLY_EQ("multiple", last_observer);

    last_observer.clear();

    last_observer = "something random";
    systemd->managerEmitFailed(SystemdMock::Instance{"application-legacy", "multiple", "2342345", 0, {}}, "exit-code");

    EXPECT_EVENTUALLY_FUNC_EQ(true, std::function<bool()>([&] { return last_observer.empty(); }));

    EXPECT_TRUE(ubuntu_app_launch_observer_delete_app_failed(failed_observer, &last_observer));

    /* For some reason dbus-mock sends two property change signals,
     * so this is 6 instead of 3 like you'd think it would be. */
    EXPECT_EVENTUALLY_FUNC_EQ(6u, std::function<unsigned int()>([&] { return systemd->resetCalls().size(); }));
}

TEST_F(LibUAL, StartHelper)
{
    /* Basic make sure we can send the event */
    gchar *cinstanceid =
        ubuntu_app_launch_start_multiple_helper("untrusted-type", "com.test.multiple_first_1.2.3", NULL);
    ASSERT_NE(nullptr, cinstanceid);
    std::string instanceid{cinstanceid};

    std::list<SystemdMock::TransientUnit> calls;
    ASSERT_EVENTUALLY_FUNC_LT(0u, std::function<unsigned int(void)>([&]() {
                                  calls = systemd->unitCalls();
                                  return calls.size();
                              }));

    /* Now look at the details of the call */
    EXPECT_EQ(SystemdMock::instanceName({"untrusted-type", "com.test.multiple_first_1.2.3", instanceid, 0, {}}),
              calls.begin()->name);

    systemd->managerClear();

    /* Let's pass some URLs */
    const gchar *urls[] = {"http://ubuntu.com/", "https://ubuntu.com/", "file:///home/phablet/test.txt", NULL};
    cinstanceid = ubuntu_app_launch_start_multiple_helper("untrusted-type", "com.test.multiple_first_1.2.3", urls);
    ASSERT_NE(nullptr, cinstanceid);
    instanceid = cinstanceid;

    ASSERT_EVENTUALLY_FUNC_LT(0u, std::function<unsigned int(void)>([&]() {
                                  calls = systemd->unitCalls();
                                  return calls.size();
                              }));

    EXPECT_EQ(SystemdMock::instanceName({"untrusted-type", "com.test.multiple_first_1.2.3", instanceid, 0, {}}),
              calls.begin()->name);

    EXPECT_EQ("file:///home/phablet/test.txt", *(calls.begin()->execline.rbegin()));
    EXPECT_EQ("https://ubuntu.com/", *(++calls.begin()->execline.rbegin()));
    EXPECT_EQ("http://ubuntu.com/", *(++(++calls.begin()->execline.rbegin())));

    return;
}

TEST_F(LibUAL, StopHelper)
{
    /* Basic helper */
    ASSERT_TRUE(ubuntu_app_launch_stop_helper("untrusted-helper", "com.foo_bar_43.23.12"));

    std::list<std::string> calls;
    ASSERT_EVENTUALLY_FUNC_LT(0u, std::function<unsigned int(void)>([&]() {
                                  calls = systemd->stopCalls();
                                  return calls.size();
                              }));

    EXPECT_EQ(SystemdMock::instanceName({"untrusted-helper", "com.foo_bar_43.23.12", {}, 0, {}}), *calls.begin());

    systemd->managerClear();

    /* Multi helper */
    ASSERT_TRUE(ubuntu_app_launch_stop_multiple_helper("untrusted-helper", "com.bar_foo_8432.13.1", "24034582324132"));

    ASSERT_EVENTUALLY_FUNC_LT(0u, std::function<unsigned int(void)>([&]() {
                                  calls = systemd->stopCalls();
                                  return calls.size();
                              }));

    EXPECT_EQ(SystemdMock::instanceName({"untrusted-helper", "com.bar_foo_8432.13.1", "24034582324132", 0, {}}),
              *calls.begin());

    return;
}

TEST_F(LibUAL, HelperList)
{
    gchar **blanktype = ubuntu_app_launch_list_helpers("not-a-type");

    EXPECT_NE(nullptr, blanktype);
    EXPECT_EQ(0u, g_strv_length(blanktype));

    g_strfreev(blanktype);

    gchar **goodtype = ubuntu_app_launch_list_helpers("untrusted-helper");

    ASSERT_NE(nullptr, goodtype);
    ASSERT_EQ(2u, g_strv_length(goodtype));

    if (g_strcmp0(goodtype[0], "com.foo_bar_43.23.12") == 0)
    {
        EXPECT_STREQ("com.foo_bar_43.23.12", goodtype[0]);
        EXPECT_STREQ("com.bar_foo_8432.13.1", goodtype[1]);
    }
    else
    {
        EXPECT_STREQ("com.foo_bar_43.23.12", goodtype[1]);
        EXPECT_STREQ("com.bar_foo_8432.13.1", goodtype[0]);
    }

    g_strfreev(goodtype);
}

TEST_F(LibUAL, HelperInstanceList)
{
    gchar **blanktype = ubuntu_app_launch_list_helper_instances("not-a-type", "com.bar_foo_8432.13.1");

    EXPECT_NE(nullptr, blanktype);
    EXPECT_EQ(0u, g_strv_length(blanktype));

    g_strfreev(blanktype);

    gchar **goodtype = ubuntu_app_launch_list_helper_instances("untrusted-helper", "com.bar_foo_8432.13.1");

    ASSERT_NE(nullptr, goodtype);
    ASSERT_EQ(1u, g_strv_length(goodtype));
    EXPECT_STREQ("24034582324132", goodtype[0]);

    g_strfreev(goodtype);
}

typedef struct
{
    int count;
    const gchar *appid;
    const gchar *type;
    const gchar *instance;
} helper_observer_data_t;

static void helper_observer_cb(const gchar *appid, const gchar *instance, const gchar *type, gpointer user_data)
{
    helper_observer_data_t *data = (helper_observer_data_t *)user_data;

    if (g_strcmp0(data->appid, appid) == 0 && g_strcmp0(data->type, type) == 0 &&
        g_strcmp0(data->instance, instance) == 0)
    {
        data->count++;
    }
}

TEST_F(LibUAL, StartStopHelperObserver)
{
    helper_observer_data_t start_data = {
        .count = 0, .appid = "com.foo_foo_1.2.3", .type = "my-type-is-scorpio", .instance = ""};
    helper_observer_data_t stop_data = {
        .count = 0, .appid = "com.bar_foo_8432.13.1", .type = "untrusted-helper", .instance = "24034582324132"};

    ASSERT_TRUE(ubuntu_app_launch_observer_add_helper_started(helper_observer_cb, "my-type-is-scorpio", &start_data));
    ASSERT_TRUE(ubuntu_app_launch_observer_add_helper_stop(helper_observer_cb, "untrusted-helper", &stop_data));

    /* Basic start */
    systemd->managerEmitNew(SystemdMock::instanceName({"my-type-is-scorpio", "com.foo_foo_1.2.3", {}, 0, {}}), "/foo");

    EXPECT_EVENTUALLY_EQ(1, start_data.count);

    /* Basic stop */
    systemd->managerEmitRemoved(
        SystemdMock::instanceName({"untrusted-helper", "com.bar_foo_8432.13.1", "24034582324132", 0, {}}), "/foo");

    EXPECT_EVENTUALLY_EQ(1, stop_data.count);

    /* Remove */
    ASSERT_TRUE(
        ubuntu_app_launch_observer_delete_helper_started(helper_observer_cb, "my-type-is-scorpio", &start_data));
    ASSERT_TRUE(ubuntu_app_launch_observer_delete_helper_stop(helper_observer_cb, "untrusted-helper", &stop_data));
}

gboolean datain(GIOChannel *source, GIOCondition cond, gpointer data)
{
    gsize *datacnt = static_cast<gsize *>(data);
    gchar *str = NULL;
    gsize len = 0;
    GError *error = NULL;

    g_io_channel_read_line(source, &str, &len, NULL, &error);
    g_free(str);

    if (error != NULL)
    {
        g_warning("Unable to read from channel: %s", error->message);
        g_error_free(error);
    }

    *datacnt += len;

    return TRUE;
}

TEST_F(LibUAL, StartSessionHelper)
{
    MirConnection *conn = mir_connect_sync("libual-test", "start-session-helper");  // Mocked, doesn't need cleaning up
    MirPromptSession *msession = mir_connection_create_prompt_session_sync(conn, 5, nullptr, nullptr);

    /* Building a temporary file and making an FD for it */
    const char *filedata = "This is some data that we should get on the other side\n";
    ASSERT_TRUE(g_file_set_contents(SESSION_TEMP_FILE, filedata, strlen(filedata), nullptr) == TRUE);
    int mirfd = open(SESSION_TEMP_FILE, 0);
    mir_mock_set_trusted_fd(mirfd);

    /* Basic make sure we can send the event */
    gchar *instance_id =
        ubuntu_app_launch_start_session_helper("untrusted-type", msession, "com.test.multiple_first_1.2.3", nullptr);
    ASSERT_NE(nullptr, instance_id);

    std::list<SystemdMock::TransientUnit> calls;
    ASSERT_EVENTUALLY_FUNC_LT(0u, std::function<unsigned int(void)>([&]() {
                                  calls = systemd->unitCalls();
                                  return calls.size();
                              }));
    EXPECT_EQ(SystemdMock::instanceName({"untrusted-type", "com.test.multiple_first_1.2.3", instance_id, 0, {}}),
              calls.begin()->name);

    /* Check the environment */
    auto nameenv = find_env(calls.begin()->environment, "UBUNTU_APP_LAUNCH_DEMANGLE_NAME");
    ASSERT_FALSE(nameenv.empty());
    auto pathenv = find_env(calls.begin()->environment, "UBUNTU_APP_LAUNCH_DEMANGLE_PATH");
    ASSERT_FALSE(pathenv.empty());

    /* Setup environment for call */
    g_setenv("UBUNTU_APP_LAUNCH_DEMANGLE_NAME", split_env(nameenv).second.c_str(), TRUE);
    g_setenv("UBUNTU_APP_LAUNCH_DEMANGLE_PATH", split_env(pathenv).second.c_str(), TRUE);

    /* Exec our tool */
    std::promise<std::string> outputpromise;
    std::thread t([&outputpromise]() {
        gchar *socketstdout = nullptr;
        GError *error = nullptr;
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

    EXPECT_EVENTUALLY_FUTURE_EQ(std::string{filedata}, outputpromise.get_future());

    return;
}

/* Hardcore socket stuff */
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

TEST_F(LibUAL, SetExec)
{
    /* Create a socket */
    class SmartSocket
    {
    public:
        int fd;
        SmartSocket()
            : fd(socket(AF_UNIX, SOCK_STREAM, 0))
        {
        }
        ~SmartSocket()
        {
            close(fd);
        }
    };
    SmartSocket sock;
    ASSERT_NE(0, sock.fd);

    std::string socketname{"/ual-setexec-test-12445343"};

    struct sockaddr_un socketaddr = {0};
    socketaddr.sun_family = AF_UNIX;
    strncpy(socketaddr.sun_path, socketname.c_str(), sizeof(socketaddr.sun_path) - 1);
    socketaddr.sun_path[0] = 0;

    ASSERT_EQ(0, bind(sock.fd, (const struct sockaddr *)&socketaddr, sizeof(struct sockaddr_un)));
    listen(sock.fd, 1); /* 1 is the number of people who can connect */

    setenv("UBUNTU_APP_LAUNCH_HELPER_EXECTOOL_SETEXEC_SOCKET", socketname.c_str(), 1);

    std::promise<std::vector<std::string>> socketpromise;
    std::thread socketreader([&]() {
        std::vector<std::string> socketvals;

        int readsocket = accept(sock.fd, NULL, NULL);

        /* Keeping this similar to the helper-helper code as that's what
         * we're running against. Not making it C++-style. */
        char readbuf[2048] = {0};
        int thisread = 0;
        int amountread = 0;
        while ((thisread = read(readsocket, readbuf + amountread, 2048 - amountread)) > 0)
        {
            amountread += thisread;

            if (amountread == 2048)
            {
                try
                {
                    throw std::runtime_error{"Read too many bytes from socket"};
                }
                catch (...)
                {
                    socketpromise.set_exception(std::current_exception());
                }
                return;
            }
        }

        close(readsocket);

        /* Parse data */
        if (amountread > 0)
        {
            char *startvar = readbuf;

            do
            {
                socketvals.emplace_back(std::string(startvar));

                startvar = startvar + strlen(startvar) + 1;
            } while (startvar < readbuf + amountread);
        }

        /* Read socket */
        socketpromise.set_value(socketvals);
    });
    socketreader.detach(); /* avoid thread cleanup code when we don't really care */

    std::vector<std::string> execList{"/usr/bin/foo", "Bar", "Really really really long value", "Another value"};
    ubuntu_app_launch_helper_set_exec(std::accumulate(execList.begin(), execList.end(), std::string{},
                                                      [](std::string accum, std::string val) {
                                                          std::string newval = val;
                                                          if (std::find(val.begin(), val.end(), ' ') != val.end())
                                                          {
                                                              newval = "\"" + val + "\"";
                                                          }

                                                          return accum.empty() ? newval : accum + " " + newval;
                                                      })
                                          .c_str(),
                                      nullptr);

    EXPECT_EVENTUALLY_FUTURE_EQ(execList, socketpromise.get_future());
}

TEST_F(LibUAL, AppInfo)
{
    char *dir = nullptr;
    char *file = nullptr;

    /* Basics */
    EXPECT_FALSE(ubuntu_app_launch_application_info("com.test.bad_not-app_1.3.3.7", nullptr, nullptr));
    EXPECT_FALSE(ubuntu_app_launch_application_info(nullptr, nullptr, nullptr));

    g_clear_pointer(&dir, g_free);
    g_clear_pointer(&file, g_free);
}
