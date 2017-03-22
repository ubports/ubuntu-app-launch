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

#include "registry-impl.h"
#include "application-icon-finder.h"
#include "application-impl-base.h"
#include <regex>
#include <unity/util/GObjectMemory.h>
#include <unity/util/GlibMemory.h>

using namespace unity::util;

namespace ubuntu
{
namespace app_launch
{

Registry::Impl::Impl(Registry& registry)
    : Impl(registry, app_store::Base::allAppStores())
{
}

Registry::Impl::Impl(Registry& registry, std::list<std::shared_ptr<app_store::Base>> appStores)
    : thread([]() {},
             [this]() {
                 zgLog_.reset();
                 jobs.reset();

                 if (_dbus)
                     g_dbus_connection_flush_sync(_dbus.get(), nullptr, nullptr);
                 _dbus.reset();
             })
    , _registry{registry}
    , _iconFinders()
    , _appStores(appStores)
{
    auto cancel = thread.getCancellable();
    _dbus = thread.executeOnThread<std::shared_ptr<GDBusConnection>>(
        [cancel]() { return share_gobject(g_bus_get_sync(G_BUS_TYPE_SESSION, cancel.get(), nullptr)); });

    /* Determine where we're getting the helper from */
    auto goomHelper = g_getenv("UBUNTU_APP_LAUNCH_OOM_HELPER");
    if (goomHelper != nullptr)
    {
        oomHelper_ = goomHelper;
    }
    else
    {
        oomHelper_ = OOM_HELPER;
    }
}

/** Helper function for printing JSON objects to debug output */
std::string Registry::Impl::printJson(std::shared_ptr<JsonObject> jsonobj)
{
    auto node = json_node_alloc();
    json_node_init_object(node, jsonobj.get());

    auto snode = std::shared_ptr<JsonNode>(node, json_node_unref);
    return printJson(snode);
}

/** Helper function for printing JSON nodes to debug output */
std::string Registry::Impl::printJson(std::shared_ptr<JsonNode> jsonnode)
{
    std::string retval;
    auto gstr = unique_gchar(json_to_string(jsonnode.get(), TRUE));

    if (gstr)
    {
        retval = gstr.get();
    }

    return retval;
}

/** Send an event to Zietgeist using the registry thread so that
    the callback comes back in the right place. */
void Registry::Impl::zgSendEvent(AppID appid, const std::string& eventtype)
{
    thread.executeOnThread([this, appid, eventtype] {
        std::string uri;

        if (appid.package.value().empty())
        {
            uri = "application://" + appid.appname.value() + ".desktop";
        }
        else
        {
            uri = "application://" + appid.package.value() + "_" + appid.appname.value() + ".desktop";
        }

        g_debug("Sending ZG event for '%s': %s", uri.c_str(), eventtype.c_str());

        if (!zgLog_)
        {
            zgLog_ = share_gobject(zeitgeist_log_new()); /* create a new log for us */
        }

        auto event = unique_gobject(zeitgeist_event_new());
        zeitgeist_event_set_actor(event.get(), "application://ubuntu-app-launch.desktop");
        zeitgeist_event_set_interpretation(event.get(), eventtype.c_str());
        zeitgeist_event_set_manifestation(event.get(), ZEITGEIST_ZG_USER_ACTIVITY);

        auto subject = unique_gobject(zeitgeist_subject_new());
        zeitgeist_subject_set_interpretation(subject.get(), ZEITGEIST_NFO_SOFTWARE);
        zeitgeist_subject_set_manifestation(subject.get(), ZEITGEIST_NFO_SOFTWARE_ITEM);
        zeitgeist_subject_set_mimetype(subject.get(), "application/x-desktop");
        zeitgeist_subject_set_uri(subject.get(), uri.c_str());

        zeitgeist_event_add_subject(event.get(), subject.get());

        zeitgeist_log_insert_event(zgLog_.get(), /* log */
                                   event.get(),  /* event */
                                   nullptr,      /* cancellable */
                                   [](GObject* obj, GAsyncResult* res, gpointer user_data) {
                                       GError* error = nullptr;

                                       unique_glib(zeitgeist_log_insert_event_finish(ZEITGEIST_LOG(obj), res, &error));

                                       if (error != nullptr)
                                       {
                                           g_warning("Unable to submit Zeitgeist Event: %s", error->message);
                                           g_error_free(error);
                                       }
                                   },        /* callback */
                                   nullptr); /* userdata */

    });
}

std::shared_ptr<IconFinder> Registry::Impl::getIconFinder(std::string basePath)
{
    if (_iconFinders.find(basePath) == _iconFinders.end())
    {
        _iconFinders[basePath] = std::make_shared<IconFinder>(basePath);
    }
    return _iconFinders[basePath];
}

/** App start watching, if we're registered for the signal we
    can't wait on it. We are making this static right now because
    we need it to go across the C and C++ APIs smoothly, and those
    can end up with different registry objects. Long term, this
    should become a member variable. */
static bool watchingAppStarting_ = false;

/** Variable to track if this program is watching app startup
    so that we can know to not wait on the response to that. */
void Registry::Impl::watchingAppStarting(bool rWatching)
{
    watchingAppStarting_ = rWatching;
}

/** Accessor for the internal variable to know whether an app
    is watching for app startup */
bool Registry::Impl::isWatchingAppStarting()
{
    return watchingAppStarting_;
}

/** Sets up the signals down to the info watchers and we aggregate
    them up to users of UAL. We connect to all their signals and
    pass them up. */
void Registry::Impl::infoWatchersSetup(const std::shared_ptr<Registry>& reg)
{
    std::call_once(flag_infoWatchersSetup, [this, reg] {
        g_debug("Info watchers signals setup");

        /* Grab all the app stores and the ZG info watcher */
        std::list<std::shared_ptr<info_watcher::Base>> watchers{_appStores.begin(), _appStores.end()};
        watchers.push_back(Registry::Impl::getZgWatcher(reg));

        /* Connect each of their signals to us, and track that connection */
        for (const auto& watcher : watchers)
        {
            infoWatchers_.emplace_back(std::make_pair(
                watcher,
                infoWatcherConnections{
                    watcher->infoChanged().connect(
                        [this](const std::shared_ptr<Application>& app) { sig_appInfoUpdated(app); }),
                    watcher->appAdded().connect([this](const std::shared_ptr<Application>& app) { sig_appAdded(app); }),
                    watcher->appRemoved().connect([this](const AppID& appid) { sig_appRemoved(appid); }),
                }));
        }
    });
}

core::Signal<const std::shared_ptr<Application>&>& Registry::Impl::appInfoUpdated(const std::shared_ptr<Registry>& reg)
{
    infoWatchersSetup(reg);
    return sig_appInfoUpdated;
}

core::Signal<const std::shared_ptr<Application>&>& Registry::Impl::appAdded(const std::shared_ptr<Registry>& reg)
{
    infoWatchersSetup(reg);
    return sig_appAdded;
}

core::Signal<const AppID&>& Registry::Impl::appRemoved(const std::shared_ptr<Registry>& reg)
{
    infoWatchersSetup(reg);
    return sig_appRemoved;
}

}  // namespace app_launch
}  // namespace ubuntu
