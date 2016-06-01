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

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <map>

#include "application-impl-base.h"
#include "registry-impl.h"

namespace ubuntu
{
namespace app_launch
{
namespace app_impls
{

Base::Base(const std::shared_ptr<Registry>& registry)
    : _registry(registry)
{
}

bool Base::hasInstances()
{
    std::string sappid = appId();
    return ubuntu_app_launch_get_primary_pid(sappid.c_str()) != 0;
}

class BaseInstance : public Application::Instance
{
public:
    explicit BaseInstance(const AppID& appId,
                          const std::string& job,
                          const std::string& instance,
                          const std::shared_ptr<Registry>& registry);

    /* Query lifecycle */
    bool isRunning() override
    {
        return primaryPid() != 0;
    }
    pid_t primaryPid() override
    {
        return ubuntu_app_launch_get_primary_pid(std::string(appId_).c_str());
    }
    bool hasPid(pid_t pid) override
    {
        return ubuntu_app_launch_pid_in_app_id(pid, std::string(appId_).c_str()) == TRUE;
    }
    std::string logPath() override
    {
        auto cpath = ubuntu_app_launch_application_log_path(std::string(appId_).c_str());
        if (cpath != nullptr)
        {
            std::string retval(cpath);
            g_free(cpath);
            return retval;
        }
        else
        {
            return {};
        }
    }
    std::vector<pid_t> pids() override
    {
        return registry_->impl->pidsFromCgroup(job_, instance_);
    }

    /* Manage lifecycle */
    void pause() override
    {
        registry_->impl->zgSendEvent(appId_, ZEITGEIST_ZG_LEAVE_EVENT);

        auto oomstr = std::to_string(static_cast<std::int32_t>(oom::paused()));
        auto pids = forAllPids([this, &oomstr](pid_t pid) {
            g_debug("Pausing PID: %d", pid);
            signalToPid(pid, SIGSTOP);
            oomValueToPid(pid, oomstr);
        });

        pidListToDbus(pids, "ApplicationPaused");
    }
    void resume() override
    {
        registry_->impl->zgSendEvent(appId_, ZEITGEIST_ZG_ACCESS_EVENT);

        auto oomstr = std::to_string(static_cast<std::int32_t>(oom::focused()));
        auto pids = forAllPids([this, &oomstr](pid_t pid) {
            g_debug("Resuming PID: %d", pid);
            signalToPid(pid, SIGCONT);
            oomValueToPid(pid, oomstr);
        });

        pidListToDbus(pids, "ApplicationResumed");
    }
    void stop() override
    {
        ubuntu_app_launch_stop_application(std::string(appId_).c_str());
    }

    /* OOM Functions */
    /** Sets the OOM adjustment by getting the list of PIDs and writing
        the value to each of their files in proc */
    void setOomAdjustment(const oom::Score score) override
    {
        auto scorestr = std::to_string(static_cast<std::int32_t>(score));
        forAllPids([this, &scorestr](pid_t pid) { oomValueToPid(pid, scorestr); });
    }

    /** Figures out the path to the primary PID of the application and
        then reads its OOM adjustment file. */
    const oom::Score getOomAdjustment() override
    {
        auto pid = primaryPid();
        if (pid == 0)
        {
            throw std::runtime_error("No PID for application: " + std::string(appId_));
        }

        auto path = pidToOomPath(pid);
        GError* error = nullptr;
        gchar* content = nullptr;

        g_file_get_contents(path.c_str(), /* path */
                            &content,     /* data */
                            nullptr,      /* size */
                            &error);      /* error */

        if (error != nullptr)
        {
            auto serror = std::shared_ptr<GError>(error, g_error_free);
            throw std::runtime_error("Unable to access OOM value for '" + std::string(appId_) + "' primary PID '" +
                                     std::to_string(pid) + "' because: " + serror->message);
        }

        auto score = static_cast<oom::Score>(std::atoi(content));
        g_free(content);
        return score;
    }

private:
    const AppID appId_;
    const std::string job_;
    const std::string instance_;
    std::shared_ptr<Registry> registry_;

    /** Go through the list of PIDs calling a function and handling
        the issue with getting PIDs being a racey condition. */
    std::vector<pid_t> forAllPids(std::function<void(pid_t)> eachPid)
    {
        std::set<pid_t> seenPids;
        bool added = true;

        while (added)
        {
            added = false;
            auto pidlist = pids();
            for (auto pid : pidlist)
            {
                if (seenPids.insert(pid).second)
                {
                    eachPid(pid);
                    added = true;
                }
            }
        }

        return std::vector<pid_t>(seenPids.begin(), seenPids.end());
    }

    /** Sends a signal to a PID with a warning if we can't send it.
        We could throw an exception, but we can't handle it usefully anyway */
    void signalToPid(pid_t pid, int signal)
    {
        if (-1 == kill(pid, signal))
        {
            /* While that didn't work, we still want to try as many as we can */
            g_warning("Unable to send signal %d to pid %d", signal, pid);
        }
    }

    /** Get the path to the PID's OOM adjust path, with allowing for an
        override for testing using the environment variable
        UBUNTU_APP_LAUNCH_OOM_PROC_PATH */
    std::string pidToOomPath(pid_t pid)
    {
        static std::string procpath;
        if (G_UNLIKELY(procpath.empty()))
        {
            /* Set by the test suite, probably not anyone else */
            auto envvar = g_getenv("UBUNTU_APP_LAUNCH_OOM_PROC_PATH");
            if (G_LIKELY(envvar == nullptr))
                procpath = "/proc";
            else
                procpath = envvar;
        }

        gchar* gpath = g_build_filename(procpath.c_str(), std::to_string(pid).c_str(), "oom_score_adj", nullptr);
        std::string path = gpath;
        g_free(gpath);
        return path;
    }

    /** Writes an OOM value to proc, assuming we have a string
        in the outer loop */
    void oomValueToPid(pid_t pid, const std::string& oomvalue)
    {
        auto path = pidToOomPath(pid);
        FILE* adj = fopen(path.c_str(), "w");
        int openerr = errno;

        if (adj == nullptr)
        {
            switch (openerr)
            {
                case ENOENT:
                    /* ENOENT happens a fair amount because of races, so it's not
                       worth printing a warning about */
                    return;
                case EACCES:
                {
                    /* We can get this error when trying to set the OOM value on
                       Oxide renderers because they're started by the sandbox and
                       don't have their adjustment value available for us to write.
                       We have a helper to deal with this, but it's kinda expensive
                       so we only use it when we have to. */
                    oomValueToPidHelper(pid, oomvalue);
                    return;
                }
                default:
                    g_warning("Unable to set OOM value for '%d' to '%s': %s", int(pid), oomvalue.c_str(),
                              std::strerror(openerr));
                    return;
            }
        }

        size_t writesize = fwrite(oomvalue.c_str(), 1, oomvalue.size(), adj);
        int writeerr = errno;
        fclose(adj);

        if (writesize == oomvalue.size())
            return;

        if (writeerr != 0)
            g_warning("Unable to set OOM value for '%d' to '%s': %s", int(pid), oomvalue.c_str(), strerror(writeerr));
        else
            /* No error, but yet, wrong size. Not sure, what could cause this. */
            g_debug("Unable to set OOM value for '%d' to '%s': Wrote %d bytes", int(pid), oomvalue.c_str(),
                    int(writesize));
    }

    /** Use a setuid root helper for setting the oom value of
        Chromium instances */
    void oomValueToPidHelper(pid_t pid, const std::string& oomvalue)
    {
        GError* error = nullptr;
        std::string pidstr = std::to_string(pid);
        std::array<const char*, 4> args = {OOM_HELPER, pidstr.c_str(), oomvalue.c_str(), nullptr};

        g_spawn_async(nullptr,               /* working dir */
                      (char**)(args.data()), /* args */
                      nullptr,               /* env */
                      G_SPAWN_DEFAULT,       /* flags */
                      nullptr,               /* child setup */
                      nullptr,               /* child setup userdata*/
                      nullptr,               /* pid */
                      &error);               /* error */

        if (error != nullptr)
        {
            g_warning("Unable to launch OOM helper '" OOM_HELPER "' on PID '%d': %s", pid, error->message);
            g_error_free(error);
            return;
        }
    }

    /** Send a signal that we've change the application. Do this on the
        registry thread in an idle so that we don't block anyone. */
    void pidListToDbus(const std::vector<pid_t>& pids, const std::string& signal)
    {
        auto registry = registry_;
        auto lappid = appId_;

        registry_->impl->thread.executeOnThread([registry, lappid, pids, signal] {
            auto vpids = std::shared_ptr<GVariant>(
                [pids]() {
                    GVariant* pidarray = nullptr;

                    if (pids.empty())
                    {
                        pidarray = g_variant_new_array(G_VARIANT_TYPE_UINT64, nullptr, 0);
                        g_variant_ref_sink(pidarray);
                        return pidarray;
                    }

                    GVariantBuilder builder;
                    g_variant_builder_init(&builder, G_VARIANT_TYPE_ARRAY);
                    for (auto pid : pids)
                    {
                        g_variant_builder_add_value(&builder, g_variant_new_uint64(pid));
                    }

                    pidarray = g_variant_builder_end(&builder);
                    g_variant_ref_sink(pidarray);
                    return pidarray;
                }(),
                [](GVariant* var) { g_variant_unref(var); });

            GVariantBuilder params;
            g_variant_builder_init(&params, G_VARIANT_TYPE_TUPLE);
            g_variant_builder_add_value(&params, g_variant_new_string(std::string(lappid).c_str()));
            g_variant_builder_add_value(&params, vpids.get());

            GError* error = nullptr;
            g_dbus_connection_emit_signal(registry->impl->_dbus.get(),     /* bus */
                                          nullptr,                         /* destination */
                                          "/",                             /* path */
                                          "com.canonical.UbuntuAppLaunch", /* interface */
                                          signal.c_str(),                  /* signal */
                                          g_variant_builder_end(&params),  /* params, the same */
                                          &error);                         /* error */

            if (error != nullptr)
            {
                g_warning("Unable to emit signal '%s' for appid '%s': %s", signal.c_str(), std::string(lappid).c_str(),
                          error->message);
                g_error_free(error);
            }
            else
            {
                g_debug("Emmitted '%s' to DBus", signal.c_str());
            }
        });
    }
};

BaseInstance::BaseInstance(const AppID& appId,
                           const std::string& job,
                           const std::string& instance,
                           const std::shared_ptr<Registry>& registry)
    : appId_(appId)
    , job_(job)
    , instance_(instance)
    , registry_(registry)
{
}

std::vector<std::shared_ptr<Application::Instance>> Base::instances()
{
    std::string job;
    std::string instance;
    std::tie(job, instance) = jobAndInstance();

    std::vector<std::shared_ptr<Instance>> vect;
    vect.emplace_back(std::make_shared<BaseInstance>(appId(), job, instance, _registry));
    return vect;
}

std::shared_ptr<gchar*> urlsToStrv(std::vector<Application::URL> urls)
{
    auto array = g_array_new(TRUE, FALSE, sizeof(gchar*));

    for (auto url : urls)
    {
        auto str = g_strdup(url.value().c_str());
        g_array_append_val(array, str);
    }

    return std::shared_ptr<gchar*>((gchar**)g_array_free(array, FALSE), g_strfreev);
}

std::shared_ptr<Application::Instance> Base::launch(const std::vector<Application::URL>& urls)
{
    std::string appIdStr = appId();
    std::shared_ptr<gchar*> urlstrv;

    if (urls.size() > 0)
    {
        urlstrv = urlsToStrv(urls);
    }

    std::string job;
    std::string instance;
    std::tie(job, instance) = jobAndInstance();

    ubuntu_app_launch_start_application(appIdStr.c_str(), urlstrv.get());

    return std::make_shared<BaseInstance>(appId(), job, instance, _registry);
}

std::shared_ptr<Application::Instance> Base::launchTest(const std::vector<Application::URL>& urls)
{
    std::string appIdStr = appId();
    std::shared_ptr<gchar*> urlstrv;

    if (urls.size() > 0)
    {
        urlstrv = urlsToStrv(urls);
    }

    std::string job;
    std::string instance;
    std::tie(job, instance) = jobAndInstance();

    ubuntu_app_launch_start_application_test(appIdStr.c_str(), urlstrv.get());

    return std::make_shared<BaseInstance>(appId(), job, instance, _registry);
}

};  // namespace app_impls
};  // namespace app_launch
};  // namespace ubuntu
