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

std::pair<std::shared_ptr<GKeyFile>, std::string> manifestAppDesktop(const std::shared_ptr<JsonObject>& manifest,
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
{
    std::tie(_keyfile, desktopPath_) = manifestAppDesktop(_manifest, appid.package, appid.appname, _clickDir);
    if (!_keyfile)
        throw std::runtime_error{"No keyfile found for click application: " + std::string(appid)};

    g_debug("Application Click object for appid '%s'", std::string(appid).c_str());
}

AppID Click::appId()
{
    return _appid;
}

std::shared_ptr<Application::Info> Click::info()
{
    if (!_info)
    {
        _info = std::make_shared<app_info::Desktop>(appId(), _keyfile, _clickDir, _clickDir,
                                                    app_info::DesktopFlags::NONE, nullptr);
    }

    return _info;
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

std::pair<std::shared_ptr<GKeyFile>, std::string> manifestAppDesktop(const std::shared_ptr<JsonObject>& manifest,
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

    return std::make_pair(keyfile, std::string(path.get()));
}

std::vector<std::shared_ptr<Application::Instance>> Click::instances()
{
    auto vbase = _registry->impl->jobs->instances(appId(), "application-click");
    return std::vector<std::shared_ptr<Application::Instance>>(vbase.begin(), vbase.end());
}

/** Grabs all the environment variables for the application to
    launch in. It sets up the confinement ones and then adds in
    the APP_EXEC line and whether to use XMir */
std::list<std::pair<std::string, std::string>> Click::launchEnv()
{
    auto retval = confinedEnv(_appid.package, _clickDir);

    retval.emplace_back(std::make_pair("APP_DIR", _clickDir));
    retval.emplace_back(std::make_pair("APP_DESKTOP_FILE_PATH", desktopPath_));

    retval.emplace_back(std::make_pair("QML2_IMPORT_PATH", _clickDir + "/lib/" + UBUNTU_APP_LAUNCH_ARCH + "/qml"));

    info();

    retval.emplace_back(std::make_pair("APP_XMIR_ENABLE", _info->xMirEnable().value() ? "1" : "0"));
    retval.emplace_back(std::make_pair("APP_EXEC", _info->execLine().value()));

    retval.emplace_back(std::make_pair("APP_EXEC_POLICY", std::string(appId())));

    return retval;
}

std::shared_ptr<Application::Instance> Click::launch(const std::vector<Application::URL>& urls)
{
    std::function<std::list<std::pair<std::string, std::string>>(void)> envfunc = [this]() { return launchEnv(); };
    return _registry->impl->jobs->launch(appId(), "application-click", {}, urls, jobs::manager::launchMode::STANDARD,
                                         envfunc);
}

std::shared_ptr<Application::Instance> Click::launchTest(const std::vector<Application::URL>& urls)
{
    std::function<std::list<std::pair<std::string, std::string>>(void)> envfunc = [this]() { return launchEnv(); };
    return _registry->impl->jobs->launch(appId(), "application-click", {}, urls, jobs::manager::launchMode::TEST,
                                         envfunc);
}

std::shared_ptr<Application::Instance> Click::findInstance(const std::string& instanceid)
{
    return _registry->impl->jobs->existing(appId(), "application-click", instanceid, std::vector<Application::URL>{});
}

}  // namespace app_impls
}  // namespace app_launch
}  // namespace ubuntu
