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
#include "string-util.h"

#include <unity/util/GlibMemory.h>

using namespace unity::util;

namespace ubuntu
{
namespace app_launch
{
namespace app_impls
{

/************************
 ** Interface Lists
 ************************/

/** All the interfaces that we run XMir for by default */
const std::set<std::string> X11_INTERFACES{"unity7", "x11"};
/** The interface to indicate direct Mir support */
const std::string MIR_INTERFACE{"mir"};
/** The interface to indicate Ubuntu lifecycle support */
const std::string LIFECYCLE_INTERFACE{"unity8"};
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
    /** AppID of snap */
    AppID appId_;

public:
    SnapInfo(const AppID& appid,
             const std::shared_ptr<Registry>& registry,
             const Snap::InterfaceInfo& interfaceInfo,
             const std::string& snapDir)
        : Desktop(appid,
                  [appid, snapDir]() -> std::shared_ptr<GKeyFile> {
                      /* This is a function to get the keyfile out of the snap using
                         the paths that snappy places things inside the dir. */
                      std::string path = snapDir + "/meta/gui/" + appid.appname.value() + ".desktop";
                      auto keyfile = share_glib(g_key_file_new());
                      GError* error = nullptr;
                      g_key_file_load_from_file(keyfile.get(), path.c_str(), G_KEY_FILE_NONE, &error);
                      if (error != nullptr)
                      {
                          auto perror = unique_glib(error);
                          error = nullptr;
                          throw std::runtime_error("Unable to find keyfile for '" + std::string(appid) + "' at '" +
                                                   path + "' because: " + perror.get()->message);
                      }

                      /* For bad reasons the Icon values in snaps have gotten to be a
                         bit crazy. We're going to try to un-fu-bar a few common patterns
                         here, but eh, we're just encouraging bad behavior */
                      auto iconvalue =
                          unique_gchar(g_key_file_get_string(keyfile.get(), "Desktop Entry", "Icon", nullptr));
                      if (iconvalue)
                      {
                          const gchar* prefix{nullptr};
                          if (g_str_has_prefix(iconvalue.get(), "${SNAP}/"))
                          {
                              /* There isn't environment parsing in desktop file values :-( */
                              prefix = "${SNAP}/";
                          }

                          auto currentdir = std::string{"/snap/"} + appid.package.value() + "/current/";
                          if (g_str_has_prefix(iconvalue.get(), currentdir.c_str()))
                          {
                              /* What? Why would we encode the snap path from root in a package
                                 format that is supposed to be relocatable? */
                              prefix = currentdir.c_str();
                          }

                          if (prefix != nullptr)
                          {
                              g_key_file_set_string(keyfile.get(), "Desktop Entry", "Icon",
                                                    iconvalue.get() + strlen(prefix) - 1);
                              /* -1 to leave trailing slash */
                          }
                      }

                      return keyfile;
                  }(),
                  snapDir,
                  snapDir,
                  app_info::DesktopFlags::NONE,
                  registry)
        , appId_(appid)
    {
        _xMirEnable = std::get<0>(interfaceInfo);
        _ubuntuLifecycle = std::get<1>(interfaceInfo);
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
        GError* error = nullptr;

        GCharVUPtr parsed(nullptr, &g_strfreev);
        {
            gchar** tmp = nullptr;
            g_shell_parse_argv(keyfile.c_str(), nullptr, &tmp, &error);
            parsed = unique_gcharv(tmp);
        }

        if (error != nullptr)
        {
            g_warning("Unable to parse exec line '%s': %s", keyfile.c_str(), error->message);
            g_error_free(error);
            return Exec::from_raw({});
        }

        if (g_strv_length(parsed.get()) == 0)
        {
            g_warning("Parse resulted in a blank line");
            return Exec::from_raw({});
        }

        /* Skip the first entry */
        gchar** parsedpp = &(parsed.get()[1]);

        auto params = unique_gchar(g_strjoinv(" ", parsedpp));

        std::string binname;
        if (appId_.package.value() == appId_.appname.value())
        {
            binname = appId_.package.value();
        }
        else
        {
            binname = appId_.package.value() + "." + appId_.appname.value();
        }

        binname = "/snap/bin/" + binname + " " + params.get();

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
    \param interfaceInfo Metadata gleaned from the snap's interfaces
*/
Snap::Snap(const AppID& appid, const std::shared_ptr<Registry>& registry, const InterfaceInfo& interfaceInfo)
    : Base(registry)
    , appid_(appid)
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

    info_ = std::make_shared<SnapInfo>(appid_, _registry, interfaceInfo, pkgInfo_->directory);

    g_debug("Application Snap object for AppID '%s'", std::string(appid).c_str());
}

/** Uses the findInterfaceInfo() function to find the interface if we don't
    have one.

    \param appid Application ID of the snap
    \param registry Registry to use for persistent connections
*/
Snap::Snap(const AppID& appid, const std::shared_ptr<Registry>& registry)
    : Snap(appid, registry, findInterfaceInfo(appid, registry))
{
}

/** Returns the stored AppID */
AppID Snap::appId() const
{
    return appid_;
}

/** Asks Snapd for the interfaces to determine which ones the application
    can support.

    \param appid Application ID of the snap
    \param registry Registry to use for persistent connections
*/
Snap::InterfaceInfo Snap::findInterfaceInfo(const AppID& appid, const std::shared_ptr<Registry>& registry)
{
    auto ifaceset = registry->impl->snapdInfo.interfacesForAppId(appid);
    auto xMirEnable = app_info::Desktop::XMirEnable::from_raw(false);
    auto ubuntuLifecycle = Application::Info::UbuntuLifecycle::from_raw(false);

    if (ifaceset.find(LIFECYCLE_INTERFACE) != ifaceset.end())
    {
        ubuntuLifecycle = Application::Info::UbuntuLifecycle::from_raw(true);
    }

    if (ifaceset.find(MIR_INTERFACE) == ifaceset.end())
    {
        for (const auto& interface : X11_INTERFACES)
        {
            if (ifaceset.find(interface) != ifaceset.end())
            {
                xMirEnable = app_info::Desktop::XMirEnable::from_raw(true);
                break;
            }
        }

        if (!xMirEnable.value())
        {
            throw std::runtime_error("Graphical interface not found for: " + std::string(appid));
        }
    }

    return std::make_tuple(xMirEnable, ubuntuLifecycle);
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

/** Returns a reference to the info for the snap */
std::shared_ptr<Application::Info> Snap::info()
{
    return info_;
}

/** Get all of the instances of this snap package that are running */
std::vector<std::shared_ptr<Application::Instance>> Snap::instances()
{
    auto vbase = _registry->impl->jobs->instances(appId(), "application-snap");
    return std::vector<std::shared_ptr<Application::Instance>>(vbase.begin(), vbase.end());
}

/** Return the launch environment for this snap. That includes whether
    or not it needs help from XMir (including Libertine helpers)
*/
std::list<std::pair<std::string, std::string>> Snap::launchEnv()
{
    g_debug("Getting snap specific environment");
    std::list<std::pair<std::string, std::string>> retval;

    retval.emplace_back(std::make_pair("APP_XMIR_ENABLE", info_->xMirEnable().value() ? "1" : "0"));
    if (info_->xMirEnable() && getenv("SNAP") == nullptr)
    {
        /* If we're setting up XMir we also need the other helpers
           that libertine is helping with */
        auto libertine_launch = g_getenv("UBUNTU_APP_LAUNCH_LIBERTINE_LAUNCH");
        if (libertine_launch == nullptr)
        {
            libertine_launch = LIBERTINE_LAUNCH;
        }

        retval.emplace_back(
            std::make_pair("APP_EXEC", std::string(libertine_launch) + " " + info_->execLine().value()));
    }
    else
    {
        /* If we're in a snap the libertine helpers are setup by
           the snap stuff */
        retval.emplace_back(std::make_pair("APP_EXEC", info_->execLine().value()));
    }

    return retval;
}

/** Create a new instance of this Snap

    \param urls URLs to pass to the command
*/
std::shared_ptr<Application::Instance> Snap::launch(const std::vector<Application::URL>& urls)
{
    auto instance = getInstance(info_);
    std::function<std::list<std::pair<std::string, std::string>>(void)> envfunc = [this]() { return launchEnv(); };
    return _registry->impl->jobs->launch(appid_, "application-snap", instance, urls,
                                         jobs::manager::launchMode::STANDARD, envfunc);
}

/** Create a new instance of this Snap with a testing environment
    setup for it.

    \param urls URLs to pass to the command
*/
std::shared_ptr<Application::Instance> Snap::launchTest(const std::vector<Application::URL>& urls)
{
    auto instance = getInstance(info_);
    std::function<std::list<std::pair<std::string, std::string>>(void)> envfunc = [this]() { return launchEnv(); };
    return _registry->impl->jobs->launch(appid_, "application-snap", instance, urls, jobs::manager::launchMode::TEST,
                                         envfunc);
}

std::shared_ptr<Application::Instance> Snap::findInstance(const std::string& instanceid)
{
    return _registry->impl->jobs->existing(appId(), "application-snap", instanceid, std::vector<Application::URL>{});
}

}  // namespace app_impls
}  // namespace app_launch
}  // namespace ubuntu
