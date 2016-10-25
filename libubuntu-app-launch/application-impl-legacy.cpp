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

#include <regex>

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

Legacy::Legacy(const AppID::AppName& appname, const std::shared_ptr<Registry>& registry)
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

    appinfo_ = std::make_shared<app_info::Desktop>(_keyfile, _basedir, rootDir,
                                                   app_info::DesktopFlags::ALLOW_NO_DISPLAY, _registry);

    if (!_keyfile)
    {
        throw std::runtime_error{"Unable to find keyfile for legacy application: " + appname.value()};
    }

    if (std::equal(snappyDesktopPath.begin(), snappyDesktopPath.end(), _basedir.begin()))
    {
        throw std::runtime_error{"Looking like a legacy app, but should be a Snap: " + appname.value()};
    }
}

std::tuple<std::string, std::shared_ptr<GKeyFile>, std::string> keyfileForApp(const AppID::AppName& name)
{
    auto desktopName = name.value() + ".desktop";
    std::string desktopPath;
    auto keyfilecheck = [desktopName, &desktopPath](const std::string& dir) -> std::shared_ptr<GKeyFile> {
        auto fullname = g_build_filename(dir.c_str(), "applications", desktopName.c_str(), nullptr);
        if (!g_file_test(fullname, G_FILE_TEST_EXISTS))
        {
            g_free(fullname);
            return {};
        }
        desktopPath = fullname;

        auto keyfile = std::shared_ptr<GKeyFile>(g_key_file_new(), clear_keyfile);

        GError* error = nullptr;
        g_key_file_load_from_file(keyfile.get(), fullname, G_KEY_FILE_NONE, &error);
        g_free(fullname);

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

/** Checks the AppID by ensuring the version and package are empty
    then looks for the application.

    \param appid AppID to check
    \param registry persistent connections to use
*/
bool Legacy::hasAppId(const AppID& appid, const std::shared_ptr<Registry>& registry)
{
    try
    {
        if (!appid.version.value().empty())
        {
            return false;
        }

        return verifyAppname(appid.package, appid.appname, registry);
    }
    catch (std::runtime_error& e)
    {
        return false;
    }
}

/** Ensure the package is empty

    \param package Container name
    \param registry persistent connections to use
*/
bool Legacy::verifyPackage(const AppID::Package& package, const std::shared_ptr<Registry>& registry)
{
    return package.value().empty();
}

/** Looks for an application by looking through the system and user
    application directories to find the desktop file.

    \param package Container name
    \param appname Application name to look for
    \param registry persistent connections to use
*/
bool Legacy::verifyAppname(const AppID::Package& package,
                           const AppID::AppName& appname,
                           const std::shared_ptr<Registry>& registry)
{
    if (!verifyPackage(package, registry))
    {
        throw std::runtime_error{"Invalid Legacy package: " + std::string(package)};
    }

    auto desktop = std::string(appname) + ".desktop";
    auto evaldir = [&desktop](const gchar* dir) -> bool {
        char* fulldir = g_build_filename(dir, "applications", desktop.c_str(), nullptr);
        gboolean found = g_file_test(fulldir, G_FILE_TEST_EXISTS);
        g_free(fulldir);
        return found == TRUE;
    };

    if (evaldir(g_get_user_data_dir()))
    {
        return true;
    }

    const char* const* data_dirs = g_get_system_data_dirs();
    for (int i = 0; data_dirs[i] != nullptr; i++)
    {
        if (evaldir(data_dirs[i]))
        {
            return true;
        }
    }

    return false;
}

/** We don't really have a way to implement this for Legacy, any
    search wouldn't really make sense. We just throw an error.

    \param package Container name
    \param card Application search paths
    \param registry persistent connections to use
*/
AppID::AppName Legacy::findAppname(const AppID::Package& package,
                                   AppID::ApplicationWildcard card,
                                   const std::shared_ptr<Registry>& registry)
{
    throw std::runtime_error("Legacy apps can't be discovered by package");
}

/** Function to return an empty string

    \param package Container name (unused)
    \param appname Application name (unused)
    \param registry persistent connections to use (unused)
*/
AppID::Version Legacy::findVersion(const AppID::Package& package,
                                   const AppID::AppName& appname,
                                   const std::shared_ptr<Registry>& registry)
{
    return AppID::Version::from_raw({});
}

static const std::regex desktop_remover("^(.*)\\.desktop$");

std::list<std::shared_ptr<Application>> Legacy::list(const std::shared_ptr<Registry>& registry)
{
    std::list<std::shared_ptr<Application>> list;
    GList* head = g_app_info_get_all();
    for (GList* item = head; item != nullptr; item = g_list_next(item))
    {
        GDesktopAppInfo* appinfo = G_DESKTOP_APP_INFO(item->data);

        if (appinfo == nullptr)
        {
            continue;
        }

        if (g_app_info_should_show(G_APP_INFO(appinfo)) == FALSE)
        {
            continue;
        }

        auto desktopappid = std::string(g_app_info_get_id(G_APP_INFO(appinfo)));
        std::string appname;
        std::smatch match;
        if (std::regex_match(desktopappid, match, desktop_remover))
        {
            appname = match[1].str();
        }
        else
        {
            continue;
        }

        /* Remove entries generated by the desktop hook in .local */
        if (g_desktop_app_info_has_key(appinfo, "X-Ubuntu-Application-ID"))
        {
            continue;
        }

        try
        {
            auto app = std::make_shared<Legacy>(AppID::AppName::from_raw(appname), registry);
            list.push_back(app);
        }
        catch (std::runtime_error& e)
        {
            g_debug("Unable to create application for legacy appname '%s': %s", appname.c_str(), e.what());
        }
    }

    g_list_free_full(head, g_object_unref);

    return list;
}

std::vector<std::shared_ptr<Application::Instance>> Legacy::instances()
{
    auto vbase = _registry->impl->jobs->instances(appId(), "application-legacy");
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
    if (appinfo_->xMirEnable())
    {
        /* If we're setting up XMir we also need the other helpers
           that libertine is helping with */
        auto libertine_launch = g_getenv("UBUNTU_APP_LAUNCH_LIBERTINE_LAUNCH");
        if (libertine_launch == nullptr)
        {
            libertine_launch = LIBERTINE_LAUNCH;
        }

        retval.emplace_back(
            std::make_pair("APP_EXEC", std::string(libertine_launch) + " " + appinfo_->execLine().value()));
    }
    else
    {
        retval.emplace_back(std::make_pair("APP_EXEC", appinfo_->execLine().value()));
    }

    /* Honor the 'Path' key if it is in the desktop file */
    if (g_key_file_has_key(_keyfile.get(), "Desktop Entry", "Path", nullptr))
    {
        gchar* path = g_key_file_get_string(_keyfile.get(), "Desktop Entry", "Path", nullptr);
        retval.emplace_back(std::make_pair("APP_DIR", path));
        g_free(path);
    }

    /* If they've asked for an Apparmor profile, let's use it! */
    gchar* apparmor = g_key_file_get_string(_keyfile.get(), "Desktop Entry", "X-Ubuntu-AppArmor-Profile", nullptr);
    if (apparmor != nullptr)
    {
        retval.emplace_back(std::make_pair("APP_EXEC_POLICY", apparmor));
        g_free(apparmor);

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
    return _registry->impl->jobs->launch(appId(), "application-legacy", instance, urls,
                                         jobs::manager::launchMode::STANDARD, envfunc);
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
    return _registry->impl->jobs->launch(appId(), "application-legacy", instance, urls, jobs::manager::launchMode::TEST,
                                         envfunc);
}

}  // namespace app_impls
}  // namespace app_launch
}  // namespace ubuntu
