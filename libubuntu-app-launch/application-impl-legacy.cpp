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

Legacy::Legacy(const AppID::AppName& appname, const std::shared_ptr<Registry>& registry)
    : Base(registry)
    , _appname(appname)
{
    _basedir = g_get_user_data_dir();
    auto keyfile_path = find_desktop_file(_basedir, "applications", appname.value() + ".desktop");
    _keyfile = keyfileFromPath(keyfile_path);

    if (!_keyfile)
    {
        auto systemDirs = g_get_system_data_dirs();
        for (auto i = 0; systemDirs[i] != nullptr; i++)
        {
            _basedir = systemDirs[i];
            auto keyfile_path = find_desktop_file(_basedir, "applications", appname.value() + ".desktop");
            _keyfile = keyfileFromPath(keyfile_path);
            if (_keyfile)
            {
                break;
            }
        }
    }

    appinfo_ = std::make_shared<app_info::Desktop>(_keyfile, _basedir, _registry, true, false);

    if (!_keyfile)
    {
        throw std::runtime_error{"Unable to find keyfile for legacy application: " + appname.value()};
    }

    if (std::equal(snappyDesktopPath.begin(), snappyDesktopPath.end(), _basedir.begin()))
    {
        throw std::runtime_error{"Looking like a legacy app, but should be a Snap: " + appname.value()};
    }
}

std::shared_ptr<Application::Info> Legacy::info()
{
    return appinfo_;
}

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

bool Legacy::verifyPackage(const AppID::Package& package, const std::shared_ptr<Registry>& registry)
{
    return package.value().empty();
}

bool Legacy::verifyAppname(const AppID::Package& package,
                           const AppID::AppName& appname,
                           const std::shared_ptr<Registry>& registry)
{
    if (!verifyPackage(package, registry))
    {
        throw std::runtime_error{"Invalid Legacy package: " + std::string(package)};
    }

    std::string desktop = std::string(appname) + ".desktop";
    std::function<bool(const gchar* dir)> evaldir = [&desktop](const gchar* dir) {
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

AppID::AppName Legacy::findAppname(const AppID::Package& package,
                                   AppID::ApplicationWildcard card,
                                   const std::shared_ptr<Registry>& registry)
{
    throw std::runtime_error("Legacy apps can't be discovered by package");
}

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
        auto fileappid = g_desktop_app_info_get_string(appinfo, "x-Ubuntu-UAL-Application-ID");
        if (fileappid != nullptr)
        {
            g_free(fileappid);
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
    std::vector<std::shared_ptr<Instance>> vect;
    auto startsWith = std::string(appId()) + "-";

    for (auto instance : _registry->impl->upstartInstancesForJob("application-legacy"))
    {
        g_debug("Looking at legacy instance: %s", instance.c_str());
        if (std::equal(startsWith.begin(), startsWith.end(), instance.begin()))
        {
            vect.emplace_back(std::make_shared<UpstartInstance>(appId(), "application-legacy", instance,
                                                                std::vector<Application::URL>{}, _registry));
        }
    }

    g_debug("Legacy app '%s' has %d instances", std::string(appId()).c_str(), int(vect.size()));

    return vect;
}

std::list<std::pair<std::string, std::string>> Legacy::launchEnv(const std::string& instance)
{
    std::list<std::pair<std::string, std::string>> retval;

    /* TODO: Not sure how we're gonna get this */
    /* APP_DESKTOP_FILE_PATH */

    info();

    retval.emplace_back(std::make_pair("APP_XMIR_ENABLE", appinfo_->xMirEnable().value() ? "1" : "0"));
    retval.emplace_back(std::make_pair("APP_EXEC", appinfo_->execLine().value()));

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

std::string Legacy::getInstance()
{
    auto single = g_key_file_get_boolean(_keyfile.get(), "Desktop Entry", "X-Ubuntu-Single-Instance", nullptr);
    if (single)
    {
        return {};
    }
    else
    {
        return std::to_string(g_get_real_time());
    }
}

std::shared_ptr<Application::Instance> Legacy::launch(const std::vector<Application::URL>& urls)
{
    std::string instance = getInstance();
    return UpstartInstance::launch(appId(), "application-legacy", "-" + instance, urls, _registry,
                                   UpstartInstance::launchMode::STANDARD,
                                   [this, instance]() { return launchEnv(instance); });
}

std::shared_ptr<Application::Instance> Legacy::launchTest(const std::vector<Application::URL>& urls)
{
    std::string instance = getInstance();
    return UpstartInstance::launch(appId(), "application-legacy", "-" + instance, urls, _registry,
                                   UpstartInstance::launchMode::TEST,
                                   [this, instance]() { return launchEnv(instance); });
}

}  // namespace app_impls
}  // namespace app_launch
}  // namespace ubuntu
