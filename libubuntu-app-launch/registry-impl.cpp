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
#include <regex>
#include <upstart.h>

namespace ubuntu
{
namespace app_launch
{

Registry::Impl::Impl(Registry* registry)
    : thread([]() {},
             [this]() {
                 _clickUser.reset();
                 _clickDB.reset();

                 zgLog_.reset();
                 jobs.reset();

                 if (_dbus)
                     g_dbus_connection_flush_sync(_dbus.get(), nullptr, nullptr);
                 _dbus.reset();
             })
    , _registry{registry}
    , _iconFinders()
{
    auto cancel = thread.getCancellable();
    _dbus = thread.executeOnThread<std::shared_ptr<GDBusConnection>>([cancel]() {
        return std::shared_ptr<GDBusConnection>(g_bus_get_sync(G_BUS_TYPE_SESSION, cancel.get(), nullptr),
                                                [](GDBusConnection* bus) { g_clear_object(&bus); });
    });

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

void Registry::Impl::initClick()
{
    if (_clickDB && _clickUser)
    {
        return;
    }

    auto init = thread.executeOnThread<bool>([this]() {
        GError* error = nullptr;

        if (!_clickDB)
        {
            _clickDB = std::shared_ptr<ClickDB>(click_db_new(), [](ClickDB* db) { g_clear_object(&db); });
            /* If TEST_CLICK_DB is unset, this reads the system database. */
            click_db_read(_clickDB.get(), g_getenv("TEST_CLICK_DB"), &error);

            if (error != nullptr)
            {
                auto perror = std::shared_ptr<GError>(error, [](GError* error) { g_error_free(error); });
                throw std::runtime_error(perror->message);
            }
        }

        if (!_clickUser)
        {
            _clickUser =
                std::shared_ptr<ClickUser>(click_user_new_for_user(_clickDB.get(), g_getenv("TEST_CLICK_USER"), &error),
                                           [](ClickUser* user) { g_clear_object(&user); });

            if (error != nullptr)
            {
                auto perror = std::shared_ptr<GError>(error, [](GError* error) { g_error_free(error); });
                throw std::runtime_error(perror->message);
            }
        }

        g_debug("Initialized Click DB");
        return true;
    });

    if (!init)
    {
        throw std::runtime_error("Unable to initialize the Click Database");
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
    auto gstr = json_to_string(jsonnode.get(), TRUE);

    if (gstr != nullptr)
    {
        retval = gstr;
        g_free(gstr);
    }

    return retval;
}

std::shared_ptr<JsonObject> Registry::Impl::getClickManifest(const std::string& package)
{
    initClick();

    auto retval = thread.executeOnThread<std::shared_ptr<JsonObject>>([this, package]() {
        GError* error = nullptr;
        auto mani = click_user_get_manifest(_clickUser.get(), package.c_str(), &error);

        if (error != nullptr)
        {
            auto perror = std::shared_ptr<GError>(error, [](GError* error) { g_error_free(error); });
            g_debug("Error parsing manifest for package '%s': %s", package.c_str(), perror->message);
            return std::shared_ptr<JsonObject>();
        }

        auto node = json_node_alloc();
        json_node_init_object(node, mani);
        json_object_unref(mani);

        auto retval = std::shared_ptr<JsonObject>(json_node_dup_object(node), json_object_unref);

        json_node_free(node);

        return retval;
    });

    if (!retval)
        throw std::runtime_error("Unable to get Click manifest for package: " + package);

    return retval;
}

std::list<AppID::Package> Registry::Impl::getClickPackages()
{
    initClick();

    return thread.executeOnThread<std::list<AppID::Package>>([this]() {
        GError* error = nullptr;
        GList* pkgs = click_user_get_package_names(_clickUser.get(), &error);

        if (error != nullptr)
        {
            auto perror = std::shared_ptr<GError>(error, [](GError* error) { g_error_free(error); });
            throw std::runtime_error(perror->message);
        }

        std::list<AppID::Package> list;
        for (GList* item = pkgs; item != nullptr; item = g_list_next(item))
        {
            auto pkgobj = static_cast<gchar*>(item->data);
            if (pkgobj)
            {
                list.emplace_back(AppID::Package::from_raw(pkgobj));
            }
        }

        g_list_free_full(pkgs, g_free);
        return list;
    });
}

std::string Registry::Impl::getClickDir(const std::string& package)
{
    initClick();

    return thread.executeOnThread<std::string>([this, package]() {
        GError* error = nullptr;
        auto dir = click_user_get_path(_clickUser.get(), package.c_str(), &error);

        if (error != nullptr)
        {
            auto perror = std::shared_ptr<GError>(error, [](GError* error) { g_error_free(error); });
            throw std::runtime_error(perror->message);
        }

        std::string cppdir(dir);
        g_free(dir);
        return cppdir;
    });
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
            zgLog_ =
                std::shared_ptr<ZeitgeistLog>(zeitgeist_log_new(), /* create a new log for us */
                                              [](ZeitgeistLog* log) { g_clear_object(&log); }); /* Free as a GObject */
        }

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

        zeitgeist_log_insert_event(zgLog_.get(), /* log */
                                   event,        /* event */
                                   nullptr,      /* cancellable */
                                   [](GObject* obj, GAsyncResult* res, gpointer user_data) {
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

        g_object_unref(event);
        g_object_unref(subject);
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

core::Signal<const std::shared_ptr<Application>&>& Registry::Impl::appInfoUpdated()
{
    return sig_appInfoUpdated;
}

}  // namespace app_launch
}  // namespace ubuntu
