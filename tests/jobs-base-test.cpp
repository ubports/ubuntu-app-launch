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
                  gchar* procdir = g_strdup_printf(CMAKE_BINARY_DIR "/jobs-base-proc/%d", pid_);
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
    g_setenv("UBUNTU_APP_LAUNCH_OOM_PROC_PATH", CMAKE_BINARY_DIR "/jobs-base-proc", 1);

    /* Setup some spew */
    SpewMaster spew;
    std::vector<pid_t> pids{spew.pid()};

    /* Build our instance */
    auto instance = simpleInstance();
    EXPECT_CALL(*instance, pids()).WillRepeatedly(testing::Return(pids));

    /* Setup registry */
    EXPECT_CALL(dynamic_cast<RegistryImplMock&>(*registry->impl), zgSendEvent(::testing::_, ::testing::_))
        .WillOnce(testing::Return());

    instance->pause();
}
