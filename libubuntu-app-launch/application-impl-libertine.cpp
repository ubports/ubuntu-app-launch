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
#include "string-util.h"

#include <unity/util/GlibMemory.h>

using namespace unity::util;

namespace ubuntu
{
namespace app_launch
{
namespace app_impls
{

Libertine::Libertine(const AppID::Package& container,
                     const AppID::AppName& appname,
                     const std::shared_ptr<Registry::Impl>& registry)
    : Base(registry)
    , _container(container)
    , _appname(appname)
{
    auto gcontainer_path = unique_gchar(libertine_container_path(container.value().c_str()));
    if (gcontainer_path)
    {
        _container_path = gcontainer_path.get();
    }

    if (!_keyfile)
    {
        auto system_app_path = unique_gchar(g_build_filename(_container_path.c_str(), "usr", "share", nullptr));
        _basedir = system_app_path.get();

        _keyfile = findDesktopFile(_basedir, "applications", appname.value() + ".desktop");
    }

    if (!_keyfile)
    {
        auto container_home_path = unique_gchar(libertine_container_home_path(container.value().c_str()));
        auto local_app_path = unique_gchar(g_build_filename(container_home_path.get(), ".local", "share", nullptr));
        _basedir = local_app_path.get();

        _keyfile = findDesktopFile(_basedir, "applications", appname.value() + ".desktop");
    }

    if (!_keyfile)
        throw std::runtime_error{"Unable to find a keyfile for application '" + appname.value() + "' in container '" +
                                 container.value() + "'"};

    appinfo_ = std::make_shared<app_info::Desktop>(appId(), _keyfile, _basedir, _container_path,
                                                   app_info::DesktopFlags::XMIR_DEFAULT, registry_);

    g_debug("Application Libertine object for container '%s' app '%s'", container.value().c_str(),
            appname.value().c_str());
}

std::shared_ptr<GKeyFile> Libertine::keyfileFromPath(const std::string& pathname)
{
    auto keyfile = share_glib(g_key_file_new());
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
    auto fullpath = unique_gchar(g_build_filename(basepath.c_str(), subpath.c_str(), filename.c_str(), nullptr));
    std::string sfullpath(fullpath.get());

    if (g_file_test(sfullpath.c_str(), G_FILE_TEST_IS_REGULAR))
    {
        return keyfileFromPath(sfullpath);
    }

    GError* error = nullptr;
    auto dirpath = unique_gchar(g_build_filename(basepath.c_str(), subpath.c_str(), nullptr));
    auto dir = unique_glib(g_dir_open(dirpath.get(), 0, &error));
    if (error != nullptr)
    {
        g_error_free(error);
        return {};
    }

    const gchar* file;
    while ((file = g_dir_read_name(dir.get())) != nullptr)
    {
        auto new_subpath = unique_gchar(g_build_filename(subpath.c_str(), file, nullptr));
        auto new_fullpath = unique_gchar(g_build_filename(basepath.c_str(), new_subpath.get(), nullptr));
        if (g_file_test(new_fullpath.get(), G_FILE_TEST_IS_DIR))
        {
            auto desktop_file = findDesktopFile(basepath, new_subpath.get(), filename);

            if (desktop_file)
            {
                return desktop_file;
            }
        }
    }

    return {};
}

std::shared_ptr<Application::Info> Libertine::info()
{
    return appinfo_;
}

std::vector<std::shared_ptr<Application::Instance>> Libertine::instances()
{
    auto vbase = registry_->jobs()->instances(appId(), "application-legacy");
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
    auto instance = getInstance(appinfo_);
    std::function<std::list<std::pair<std::string, std::string>>(void)> envfunc = [this]() { return launchEnv(); };
    return registry_->jobs()->launch(appId(), "application-legacy", instance, urls, jobs::manager::launchMode::STANDARD,
                                     envfunc);
}

std::shared_ptr<Application::Instance> Libertine::launchTest(const std::vector<Application::URL>& urls)
{
    auto instance = getInstance(appinfo_);
    std::function<std::list<std::pair<std::string, std::string>>(void)> envfunc = [this]() { return launchEnv(); };
    return registry_->jobs()->launch(appId(), "application-legacy", instance, urls, jobs::manager::launchMode::TEST,
                                     envfunc);
}

std::shared_ptr<Application::Instance> Libertine::findInstance(const std::string& instanceid)
{
    return registry_->jobs()->existing(appId(), "application-legacy", instanceid, std::vector<Application::URL>{});
}

}  // namespace app_impls
}  // namespace app_launch
}  // namespace ubuntu
