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

extern "C" {
#include <zeitgeist.h>
}

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
    explicit BaseInstance(const AppID& appId, const std::shared_ptr<Registry>& registry);

    /* Query lifecycle */
    bool isRunning() override
    {
        return ubuntu_app_launch_get_primary_pid(std::string(appId_).c_str()) != 0;
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
        std::vector<pid_t> vector;
        GList* list = ubuntu_app_launch_get_pids(std::string(appId_).c_str());

        for (GList* pntr = list; pntr != nullptr; pntr = g_list_next(pntr))
        {
            vector.push_back(static_cast<pid_t>(GPOINTER_TO_INT(pntr->data)));
        }

        g_list_free(list);

        return vector;
    }

    /* Manage lifecycle */
    void pause() override
    {
        zgSendEvent(ZEITGEIST_ZG_LEAVE_EVENT);

        auto oomstr = std::to_string(static_cast<std::int32_t>(oom::paused()));
        auto pids = forAllPids([this, oomstr](pid_t pid) {
            g_debug("Pausing PID: %d", pid);
            signalToPid(pid, SIGSTOP);
            oomValueToPid(pid, oomstr);
        });

        pidListToDbus(pids, "ApplicationPaused");
    }
    void resume() override
    {
        zgSendEvent(ZEITGEIST_ZG_ACCESS_EVENT);

        auto oomstr = std::to_string(static_cast<std::int32_t>(oom::focused()));
        auto pids = forAllPids([this, oomstr](pid_t pid) {
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
        forAllPids([this, scorestr](pid_t pid) { oomValueToPid(pid, scorestr); });
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
                                     std::to_string(pid) + "' becuase: " + serror->message);
        }

        auto score = static_cast<oom::Score>(std::atoi(content));
        g_free(content);
        return score;
    }

private:
    AppID appId_;
    std::shared_ptr<Registry> registry_;

    /** Go through the list of PIDs calling a function and handling
        the issue with getting PIDs being a racey condition. */
    std::vector<pid_t> forAllPids(std::function<void(pid_t)> eachPid)
    {
        std::map<pid_t, bool> seenPids;
        bool found = true;

        while (found)
        {
            found = false;
            auto pidlist = pids();
            for (auto pid : pidlist)
            {
                try
                {
                    seenPids.at(pid);
                }
                catch (std::out_of_range& e)
                {
                    eachPid(pid);
                    seenPids[pid] = true;
                    found = true;
                }
            }
        }

        std::vector<pid_t> pidsout;
        std::for_each(seenPids.begin(), seenPids.end(), /* entries to grab */
                      [&pidsout](std::pair<const pid_t, bool>& entry) {
                          return pidsout.push_back(entry.first);
                      }); /* transform function */
        return pidsout;
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
            procpath = g_getenv("UBUNTU_APP_LAUNCH_OOM_PROC_PATH");
            if (G_LIKELY(procpath.empty()))
            {
                procpath = "/proc";
            }
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
                    g_warning("Unable to set OOM value for '%d' to '%s': %s", pid, oomvalue.c_str(),
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
            g_warning("Unable to set OOM value for '%d' to '%s': %s", pid, oomvalue.c_str(), strerror(writeerr));
        else
            /* No error, but yet, wrong size. Not sure, what could cause this. */
            g_debug("Unable to set OOM value for '%d' to '%s': Wrote %d bytes", pid, oomvalue.c_str(), (int)writesize);

        return;
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

        return;
    }

    /** Send a signal that we've change the application. Do this on the
        registry thread in an idle so that we don't block anyone. */
    void pidListToDbus(std::vector<pid_t>& pids, const std::string& signal)
    {
        auto registry = registry_;
        auto lappid = appId_;

        registry_->impl->thread.executeOnThread([registry, lappid, pids, signal] {
            auto vpids = std::shared_ptr<GVariant>(
                [pids]() {
                    GVariant* pidarray = nullptr;

                    if (pids.size() == 0)
                    {
                        pidarray = g_variant_new_array(G_VARIANT_TYPE_UINT64, NULL, 0);
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

    /** Send an event to Zietgeist using the registry thread so that
        the callback comes back in the right place. */
    void zgSendEvent(const std::string& eventtype)
    {
        auto lappid = appId_;
        registry_->impl->thread.executeOnThread([lappid, eventtype] {
            std::string uri;

            if (lappid.package.value().empty())
            {
                uri = "application://" + lappid.appname.value() + ".desktop";
            }
            else
            {
                uri = "application://" + lappid.package.value() + "_" + lappid.appname.value() + ".desktop";
            }

            g_debug("Sending ZG event for '%s': %s", uri.c_str(), eventtype.c_str());

            ZeitgeistLog* log = zeitgeist_log_get_default();

            ZeitgeistEvent* event = zeitgeist_event_new();
            zeitgeist_event_set_actor(event, "application://ubuntu-app-launch.desktop");
            zeitgeist_event_set_interpretation(event, eventtype.c_str());
            zeitgeist_event_set_manifestation(event, ZEITGEIST_ZG_USER_ACTIVITY);

            ZeitgeistSubject* subject = zeitgeist_subject_new();
            zeitgeist_subject_set_interpretation(subject, ZEITGEIST_NFO_SOFTWARE);
            zeitgeist_subject_set_manifestation(subject, ZEITGEIST_NFO_SOFTWARE_ITEM);
            zeitgeist_subject_set_mimetype(subject, "application/x-desktop");
            zeitgeist_subject_set_uri(subject, uri.c_str());

            zeitgeist_event_add_subject(event, subject);

            zeitgeist_log_insert_event(log,     /* log */
                                       event,   /* event */
                                       nullptr, /* cancellable */
                                       [](GObject* obj, GAsyncResult* res, gpointer user_data) -> void {
                                           GError* error = nullptr;
                                           GArray* result = nullptr;

                                           result = zeitgeist_log_insert_event_finish(ZEITGEIST_LOG(obj), res, &error);

                                           if (error != nullptr)
                                           {
                                               g_warning("Unable to submit Zeitgeist Event: %s", error->message);
                                               g_error_free(error);
                                           }

                                           g_array_free(result, TRUE);
                                       },        /* callback */
                                       nullptr); /* userdata */

            g_object_unref(log);
            g_object_unref(event);
            g_object_unref(subject);

        });
    }
};

BaseInstance::BaseInstance(const AppID& appId, const std::shared_ptr<Registry>& registry)
    : appId_(appId)
    , registry_(registry)
{
}

std::vector<std::shared_ptr<Application::Instance>> Base::instances()
{
    std::vector<std::shared_ptr<Instance>> vect;
    vect.emplace_back(std::make_shared<BaseInstance>(appId(), _registry));
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

    ubuntu_app_launch_start_application(appIdStr.c_str(), urlstrv.get());

    return std::make_shared<BaseInstance>(appId(), _registry);
}

std::shared_ptr<Application::Instance> Base::launchTest(const std::vector<Application::URL>& urls)
{
    std::string appIdStr = appId();
    std::shared_ptr<gchar*> urlstrv;

    if (urls.size() > 0)
    {
        urlstrv = urlsToStrv(urls);
    }

    ubuntu_app_launch_start_application_test(appIdStr.c_str(), urlstrv.get());

    return std::make_shared<BaseInstance>(appId(), _registry);
}

};  // namespace app_impls
};  // namespace app_launch
};  // namespace ubuntu
