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
#include <numeric>

#include <upstart.h>

#include "app-info.h"
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
    return !instances().empty();
}

bool UpstartInstance::isRunning()
{
    return primaryPid() != 0;
}

pid_t UpstartInstance::primaryPid()
{
    auto jobpath = registry_->impl->upstartJobPath(job_);
    if (jobpath.empty())
    {
        g_debug("Unable to get a valid job path");
        return 0;
    }

    return registry_->impl->thread.executeOnThread<pid_t>([this, &jobpath]() -> pid_t {
        GError* error = nullptr;
        g_debug("Getting instance by name: %s", instance_.c_str());
        GVariant* vinstance_path =
            g_dbus_connection_call_sync(registry_->impl->_dbus.get(),                   /* connection */
                                        DBUS_SERVICE_UPSTART,                           /* service */
                                        jobpath.c_str(),                                /* object path */
                                        DBUS_INTERFACE_UPSTART_JOB,                     /* iface */
                                        "GetInstanceByName",                            /* method */
                                        g_variant_new("(s)", instance_.c_str()),        /* params */
                                        G_VARIANT_TYPE("(o)"),                          /* return type */
                                        G_DBUS_CALL_FLAGS_NONE,                         /* flags */
                                        -1,                                             /* timeout: default */
                                        registry_->impl->thread.getCancellable().get(), /* cancelable */
                                        &error);

        if (error != nullptr)
        {
            g_warning("Unable to get instance '%s' of job '%s': %s", instance_.c_str(), job_.c_str(), error->message);
            g_error_free(error);
            return 0;
        }

        /* Jump rope to make this into a C++ type */
        std::string instance_path;
        gchar* cinstance_path = nullptr;
        g_variant_get(vinstance_path, "(o)", &cinstance_path);
        g_variant_unref(vinstance_path);
        if (cinstance_path != nullptr)
        {
            instance_path = cinstance_path;
            g_free(cinstance_path);
        }

        if (instance_path.empty())
        {
            g_debug("No instance object for instance name: %s", instance_.c_str());
            return 0;
        }

        GVariant* props_tuple =
            g_dbus_connection_call_sync(registry_->impl->_dbus.get(),                          /* connection */
                                        DBUS_SERVICE_UPSTART,                                  /* service */
                                        instance_path.c_str(),                                 /* object path */
                                        "org.freedesktop.DBus.Properties",                     /* interface */
                                        "GetAll",                                              /* method */
                                        g_variant_new("(s)", DBUS_INTERFACE_UPSTART_INSTANCE), /* params */
                                        G_VARIANT_TYPE("(a{sv})"),                             /* return type */
                                        G_DBUS_CALL_FLAGS_NONE,                                /* flags */
                                        -1,                                                    /* timeout: default */
                                        registry_->impl->thread.getCancellable().get(),        /* cancelable */
                                        &error);

        if (error != nullptr)
        {
            g_warning("Unable to name of properties '%s': %s", instance_path.c_str(), error->message);
            g_error_free(error);
            error = nullptr;
            return 0;
        }

        GVariant* props_dict = g_variant_get_child_value(props_tuple, 0);

        pid_t retval = 0;
        GVariant* processes = g_variant_lookup_value(props_dict, "processes", G_VARIANT_TYPE("a(si)"));
        if (processes != nullptr && g_variant_n_children(processes) > 0)
        {

            GVariant* first_entry = g_variant_get_child_value(processes, 0);
            GVariant* pidv = g_variant_get_child_value(first_entry, 1);

            retval = g_variant_get_int32(pidv);

            g_variant_unref(pidv);
            g_variant_unref(first_entry);
        }
        else
        {
            g_debug("Unable to get 'processes' from properties of instance at path: %s", instance_path.c_str());
        }

        g_variant_unref(props_dict);

        return retval;
    });
}

bool UpstartInstance::hasPid(pid_t pid)
{
    for (auto testpid : registry_->impl->pidsFromCgroup(job_, instance_))
        if (pid == testpid)
            return true;
    return false;
}

std::string UpstartInstance::logPath()
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

std::vector<pid_t> UpstartInstance::pids()
{
    auto pids = registry_->impl->pidsFromCgroup(job_, instance_);
    g_debug("Got %d PIDs for AppID '%s'", int(pids.size()), std::string(appId_).c_str());
    return pids;
}

void UpstartInstance::pause()
{
    registry_->impl->zgSendEvent(appId_, ZEITGEIST_ZG_LEAVE_EVENT);

    auto pids = forAllPids([this](pid_t pid) {
        auto oomval = oom::paused();
        g_debug("Pausing PID: %d (%d)", pid, int(oomval));
        signalToPid(pid, SIGSTOP);
        oomValueToPid(pid, oomval);
    });

    pidListToDbus(pids, "ApplicationPaused");
}

void UpstartInstance::resume()
{
    registry_->impl->zgSendEvent(appId_, ZEITGEIST_ZG_ACCESS_EVENT);

    auto pids = forAllPids([this](pid_t pid) {
        auto oomval = oom::focused();
        g_debug("Resuming PID: %d (%d)", pid, int(oomval));
        signalToPid(pid, SIGCONT);
        oomValueToPid(pid, oomval);
    });

    pidListToDbus(pids, "ApplicationResumed");
}

void UpstartInstance::stop()
{
    ubuntu_app_launch_stop_application(std::string(appId_).c_str());
}

/** Sets the OOM adjustment by getting the list of PIDs and writing
    the value to each of their files in proc */
void UpstartInstance::setOomAdjustment(const oom::Score score)
{
    forAllPids([this, &score](pid_t pid) { oomValueToPid(pid, score); });
}

/** Figures out the path to the primary PID of the application and
    then reads its OOM adjustment file. */
const oom::Score UpstartInstance::getOomAdjustment()
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

/** Go through the list of PIDs calling a function and handling
    the issue with getting PIDs being a racey condition. */
std::vector<pid_t> UpstartInstance::forAllPids(std::function<void(pid_t)> eachPid)
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
void UpstartInstance::signalToPid(pid_t pid, int signal)
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
std::string UpstartInstance::pidToOomPath(pid_t pid)
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
void UpstartInstance::oomValueToPid(pid_t pid, const oom::Score oomvalue)
{
    auto oomstr = std::to_string(static_cast<std::int32_t>(oomvalue));
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
                g_warning("Unable to set OOM value for '%d' to '%s': %s", int(pid), oomstr.c_str(),
                          std::strerror(openerr));
                return;
        }
    }

    size_t writesize = fwrite(oomstr.c_str(), 1, oomstr.size(), adj);
    int writeerr = errno;
    fclose(adj);

    if (writesize == oomstr.size())
        return;

    if (writeerr != 0)
        g_warning("Unable to set OOM value for '%d' to '%s': %s", int(pid), oomstr.c_str(), strerror(writeerr));
    else
        /* No error, but yet, wrong size. Not sure, what could cause this. */
        g_debug("Unable to set OOM value for '%d' to '%s': Wrote %d bytes", int(pid), oomstr.c_str(), int(writesize));
}

/** Use a setuid root helper for setting the oom value of
    Chromium instances */
void UpstartInstance::oomValueToPidHelper(pid_t pid, const oom::Score oomvalue)
{
    GError* error = nullptr;
    std::string oomstr = std::to_string(static_cast<std::int32_t>(oomvalue));
    std::string pidstr = std::to_string(pid);
    std::array<const char*, 4> args = {OOM_HELPER, pidstr.c_str(), oomstr.c_str(), nullptr};

    g_debug("Excuting OOM Helper (pid: %d, score: %d): %s", int(pid), int(oomvalue),
            std::accumulate(args.begin(), args.end(), std::string{},
                            [](const std::string& instr, const char* output) -> std::string {
                                if (instr.empty())
                                {
                                    return output;
                                }
                                else if (output != nullptr)
                                {
                                    return instr + " " + std::string(output);
                                }
                                else
                                {
                                    return instr;
                                }
                            })
                .c_str());

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
void UpstartInstance::pidListToDbus(const std::vector<pid_t>& pids, const std::string& signal)
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

UpstartInstance::UpstartInstance(const AppID& appId,
                                 const std::string& job,
                                 const std::string& instance,
                                 const std::shared_ptr<Registry>& registry)
    : appId_(appId)
    , job_(job)
    , instance_(instance)
    , registry_(registry)
{
}

std::shared_ptr<gchar*> urlsToStrv(const std::vector<Application::URL>& urls)
{
    if (urls.empty())
    {
        return {};
    }

    auto array = g_array_new(TRUE, FALSE, sizeof(gchar*));

    for (auto url : urls)
    {
        auto str = g_strdup(url.value().c_str());
        g_array_append_val(array, str);
    }

    return std::shared_ptr<gchar*>((gchar**)g_array_free(array, FALSE), g_strfreev);
}

std::shared_ptr<UpstartInstance> UpstartInstance::launch(const AppID& appId,
                                                         const std::string& job,
                                                         const std::string& instance,
                                                         const std::vector<Application::URL>& urls,
                                                         const std::shared_ptr<Registry>& registry,
                                                         launchMode mode)
{
    auto urlstrv = urlsToStrv(urls);
    auto start_result = registry->impl->thread.executeOnThread<gboolean>([registry, &appId, &urlstrv, &mode]() {
        return start_application_core(registry->impl->_dbus.get(), registry->impl->thread.getCancellable().get(),
                                      std::string(appId).c_str(), urlstrv.get(),
                                      mode == launchMode::STANDARD ? FALSE : TRUE);
    });

    if (start_result == FALSE)
    {
        return {};
    }

    return std::make_shared<UpstartInstance>(appId, job, instance, registry);
}

};  // namespace app_impls
};  // namespace app_launch
};  // namespace ubuntu
