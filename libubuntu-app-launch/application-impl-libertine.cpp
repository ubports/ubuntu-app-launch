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

#include "application-impl-libertine.h"
#include "libertine.h"
#include "registry-impl.h"

namespace ubuntu
{
namespace app_launch
{
namespace app_impls
{

Libertine::Libertine(const AppID::Package& container,
                     const AppID::AppName& appname,
                     const std::shared_ptr<Registry>& registry)
    : Base(registry)
    , _container(container)
    , _appname(appname)
{
    auto gcontainer_path = libertine_container_path(container.value().c_str());
    if (gcontainer_path != nullptr)
    {
        _container_path = gcontainer_path;
        g_free(gcontainer_path);
    }

    if (!_keyfile)
    {
        auto system_app_path = g_build_filename(_container_path.c_str(), "usr", "share", nullptr);
        _basedir = system_app_path;
        g_free(system_app_path);

        _keyfile = findDesktopFile(_basedir, "applications", appname.value() + ".desktop");
    }

    if (!_keyfile)
    {
        auto container_home_path = libertine_container_home_path(container.value().c_str());
        auto local_app_path = g_build_filename(container_home_path, ".local", "share", nullptr);
        _basedir = local_app_path;
        g_free(local_app_path);
        g_free(container_home_path);

        _keyfile = findDesktopFile(_basedir, "applications", appname.value() + ".desktop");
    }

    if (!_keyfile)
        throw std::runtime_error{"Unable to find a keyfile for application '" + appname.value() + "' in container '" +
                                 container.value() + "'"};
}

std::shared_ptr<GKeyFile> Libertine::keyfileFromPath(const std::string& pathname)
{
    std::shared_ptr<GKeyFile> keyfile(g_key_file_new(), [](GKeyFile* keyfile) {
        if (keyfile != nullptr)
        {
            g_key_file_free(keyfile);
        }
    });
    GError* error = nullptr;

    g_key_file_load_from_file(keyfile.get(), pathname.c_str(), G_KEY_FILE_NONE, &error);

    if (error != nullptr)
    {
        g_error_free(error);
        return {};
    }

    return keyfile;
}

std::shared_ptr<GKeyFile> Libertine::findDesktopFile(const std::string& basepath,
                                                     const std::string& subpath,
                                                     const std::string& filename)
{
    auto fullpath = g_build_filename(basepath.c_str(), subpath.c_str(), filename.c_str(), nullptr);
    std::string sfullpath(fullpath);
    g_free(fullpath);

    if (g_file_test(sfullpath.c_str(), G_FILE_TEST_IS_REGULAR))
    {
        return keyfileFromPath(sfullpath);
    }

    GError* error = nullptr;
    auto dirpath = g_build_filename(basepath.c_str(), subpath.c_str(), nullptr);
    GDir* dir = g_dir_open(dirpath, 0, &error);
    if (error != nullptr)
    {
        g_error_free(error);
        g_free(dirpath);
        return {};
    }
    g_free(dirpath);

    const gchar* file;
    while ((file = g_dir_read_name(dir)) != nullptr)
    {
        auto new_subpath = g_build_filename(subpath.c_str(), file, nullptr);
        auto new_fullpath = g_build_filename(basepath.c_str(), new_subpath, nullptr);
        if (g_file_test(new_fullpath, G_FILE_TEST_IS_DIR))
        {
            auto desktop_file = findDesktopFile(basepath, new_subpath, filename);

            if (desktop_file)
            {
                g_free(new_fullpath);
                g_free(new_subpath);
                g_dir_close(dir);
                return desktop_file;
            }
        }
        g_free(new_fullpath);
        g_free(new_subpath);
    }
    g_dir_close(dir);

    return {};
}

/** Checks the AppID by making sure the version is "0.0" and then
    calling verifyAppname() to check the rest.

    \param appid AppID to check
    \param registry persistent connections to use
*/
bool Libertine::hasAppId(const AppID& appid, const std::shared_ptr<Registry>& registry)
{
    try
    {
        if (appid.version.value() != "0.0")
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

/** Verify a package name by getting the list of containers from
    liblibertine and ensuring it is in that list.

    \param package Container name
    \param registry persistent connections to use
*/
bool Libertine::verifyPackage(const AppID::Package& package, const std::shared_ptr<Registry>& registry)
{
    auto containers = std::shared_ptr<gchar*>(libertine_list_containers(), g_strfreev);

    for (int i = 0; containers.get()[i] != nullptr; i++)
    {
        auto container = containers.get()[i];
        if (container == package.value())
        {
            return true;
        }
    }

    return false;
}

/** Gets the list of applications from the container using liblibertine
    and see if @appname is in that list.

    \param package Container name
    \param appname Application name to look for
    \param registry persistent connections to use
*/
bool Libertine::verifyAppname(const AppID::Package& package,
                              const AppID::AppName& appname,
                              const std::shared_ptr<Registry>& registry)
{
    auto apps = std::shared_ptr<gchar*>(libertine_list_apps_for_container(package.value().c_str()), g_strfreev);

    for (int i = 0; apps.get()[i] != nullptr; i++)
    {
        auto appid = AppID::parse(apps.get()[i]);
        if (appid.appname.value() == appname.value())
        {
            return true;
        }
    }

    return false;
}

/** We don't really have a way to implement this for Libertine, any
    search wouldn't really make sense. We just throw an error.

    \param package Container name
    \param card Application search paths
    \param registry persistent connections to use
*/
AppID::AppName Libertine::findAppname(const AppID::Package& package,
                                      AppID::ApplicationWildcard card,
                                      const std::shared_ptr<Registry>& registry)
{
    throw std::runtime_error("Legacy apps can't be discovered by package");
}

/** Function to return "0.0"

    \param package Container name (unused)
    \param appname Application name (unused)
    \param registry persistent connections to use (unused)
*/
AppID::Version Libertine::findVersion(const AppID::Package& package,
                                      const AppID::AppName& appname,
                                      const std::shared_ptr<Registry>& registry)
{
    return AppID::Version::from_raw("0.0");
}

std::list<std::shared_ptr<Application>> Libertine::list(const std::shared_ptr<Registry>& registry)
{
    std::list<std::shared_ptr<Application>> applist;

    auto containers = std::shared_ptr<gchar*>(libertine_list_containers(), g_strfreev);

    for (int i = 0; containers.get()[i] != nullptr; i++)
    {
        auto container = containers.get()[i];
        auto apps = std::shared_ptr<gchar*>(libertine_list_apps_for_container(container), g_strfreev);

        for (int j = 0; apps.get()[j] != nullptr; j++)
        {
            try
            {
                auto appid = AppID::parse(apps.get()[j]);
                auto sapp = std::make_shared<Libertine>(appid.package, appid.appname, registry);
                applist.emplace_back(sapp);
            }
            catch (std::runtime_error& e)
            {
                g_debug("Unable to create application for libertine appname '%s': %s", apps.get()[j], e.what());
            }
        }
    }

    return applist;
}

std::shared_ptr<Application::Info> Libertine::info()
{
    if (!appinfo_)
    {
        appinfo_ = std::make_shared<app_info::Desktop>(_keyfile, _basedir, _container_path,
                                                       app_info::DesktopFlags::XMIR_DEFAULT, _registry);
    }
    return appinfo_;
}

std::vector<std::shared_ptr<Application::Instance>> Libertine::instances()
{
    auto vbase = _registry->impl->jobs->instances(appId(), "application-legacy");
    return std::vector<std::shared_ptr<Application::Instance>>(vbase.begin(), vbase.end());
}

/** Grabs all the environment variables for the application to
    launch in. It sets up the confinement ones and then adds in
    the APP_EXEC line and whether to use XMir.

    This function adds 'libertine-launch' at the beginning of the
    Exec line with the container name as a parameter. The command
    can be overridden with the UBUNTU_APP_LAUNCH_LIBERTINE_LAUNCH
    environment variable.
*/
std::list<std::pair<std::string, std::string>> Libertine::launchEnv()
{
    std::list<std::pair<std::string, std::string>> retval;

    info();

    retval.emplace_back(std::make_pair("APP_XMIR_ENABLE", appinfo_->xMirEnable().value() ? "1" : "0"));

    /* The container is our confinement */
    retval.emplace_back(std::make_pair("APP_EXEC_POLICY", "unconfined"));

    auto libertine_launch = g_getenv("UBUNTU_APP_LAUNCH_LIBERTINE_LAUNCH");
    if (libertine_launch == nullptr)
    {
        libertine_launch = LIBERTINE_LAUNCH;
    }

    auto desktopexec = appinfo_->execLine().value();
    auto execline = std::string(libertine_launch) + " \"--id=" + _container.value() + "\" " + desktopexec;
    retval.emplace_back(std::make_pair("APP_EXEC", execline));

    /* TODO: Go multi instance */
    retval.emplace_back(std::make_pair("INSTANCE_ID", ""));

    return retval;
}

std::shared_ptr<Application::Instance> Libertine::launch(const std::vector<Application::URL>& urls)
{
    std::function<std::list<std::pair<std::string, std::string>>(void)> envfunc = [this]() { return launchEnv(); };
    return _registry->impl->jobs->launch(appId(), "application-legacy", {}, urls, jobs::manager::launchMode::STANDARD,
                                         envfunc);
}

std::shared_ptr<Application::Instance> Libertine::launchTest(const std::vector<Application::URL>& urls)
{
    std::function<std::list<std::pair<std::string, std::string>>(void)> envfunc = [this]() { return launchEnv(); };
    return _registry->impl->jobs->launch(appId(), "application-legacy", {}, urls, jobs::manager::launchMode::TEST,
                                         envfunc);
}

std::shared_ptr<Application::Instance> Libertine::findInstance(const std::string& instanceid)
{
    return _registry->impl->jobs->existing(appId(), "application-legacy", instanceid, std::vector<Application::URL>{});
}

}  // namespace app_impls
}  // namespace app_launch
}  // namespace ubuntu
