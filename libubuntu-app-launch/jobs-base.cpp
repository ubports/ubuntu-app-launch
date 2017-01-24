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
#include <numeric>

#include "jobs-base.h"
#include "jobs-upstart.h"
#include "registry-impl.h"

namespace ubuntu
{
namespace app_launch
{
namespace jobs
{
namespace manager
{

Base::Base(const std::shared_ptr<Registry>& registry)
    : registry_(registry)
{
}

std::shared_ptr<Base> Base::determineFactory(std::shared_ptr<Registry> registry)
{
    return std::make_shared<jobs::manager::Upstart>(registry);
}

}  // namespace manager

namespace instance
{

Base::Base(const AppID& appId,
           const std::string& job,
           const std::string& instance,
           const std::vector<Application::URL>& urls,
           const std::shared_ptr<Registry>& registry)
    : appId_(appId)
    , job_(job)
    , instance_(instance)
    , urls_(urls)
    , registry_(registry)
{
}

/** Checks to see if we have a primary PID for the instance */
bool Base::isRunning()
{
    return primaryPid() != 0;
}

/** Looks at the PIDs in the instance cgroup and checks to see if @pid
    is in the set.

    @param pid PID to look for
*/
bool Base::hasPid(pid_t pid)
{
    auto vpids = pids();
    return std::find(vpids.begin(), vpids.end(), pid) != vpids.end();
}

/** Pauses this application by sending SIGSTOP to all the PIDs in the
    cgroup and tells Zeitgeist that we've left the application. */
void Base::pause()
{
    g_debug("Pausing application: %s", std::string(appId_).c_str());
    registry_->impl->zgSendEvent(appId_, ZEITGEIST_ZG_LEAVE_EVENT);

    auto pids = forAllPids([this](pid_t pid) {
        auto oomval = oom::paused();
        g_debug("Pausing PID: %d (%d)", pid, int(oomval));
        signalToPid(pid, SIGSTOP);
        oomValueToPid(pid, oomval);
    });

    pidListToDbus(registry_, appId_, pids, "ApplicationPaused");
}

/** Resumes this application by sending SIGCONT to all the PIDs in the
    cgroup and tells Zeitgeist that we're accessing the application. */
void Base::resume()
{
    g_debug("Resuming application: %s", std::string(appId_).c_str());
    registry_->impl->zgSendEvent(appId_, ZEITGEIST_ZG_ACCESS_EVENT);

    auto pids = forAllPids([this](pid_t pid) {
        auto oomval = oom::focused();
        g_debug("Resuming PID: %d (%d)", pid, int(oomval));
        signalToPid(pid, SIGCONT);
        oomValueToPid(pid, oomval);
    });

    pidListToDbus(registry_, appId_, pids, "ApplicationResumed");
}

/** Go through the list of PIDs calling a function and handling
    the issue with getting PIDs being a racey condition.

    \param eachPid Function to run on each PID
*/
std::vector<pid_t> Base::forAllPids(std::function<void(pid_t)> eachPid)
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

/** Send a signal that we've change the application. Do this on the
    registry thread in an idle so that we don't block anyone.

    \param pids List of PIDs to turn into variants to send
    \param signal Name of the DBus signal to send
*/
void Base::pidListToDbus(const std::shared_ptr<Registry>& reg,
                         const AppID& appid,
                         const std::vector<pid_t>& pids,
                         const std::string& signal)
{
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
    g_variant_builder_add_value(&params, g_variant_new_string(std::string(appid).c_str()));
    g_variant_builder_add_value(&params, vpids.get());

    GError* error = nullptr;
    g_dbus_connection_emit_signal(reg->impl->_dbus.get(),          /* bus */
                                  nullptr,                         /* destination */
                                  "/",                             /* path */
                                  "com.canonical.UbuntuAppLaunch", /* interface */
                                  signal.c_str(),                  /* signal */
                                  g_variant_builder_end(&params),  /* params, the same */
                                  &error);                         /* error */

    if (error != nullptr)
    {
        g_warning("Unable to emit signal '%s' for appid '%s': %s", signal.c_str(), std::string(appid).c_str(),
                  error->message);
        g_error_free(error);
    }
    else
    {
        g_debug("Emmitted '%s' to DBus", signal.c_str());
    }
}

/** Sets the OOM adjustment by getting the list of PIDs and writing
    the value to each of their files in proc

    \param score OOM Score to set
*/
void Base::setOomAdjustment(const oom::Score score)
{
    forAllPids([this, &score](pid_t pid) { oomValueToPid(pid, score); });
}

/** Figures out the path to the primary PID of the application and
    then reads its OOM adjustment file. */
const oom::Score Base::getOomAdjustment()
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

/** Sends a signal to a PID with a warning if we can't send it.
    We could throw an exception, but we can't handle it usefully anyway

    \param pid PID to send the signal to
    \param signal signal to send
*/
void Base::signalToPid(pid_t pid, int signal)
{
    if (-1 == kill(pid, signal))
    {
        /* While that didn't work, we still want to try as many as we can */
        g_warning("Unable to send signal %d to pid %d", signal, pid);
    }
}

/** Get the path to the PID's OOM adjust path, with allowing for an
    override for testing using the environment variable
    UBUNTU_APP_LAUNCH_OOM_PROC_PATH

    \param pid PID to build path for
*/
std::string Base::pidToOomPath(pid_t pid)
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
    in the outer loop

    \param pid PID to change the OOM value of
    \param oomvalue OOM value to set
*/
void Base::oomValueToPid(pid_t pid, const oom::Score oomvalue)
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
    Chromium instances

    \param pid PID to change the OOM value of
    \param oomvalue OOM value to set
*/
void Base::oomValueToPidHelper(pid_t pid, const oom::Score oomvalue)
{
    GError* error = nullptr;
    std::string oomstr = std::to_string(static_cast<std::int32_t>(oomvalue));
    std::string pidstr = std::to_string(pid);
    std::array<const char*, 4> args = {OOM_HELPER, pidstr.c_str(), oomstr.c_str(), nullptr};

    g_debug("Excuting OOM Helper (pid: %d, score: %d): %s", int(pid), int(oomvalue),
            std::accumulate(args.begin(), args.end(), std::string{}, [](const std::string& instr,
                                                                        const char* output) -> std::string {
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
            }).c_str());

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

}  // namespace instance

}  // namespace jobs
}  // namespace app_launch
}  // namespace ubuntu
