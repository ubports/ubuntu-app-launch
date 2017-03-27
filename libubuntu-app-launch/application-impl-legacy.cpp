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

#include "application-impl-legacy.h"
#include "application-info-desktop.h"
#include "registry-impl.h"
#include "string-util.h"

#include <regex>
#include <unity/util/GlibMemory.h>

using namespace unity::util;

namespace ubuntu
{
namespace app_launch
{
namespace app_impls
{

/** Path that snapd puts desktop files, we don't want to read those directly
    in the Legacy backend. We want to use the snap backend. */
const std::string snappyDesktopPath{"/var/lib/snapd"};

/***********************************
   Prototypes
 ***********************************/
std::tuple<std::string, std::shared_ptr<GKeyFile>, std::string> keyfileForApp(const AppID::AppName& name);

/** Helper function to put on shared_ptr's for keyfiles */
void clear_keyfile(GKeyFile* keyfile)
{
    if (keyfile != nullptr)
    {
        g_key_file_free(keyfile);
    }
}

Legacy::Legacy(const AppID::AppName& appname, const std::shared_ptr<Registry::Impl>& registry)
    : Base(registry)
    , _appname(appname)
{
    std::tie(_basedir, _keyfile, desktopPath_) = keyfileForApp(appname);

    std::string rootDir = "";
    auto rootenv = g_getenv("UBUNTU_APP_LAUNCH_LEGACY_ROOT");
    if (rootenv != nullptr && /* Check that we have an alternate root available */
        g_str_has_prefix(_basedir.c_str(), rootenv))
    { /* And check that we found this in that root */
        rootDir = rootenv;
    }

    auto flags = app_info::DesktopFlags::ALLOW_NO_DISPLAY;

    if (!g_key_file_has_key(_keyfile.get(), "Desktop Entry", "X-Ubuntu-Touch", nullptr))
    {
        flags |= app_info::DesktopFlags::XMIR_DEFAULT;
    }

    appinfo_ = std::make_shared<app_info::Desktop>(appId(), _keyfile, _basedir, rootDir, flags, registry_);

    if (!_keyfile)
    {
        throw std::runtime_error{"Unable to find keyfile for legacy application: " + appname.value()};
    }

    if (std::equal(snappyDesktopPath.begin(), snappyDesktopPath.end(), _basedir.begin()))
    {
        throw std::runtime_error{"Looking like a legacy app, but should be a Snap: " + appname.value()};
    }

    g_debug("Application Legacy object for app '%s'", appname.value().c_str());
}

std::tuple<std::string, std::shared_ptr<GKeyFile>, std::string> keyfileForApp(const AppID::AppName& name)
{
    auto desktopName = name.value() + ".desktop";
    std::string desktopPath;
    auto keyfilecheck = [desktopName, &desktopPath](const std::string& dir) -> std::shared_ptr<GKeyFile> {
        auto fullname = unique_gchar(g_build_filename(dir.c_str(), "applications", desktopName.c_str(), nullptr));
        if (!g_file_test(fullname.get(), G_FILE_TEST_EXISTS))
        {
            return {};
        }
        desktopPath = fullname.get();

        auto keyfile = unique_glib(g_key_file_new());

        GError* error = nullptr;
        g_key_file_load_from_file(keyfile.get(), fullname.get(), G_KEY_FILE_NONE, &error);

        if (error != nullptr)
        {
            g_debug("Unable to load keyfile '%s' becuase: %s", desktopName.c_str(), error->message);
            g_error_free(error);
            return {};
        }

        return keyfile;
    };

    std::string basedir = g_get_user_data_dir();
    auto retval = keyfilecheck(basedir);

    auto systemDirs = g_get_system_data_dirs();
    for (auto i = 0; !retval && systemDirs[i] != nullptr; i++)
    {
        basedir = systemDirs[i];
        retval = keyfilecheck(basedir);
    }

    return std::make_tuple(basedir, retval, desktopPath);
}

std::shared_ptr<Application::Info> Legacy::info()
{
    return appinfo_;
}

std::vector<std::shared_ptr<Application::Instance>> Legacy::instances()
{
    auto vbase = registry_->jobs->instances(appId(), "application-legacy");
    return std::vector<std::shared_ptr<Application::Instance>>(vbase.begin(), vbase.end());
}

/** Grabs all the environment for a legacy app. Mostly this consists of
    the exec line and whether it needs XMir. Also we set the path if that
    is specified in the desktop file. We can also set an AppArmor profile
    if requested. */
std::list<std::pair<std::string, std::string>> Legacy::launchEnv(const std::string& instance)
{
    std::list<std::pair<std::string, std::string>> retval;

    retval.emplace_back(std::make_pair("APP_DESKTOP_FILE_PATH", desktopPath_));

    info();

    retval.emplace_back(std::make_pair("APP_XMIR_ENABLE", appinfo_->xMirEnable().value() ? "1" : "0"));
    auto execline = appinfo_->execLine().value();

    auto snappath = getenv("SNAP");
    if (snappath != nullptr)
    {
        /* This means we're inside a snap, and if we're in a snap then
           the legacy application is in a snap. We need to try and set
           up the proper environment for that app */
        retval.emplace_back(std::make_pair("SNAP", snappath));

        const char* legacyexec = getenv("UBUNTU_APP_LAUNCH_SNAP_LEGACY_EXEC");
        if (legacyexec == nullptr)
        {
            legacyexec = "/snap/bin/unity8-session.legacy-exec";
        }

        execline = std::string{legacyexec} + " " + execline;
    }
    else if (appinfo_->xMirEnable().value())
    {
        /* If we're setting up XMir we also need the other helpers
           that libertine is helping with */
        auto libertine_launch = g_getenv("UBUNTU_APP_LAUNCH_LIBERTINE_LAUNCH");
        if (libertine_launch == nullptr)
        {
            libertine_launch = LIBERTINE_LAUNCH;
        }

        execline = std::string{libertine_launch} + " " + execline;
    }

    retval.emplace_back(std::make_pair("APP_EXEC", execline));

    /* Honor the 'Path' key if it is in the desktop file */
    if (g_key_file_has_key(_keyfile.get(), "Desktop Entry", "Path", nullptr))
    {
        auto path = unique_gchar(g_key_file_get_string(_keyfile.get(), "Desktop Entry", "Path", nullptr));
        retval.emplace_back(std::make_pair("APP_DIR", path.get()));
    }

    /* If they've asked for an Apparmor profile, let's use it! */
    auto apparmor =
        unique_gchar(g_key_file_get_string(_keyfile.get(), "Desktop Entry", "X-Ubuntu-AppArmor-Profile", nullptr));
    if (apparmor)
    {
        retval.emplace_back(std::make_pair("APP_EXEC_POLICY", apparmor.get()));

        retval.splice(retval.end(), confinedEnv(_appname, "/usr/share"));
    }
    else
    {
        retval.emplace_back(std::make_pair("APP_EXEC_POLICY", "unconfined"));
    }

    retval.emplace_back(std::make_pair("INSTANCE_ID", instance));

    return retval;
}

/** Create an UpstartInstance for this AppID using the UpstartInstance launch
    function.

    \param urls URLs to pass to the application
*/
std::shared_ptr<Application::Instance> Legacy::launch(const std::vector<Application::URL>& urls)
{
    auto instance = getInstance(appinfo_);
    std::function<std::list<std::pair<std::string, std::string>>(void)> envfunc = [this, instance]() {
        return launchEnv(instance);
    };
    return registry_->jobs->launch(appId(), "application-legacy", instance, urls, jobs::manager::launchMode::STANDARD,
                                   envfunc);
}

/** Create an UpstartInstance for this AppID using the UpstartInstance launch
    function with a testing environment.

    \param urls URLs to pass to the application
*/
std::shared_ptr<Application::Instance> Legacy::launchTest(const std::vector<Application::URL>& urls)
{
    auto instance = getInstance(appinfo_);
    std::function<std::list<std::pair<std::string, std::string>>(void)> envfunc = [this, instance]() {
        return launchEnv(instance);
    };
    return registry_->jobs->launch(appId(), "application-legacy", instance, urls, jobs::manager::launchMode::TEST,
                                   envfunc);
}

std::shared_ptr<Application::Instance> Legacy::findInstance(const std::string& instanceid)
{
    return registry_->jobs->existing(appId(), "application-legacy", instanceid, std::vector<Application::URL>{});
}

}  // namespace app_impls
}  // namespace app_launch
}  // namespace ubuntu
