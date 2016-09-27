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

#include <regex>

#include "application-impl-snap.h"
#include "application-info-desktop.h"
#include "registry-impl.h"

namespace ubuntu
{
namespace app_launch
{
namespace app_impls
{

/************************
 ** Interface Lists
 ************************/

/** All the interfaces that we support running applications with */
const std::set<std::string> SUPPORTED_INTERFACES{"unity8", "unity7", "x11"};
/** All the interfaces that we run XMir for by default */
const std::set<std::string> XMIR_INTERFACES{"unity7", "x11"};
/** All the interfaces that we tell Unity support lifecycle */
const std::set<std::string> LIFECYCLE_INTERFACES{"unity8"};
/** Snappy has more restrictive appnames than everyone else */
const std::regex appnameRegex{"^[a-zA-Z0-9](?:-?[a-zA-Z0-9])*$"};

/************************
 ** Info support
 ************************/

/** Subclassing the desktop info object so that we can override a couple
    of properties with interface definitions. This may grow as we add more
    fields to the desktop spec that come from Snappy interfaces. */
class SnapInfo : public app_info::Desktop
{
    /** The core interface for this snap */
    std::string interface_;
    /** AppID of snap */
    AppID appId_;

public:
    SnapInfo(const AppID& appid,
             const std::shared_ptr<Registry>& registry,
             const std::string& interface,
             const std::string& snapDir)
        : Desktop(
              [appid, snapDir]() -> std::shared_ptr<GKeyFile> {
                  /* This is a function to get the keyfile out of the snap using
                     the paths that snappy places things inside the dir. */
                  std::string path = snapDir + "/meta/gui/" + appid.appname.value() + ".desktop";
                  std::shared_ptr<GKeyFile> keyfile(g_key_file_new(), g_key_file_free);
                  GError* error = nullptr;
                  g_key_file_load_from_file(keyfile.get(), path.c_str(), G_KEY_FILE_NONE, &error);
                  if (error != nullptr)
                  {
                      auto perror = std::shared_ptr<GError>(error, g_error_free);
                      throw std::runtime_error("Unable to find keyfile for '" + std::string(appid) + "' at '" + path +
                                               "' because: " + perror.get()->message);
                  }

                  return keyfile;
              }(),
              snapDir,
              app_info::DesktopFlags::NONE,
              registry)
        , interface_(interface)
        , appId_(appid)
    {
    }

    /** Return the xMirEnable value based on whether the interface is
        in the list of interfaces using XMir */
    XMirEnable xMirEnable() override
    {
        if (XMIR_INTERFACES.find(interface_) != XMIR_INTERFACES.end())
        {
            return XMirEnable::from_raw(true);
        }
        else
        {
            return XMirEnable::from_raw(false);
        }
    }

    /** Return the xMirEnable value based on whether the interface is
        in the list of interfaces supporting the lifecycle */
    UbuntuLifecycle supportsUbuntuLifecycle() override
    {
        if (LIFECYCLE_INTERFACES.find(interface_) != LIFECYCLE_INTERFACES.end())
        {
            return UbuntuLifecycle::from_raw(true);
        }
        else
        {
            return UbuntuLifecycle::from_raw(false);
        }
    }

    /** Figures out the exec line for a snappy command. We're not using
        the Exec in the desktop file exactly, but assuming that it is kinda
        what we want to be run. So we're replacing that with the script, which
        we have to use as we can't get the command that is in the snap
        metadata as Snapd won't give it to us. So we're parsing the Exec line
        and replacing the first entry. Then putting it back together again. */
    Exec execLine() override
    {
        std::string keyfile = _exec.value();
        gchar** parsed = nullptr;
        GError* error = nullptr;

        g_shell_parse_argv(keyfile.c_str(), nullptr, &parsed, &error);

        if (error != nullptr)
        {
            g_warning("Unable to parse exec line '%s': %s", keyfile.c_str(), error->message);
            g_error_free(error);
            return Exec::from_raw({});
        }

        if (g_strv_length(parsed) == 0)
        {
            g_warning("Parse resulted in a blank line");
            g_strfreev(parsed);
            return Exec::from_raw({});
        }

        /* Skip the first entry */
        gchar** parsedpp = &(parsed[1]);

        gchar* params = g_strjoinv(" ", parsedpp);
        g_strfreev(parsed);

        std::string binname;
        if (appId_.package.value() == appId_.appname.value())
        {
            binname = appId_.package.value();
        }
        else
        {
            binname = appId_.package.value() + " " + appId_.appname.value();
        }

        binname = "/snap/bin/" + binname + " " + params;
        g_free(params);

        return Exec::from_raw(binname);
    }
};

/************************
 ** Snap implementation
 ************************/

/** Creates a Snap application object. Will throw exceptions if the AppID
    doesn't resolve into a valid package or that package doesn't have a desktop
    file that matches the app name.

    \param appid Application ID of the snap
    \param registry Registry to use for persistent connections
    \param interface Primary interface that we found this snap for
*/
Snap::Snap(const AppID& appid, const std::shared_ptr<Registry>& registry, const std::string& interface)
    : Base(registry)
    , appid_(appid)
    , interface_(interface)
{
    pkgInfo_ = registry->impl->snapdInfo.pkgInfo(appid.package);
    if (!pkgInfo_)
    {
        throw std::runtime_error("Unable to get snap package info for AppID: " + std::string(appid));
    }

    if (!checkPkgInfo(pkgInfo_, appid_))
    {
        throw std::runtime_error("AppID does not match installed package for: " + std::string(appid));
    }

    info_ = std::make_shared<SnapInfo>(appid_, _registry, interface_, pkgInfo_->directory);
}

/** Uses the findInterface() function to find the interface if we don't
    have one.

    \param appid Application ID of the snap
    \param registry Registry to use for persistent connections
*/
Snap::Snap(const AppID& appid, const std::shared_ptr<Registry>& registry)
    : Snap(appid, registry, findInterface(appid, registry))
{
}

/** Lists all the Snappy apps that are using one of our supported interfaces.
    Also makes sure they're valid.

    \param registry Registry to use for persistent connections
*/
std::list<std::shared_ptr<Application>> Snap::list(const std::shared_ptr<Registry>& registry)
{
    std::list<std::shared_ptr<Application>> apps;

    for (const auto& interface : SUPPORTED_INTERFACES)
    {
        for (const auto& id : registry->impl->snapdInfo.appsForInterface(interface))
        {
            try
            {
                auto app = std::make_shared<Snap>(id, registry, interface);
                apps.emplace_back(app);
            }
            catch (std::runtime_error& e)
            {
                g_warning("Unable to make Snap object for '%s': %s", std::string(id).c_str(), e.what());
            }
        }
    }

    return apps;
}

/** Returns the stored AppID */
AppID Snap::appId()
{
    return appid_;
}

/** Asks Snapd for the interfaces to determine which one the application
    can support.

    \param appid Application ID of the snap
    \param registry Registry to use for persistent connections
*/
std::string Snap::findInterface(const AppID& appid, const std::shared_ptr<Registry>& registry)
{
    auto ifaceset = registry->impl->snapdInfo.interfacesForAppId(appid);

    for (const auto& interface : SUPPORTED_INTERFACES)
    {
        if (ifaceset.find(interface) != ifaceset.end())
        {
            return interface;
        }
    }

    throw std::runtime_error("Interface not found for: " + std::string(appid));
}

/** Checks a PkgInfo structure to ensure that it matches the AppID */
bool Snap::checkPkgInfo(const std::shared_ptr<snapd::Info::PkgInfo>& pkginfo, const AppID& appid)
{
    if (!pkginfo)
    {
        return false;
    }

    return pkginfo->revision == appid.version.value() &&
           pkginfo->appnames.find(appid.appname) != pkginfo->appnames.end();
}

/** Checks if an AppID could be a snap. Note it doesn't look for a desktop
    file just the package, app and version. This is done to make the lookup
    quickly, as this function can be used to select which backend to use
    and we want to reject quickly.

    \param appid Application ID of the snap
    \param registry Registry to use for persistent connections
*/
bool Snap::hasAppId(const AppID& appId, const std::shared_ptr<Registry>& registry)
{
    if (!std::regex_match(appId.appname.value(), appnameRegex))
    {
        return false;
    }

    auto pkginfo = registry->impl->snapdInfo.pkgInfo(appId.package);
    return checkPkgInfo(pkginfo, appId);
}

/** Look to see if a package is a valid Snap package name

    \param package Package name
    \param registry Registry to use for persistent connections
*/
bool Snap::verifyPackage(const AppID::Package& package, const std::shared_ptr<Registry>& registry)
{
    try
    {
        auto pkgInfo = registry->impl->snapdInfo.pkgInfo(package);
        return pkgInfo != nullptr;
    }
    catch (std::runtime_error& e)
    {
        return false;
    }
}

/** Look to see if an appname is a valid for a Snap package

    \param package Package name
    \param appname Command name
    \param registry Registry to use for persistent connections
*/
bool Snap::verifyAppname(const AppID::Package& package,
                         const AppID::AppName& appname,
                         const std::shared_ptr<Registry>& registry)
{
    if (!std::regex_match(appname.value(), appnameRegex))
    {
        return false;
    }

    auto pkgInfo = registry->impl->snapdInfo.pkgInfo(package);
    return pkgInfo->appnames.find(appname) != pkgInfo->appnames.end();
}

/** Look for an application name on a Snap package based on a
    wildcard type.

    \param package Package name
    \param card Wildcard to use for finding the appname
    \param registry Registry to use for persistent connections
*/
AppID::AppName Snap::findAppname(const AppID::Package& package,
                                 AppID::ApplicationWildcard card,
                                 const std::shared_ptr<Registry>& registry)
{
    auto pkgInfo = registry->impl->snapdInfo.pkgInfo(package);

    if (pkgInfo->appnames.empty())
    {
        throw std::runtime_error("No apps in package '" + package.value() + "' to find");
    }

    switch (card)
    {
        case AppID::ApplicationWildcard::FIRST_LISTED:
            return AppID::AppName::from_raw(*pkgInfo->appnames.begin());
        case AppID::ApplicationWildcard::LAST_LISTED:
            return AppID::AppName::from_raw(*pkgInfo->appnames.rbegin());
        case AppID::ApplicationWildcard::ONLY_LISTED:
            if (pkgInfo->appnames.size() != 1)
            {
                throw std::runtime_error("More than a single app in package '" + package.value() +
                                         "' when requested to find only app");
            }
            return AppID::AppName::from_raw(*pkgInfo->appnames.begin());
    }

    throw std::logic_error("Got a value of the app wildcard enum that can't exist");
}

/** Look for a version of a Snap package

    \param package Package name
    \param appname Not used for snaps
    \param registry Registry to use for persistent connections
*/
AppID::Version Snap::findVersion(const AppID::Package& package,
                                 const AppID::AppName& appname,
                                 const std::shared_ptr<Registry>& registry)
{
    auto pkgInfo = registry->impl->snapdInfo.pkgInfo(package);
    return AppID::Version::from_raw(pkgInfo->revision);
}

/** Returns a reference to the info for the snap */
std::shared_ptr<Application::Info> Snap::info()
{
    return info_;
}

/** Get all of the instances of this snap package that are running */
std::vector<std::shared_ptr<Application::Instance>> Snap::instances()
{
    std::vector<std::shared_ptr<Instance>> vect;
    auto startsWith = std::string(appid_) + "-";

    for (const auto& instance : _registry->impl->upstartInstancesForJob("application-snap"))
    {
        if (std::equal(startsWith.begin(), startsWith.end(), instance.begin()))
        {
            vect.emplace_back(_registry->impl->instances->existing(appid_, "application-snap", std::string{},
                                                                   std::vector<Application::URL>{}));
        }
    }

    return vect;
}

/** Return the launch environment for this snap. That includes whether
    or not it needs help from XMir (including Libertine helpers)
*/
std::list<std::pair<std::string, std::string>> Snap::launchEnv()
{
    g_debug("Getting snap specific environment");
    std::list<std::pair<std::string, std::string>> retval;

    retval.emplace_back(std::make_pair("APP_XMIR_ENABLE", info_->xMirEnable().value() ? "1" : "0"));
    if (info_->xMirEnable())
    {
        /* If we're setting up XMir we also need the other helpers
           that libertine is helping with */
        /* retval.emplace_back(std::make_pair("APP_EXEC", "libertine-launch --no-container " +
         * info_->execLine().value())); */
        /* Not yet */
        retval.emplace_back(std::make_pair("APP_EXEC", info_->execLine().value()));
    }
    else
    {
        retval.emplace_back(std::make_pair("APP_EXEC", info_->execLine().value()));
    }

    return retval;
}

/** Create a new instance of this Snap

    \param urls URLs to pass to the command
*/
std::shared_ptr<Application::Instance> Snap::launch(const std::vector<Application::URL>& urls)
{
    std::function<std::list<std::pair<std::string, std::string>>(void)> envfunc = [this]() { return launchEnv(); };
    return _registry->impl->instances->launch(appid_, "application-snap", {}, urls,
                                              InstanceFactory::launchMode::STANDARD, envfunc);
}

/** Create a new instance of this Snap with a testing environment
    setup for it.

    \param urls URLs to pass to the command
*/
std::shared_ptr<Application::Instance> Snap::launchTest(const std::vector<Application::URL>& urls)
{
    std::function<std::list<std::pair<std::string, std::string>>(void)> envfunc = [this]() { return launchEnv(); };
    return _registry->impl->instances->launch(appid_, "application-snap", {}, urls, InstanceFactory::launchMode::TEST,
                                              envfunc);
}

}  // namespace app_impls
}  // namespace app_launch
}  // namespace ubuntu
