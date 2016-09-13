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

#include "application-impl-click.h"
#include "application-info-desktop.h"
#include "registry-impl.h"

#include <algorithm>

namespace ubuntu
{
namespace app_launch
{
namespace app_impls
{

AppID::Version manifestVersion(const std::shared_ptr<JsonObject>& manifest);
std::list<AppID::AppName> manifestApps(const std::shared_ptr<JsonObject>& manifest);
std::shared_ptr<GKeyFile> manifestAppDesktop(const std::shared_ptr<JsonObject>& manifest,
                                             const std::string& package,
                                             const std::string& app,
                                             const std::string& clickDir);

Click::Click(const AppID& appid, const std::shared_ptr<Registry>& registry)
    : Click(appid, registry->impl->getClickManifest(appid.package), registry)
{
}

Click::Click(const AppID& appid, const std::shared_ptr<JsonObject>& manifest, const std::shared_ptr<Registry>& registry)
    : Base(registry)
    , _appid(appid)
    , _manifest(manifest)
    , _clickDir(registry->impl->getClickDir(appid.package))
    , _keyfile(manifestAppDesktop(manifest, appid.package, appid.appname, _clickDir))
{
    if (!_keyfile)
        throw std::runtime_error{"No keyfile found for click application: " + (std::string)appid};
}

AppID Click::appId()
{
    return _appid;
}

std::shared_ptr<Application::Info> Click::info()
{
    return std::make_shared<app_info::Desktop>(_keyfile, _clickDir);
}

AppID::Version manifestVersion(const std::shared_ptr<JsonObject>& manifest)
{
    const gchar* cstr = nullptr;
    if (!json_object_has_member(manifest.get(), "version") ||
        (cstr = json_object_get_string_member(manifest.get(), "version")) == nullptr)
    {
        throw std::runtime_error("Unable to find version number in manifest: " + Registry::Impl::printJson(manifest));
    }

    auto cppstr = AppID::Version::from_raw(cstr);
    return cppstr;
}

std::list<AppID::AppName> manifestApps(const std::shared_ptr<JsonObject>& manifest)
{
    JsonObject* hooks = nullptr;
    if (!json_object_has_member(manifest.get(), "hooks") ||
        (hooks = json_object_get_object_member(manifest.get(), "hooks")) == nullptr)
    {
        throw std::runtime_error("Manifest does not have a 'hooks' field: " + Registry::Impl::printJson(manifest));
    }

    auto gapps = json_object_get_members(hooks);
    if (gapps == nullptr)
    {
        throw std::runtime_error("GLib JSON confusion, please talk to your library vendor");
    }

    std::list<AppID::AppName> apps;

    for (GList* item = gapps; item != nullptr; item = g_list_next(item))
    {
        auto appname = (const gchar*)item->data;

        auto hooklist = json_object_get_object_member(hooks, appname);

        if (json_object_has_member(hooklist, "desktop") == TRUE)
        {
            apps.emplace_back(AppID::AppName::from_raw(appname));
        }
    }

    g_list_free(gapps);
    return apps;
}

std::shared_ptr<GKeyFile> manifestAppDesktop(const std::shared_ptr<JsonObject>& manifest,
                                             const std::string& package,
                                             const std::string& app,
                                             const std::string& clickDir)
{
    if (!manifest)
    {
        throw std::runtime_error("No manifest for package '" + package + "'");
    }

    JsonObject* hooks = nullptr;
    if (!json_object_has_member(manifest.get(), "hooks") ||
        (hooks = json_object_get_object_member(manifest.get(), "hooks")) == nullptr)
    {
        throw std::runtime_error("Manifest for application '" + app + "' does not have a 'hooks' field: " +
                                 Registry::Impl::printJson(manifest));
    }

    auto gapps = json_object_get_members(hooks);
    if (gapps == nullptr)
    {
        throw std::runtime_error("GLib JSON confusion, please talk to your library vendor");
    }
    else
    {
        g_list_free(gapps);
    }

    JsonObject* hooklist = nullptr;
    if (!json_object_has_member(hooks, app.c_str()) ||
        (hooklist = json_object_get_object_member(hooks, app.c_str())) == nullptr)
    {
        throw std::runtime_error("Manifest for does not have an application '" + app + "': " +
                                 Registry::Impl::printJson(manifest));
    }

    auto desktoppath = json_object_get_string_member(hooklist, "desktop");
    if (desktoppath == nullptr)
        throw std::runtime_error("Manifest for application '" + app + "' does not have a 'desktop' hook: " +
                                 Registry::Impl::printJson(manifest));

    auto path = std::shared_ptr<gchar>(g_build_filename(clickDir.c_str(), desktoppath, nullptr), g_free);

    std::shared_ptr<GKeyFile> keyfile(g_key_file_new(), g_key_file_free);
    GError* error = nullptr;
    g_key_file_load_from_file(keyfile.get(), path.get(), G_KEY_FILE_NONE, &error);
    if (error != nullptr)
    {
        auto perror = std::shared_ptr<GError>(error, g_error_free);
        throw std::runtime_error(perror.get()->message);
    }

    return keyfile;
}

std::list<std::shared_ptr<Application>> Click::list(const std::shared_ptr<Registry>& registry)
{
    std::list<std::shared_ptr<Application>> applist;

    try
    {
        for (auto pkg : registry->impl->getClickPackages())
        {
            try
            {
                auto manifest = registry->impl->getClickManifest(pkg);

                for (auto appname : manifestApps(manifest))
                {
                    try
                    {
                        AppID appid{pkg, appname, manifestVersion(manifest)};
                        auto app = std::make_shared<Click>(appid, manifest, registry);
                        applist.emplace_back(app);
                    }
                    catch (std::runtime_error& e)
                    {
                        g_debug("Unable to create Click for application '%s' in package '%s': %s",
                                appname.value().c_str(), pkg.value().c_str(), e.what());
                    }
                }
            }
            catch (std::runtime_error& e)
            {
                g_debug("Unable to get information to build Click app on package '%s': %s", pkg.value().c_str(),
                        e.what());
            }
        }
    }
    catch (std::runtime_error& e)
    {
        g_debug("Unable to get packages from Click database: %s", e.what());
    }

    return applist;
}

std::vector<std::shared_ptr<Application::Instance>> Click::instances()
{
    std::vector<std::shared_ptr<Instance>> vect;
    std::string sappid = appId();

    for (auto instancename : _registry->impl->upstartInstancesForJob("application-click"))
    {
        /* There an be only one, but we want to make sure it is
           there or return an empty vector */
        if (sappid == instancename)
        {
            vect.emplace_back(std::make_shared<UpstartInstance>(appId(), "application-click", sappid, _registry));
            break;
        }
    }
    return vect;
}

std::shared_ptr<Application::Instance> Click::launch(const std::vector<Application::URL>& urls)
{
    return UpstartInstance::launch(appId(), "application-click", std::string(appId()), urls, _registry,
                                   UpstartInstance::launchMode::STANDARD);
}

std::shared_ptr<Application::Instance> Click::launchTest(const std::vector<Application::URL>& urls)
{
    return UpstartInstance::launch(appId(), "application-click", std::string(appId()), urls, _registry,
                                   UpstartInstance::launchMode::TEST);
}

}  // namespace app_impls
}  // namespace app_launch
}  // namespace ubuntu
