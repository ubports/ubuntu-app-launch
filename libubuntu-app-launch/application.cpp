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

extern "C" {
#include "ubuntu-app-launch.h"
}

#include "application-impl-click.h"
#include "application-impl-legacy.h"
#include "application-impl-libertine.h"
#include "application.h"
#include "registry.h"

#include <functional>
#include <iostream>
#include <regex>

namespace ubuntu
{
namespace app_launch
{

std::shared_ptr<Application> Application::create(const AppID& appid, const std::shared_ptr<Registry>& registry)
{
    if (appid.empty())
    {
        throw std::runtime_error("AppID is empty");
    }

    if (app_impls::Click::hasAppId(appid, registry))
    {
        return std::make_shared<app_impls::Click>(appid, registry);
    }
    else if (app_impls::Libertine::hasAppId(appid, registry))
    {
        return std::make_shared<app_impls::Libertine>(appid.package, appid.appname, registry);
    }
    else if (app_impls::Legacy::hasAppId(appid, registry))
    {
        return std::make_shared<app_impls::Legacy>(appid.appname, registry);
    }
    else
    {
        throw std::runtime_error("Invalid app ID: " + std::string(appid));
    }
}

AppID::AppID()
    : package(Package::from_raw({}))
    , appname(AppName::from_raw({}))
    , version(Version::from_raw({}))
{
}

AppID::AppID(Package pkg, AppName app, Version ver)
    : package(pkg)
    , appname(app)
    , version(ver)
{
}

#define REGEX_PKGNAME "([a-z0-9][a-z0-9+.-]+)"
#define REGEX_APPNAME "([A-Za-z0-9+-.:~-][\\sA-Za-z0-9+-.:~-]+)"
#define REGEX_VERSION "([\\d+:]?[A-Za-z0-9.+:~-]+?(?:-[A-Za-z0-9+.~]+)?)"

const std::regex full_appid_regex("^" REGEX_PKGNAME "_" REGEX_APPNAME "_" REGEX_VERSION "$");
const std::regex short_appid_regex("^" REGEX_PKGNAME "_" REGEX_APPNAME "$");
const std::regex legacy_appid_regex("^" REGEX_APPNAME "$");

AppID AppID::parse(const std::string& sappid)
{
    std::smatch match;

    if (std::regex_match(sappid, match, full_appid_regex))
    {
        return {AppID::Package::from_raw(match[1].str()), AppID::AppName::from_raw(match[2].str()),
                AppID::Version::from_raw(match[3].str())};
    }
    else
    {
        /* Allow returning an empty AppID with empty internal */
        return {AppID::Package::from_raw({}), AppID::AppName::from_raw({}), AppID::Version::from_raw({})};
    }
}

bool AppID::valid(const std::string& sappid)
{
    return std::regex_match(sappid, full_appid_regex);
}

AppID AppID::find(const std::string& sappid)
{
    auto registry = Registry::getDefault();
    return find(registry, sappid);
}

AppID AppID::find(const std::shared_ptr<Registry>& registry, const std::string& sappid)
{
    std::smatch match;

    if (std::regex_match(sappid, match, full_appid_regex))
    {
        return {AppID::Package::from_raw(match[1].str()), AppID::AppName::from_raw(match[2].str()),
                AppID::Version::from_raw(match[3].str())};
    }
    else if (std::regex_match(sappid, match, short_appid_regex))
    {
        return discover(registry, match[1].str(), match[2].str());
    }
    else if (std::regex_match(sappid, match, legacy_appid_regex))
    {
        return {AppID::Package::from_raw({}), AppID::AppName::from_raw(sappid), AppID::Version::from_raw({})};
    }
    else
    {
        return {AppID::Package::from_raw({}), AppID::AppName::from_raw({}), AppID::Version::from_raw({})};
    }
}

AppID::operator std::string() const
{
    if (package.value().empty() && version.value().empty())
    {
        if (appname.value().empty())
        {
            return {};
        }
        else
        {
            return appname.value();
        }
    }

    return package.value() + "_" + appname.value() + "_" + version.value();
}

bool operator==(const AppID& a, const AppID& b)
{
    return a.package.value() == b.package.value() && a.appname.value() == b.appname.value() &&
           a.version.value() == b.version.value();
}

bool operator!=(const AppID& a, const AppID& b)
{
    return a.package.value() != b.package.value() || a.appname.value() != b.appname.value() ||
           a.version.value() != b.version.value();
}

bool operator<(const AppID& a, const AppID& b)
{
    return std::string(a) < std::string(b);
}

bool AppID::empty() const
{
    return package.value().empty() && appname.value().empty() && version.value().empty();
}

/* Basically we're making our own VTable of static functions. Static
   functions don't go in the normal VTables, so we can't use our class
   inheritance here to help. So we're just packing these puppies into
   a data structure and itterating over it. */
struct DiscoverTools
{
    std::function<bool(const AppID::Package& package, const std::shared_ptr<Registry>& registry)> verifyPackage;
    std::function<bool(
        const AppID::Package& package, const AppID::AppName& appname, const std::shared_ptr<Registry>& registry)>
        verifyAppname;
    std::function<AppID::AppName(
        const AppID::Package& package, AppID::ApplicationWildcard card, const std::shared_ptr<Registry>& registry)>
        findAppname;
    std::function<AppID::Version(
        const AppID::Package& package, const AppID::AppName& appname, const std::shared_ptr<Registry>& registry)>
        findVersion;
    std::function<bool(const AppID& appid, const std::shared_ptr<Registry>& registry)> hasAppId;
};

static const std::vector<DiscoverTools> discoverTools{
    /* Click */
    {app_impls::Click::verifyPackage, app_impls::Click::verifyAppname, app_impls::Click::findAppname,
     app_impls::Click::findVersion, app_impls::Click::hasAppId},
    /* Libertine */
    {app_impls::Libertine::verifyPackage, app_impls::Libertine::verifyAppname, app_impls::Libertine::findAppname,
     app_impls::Libertine::findVersion, app_impls::Libertine::hasAppId},
    /* Legacy */
    {app_impls::Legacy::verifyPackage, app_impls::Legacy::verifyAppname, app_impls::Legacy::findAppname,
     app_impls::Legacy::findVersion, app_impls::Legacy::hasAppId}};

AppID AppID::discover(const std::shared_ptr<Registry>& registry,
                      const std::string& package,
                      const std::string& appname,
                      const std::string& version)
{
    auto pkg = AppID::Package::from_raw(package);

    for (auto tools : discoverTools)
    {
        /* Figure out which type we have */
        try
        {
            if (tools.verifyPackage(pkg, registry))
            {
                auto app = AppID::AppName::from_raw({});

                if (appname.empty() || appname == "first-listed-app")
                {
                    app = tools.findAppname(pkg, ApplicationWildcard::FIRST_LISTED, registry);
                }
                else if (appname == "last-listed-app")
                {
                    app = tools.findAppname(pkg, ApplicationWildcard::LAST_LISTED, registry);
                }
                else if (appname == "only-listed-app")
                {
                    app = tools.findAppname(pkg, ApplicationWildcard::ONLY_LISTED, registry);
                }
                else
                {
                    app = AppID::AppName::from_raw(appname);
                    if (!tools.verifyAppname(pkg, app, registry))
                    {
                        throw std::runtime_error("App name passed in is not valid for this package type");
                    }
                }

                auto ver = AppID::Version::from_raw({});
                if (version.empty() || version == "current-user-version")
                {
                    ver = tools.findVersion(pkg, app, registry);
                }
                else
                {
                    ver = AppID::Version::from_raw(version);
                    if (!tools.hasAppId({pkg, app, ver}, registry))
                    {
                        throw std::runtime_error("Invalid version passed for this package type");
                    }
                }

                return AppID{pkg, app, ver};
            }
        }
        catch (std::runtime_error& e)
        {
            continue;
        }
    }

    return {};
}

AppID AppID::discover(const std::shared_ptr<Registry>& registry,
                      const std::string& package,
                      ApplicationWildcard appwildcard,
                      VersionWildcard versionwildcard)
{
    auto pkg = AppID::Package::from_raw(package);

    for (auto tools : discoverTools)
    {
        try
        {
            if (tools.verifyPackage(pkg, registry))
            {
                auto app = tools.findAppname(pkg, appwildcard, registry);
                auto ver = tools.findVersion(pkg, app, registry);
                return AppID{pkg, app, ver};
            }
        }
        catch (std::runtime_error& e)
        {
            /* Normal, try another */
            continue;
        }
    }

    return {};
}

AppID AppID::discover(const std::shared_ptr<Registry>& registry,
                      const std::string& package,
                      const std::string& appname,
                      VersionWildcard versionwildcard)
{
    auto pkg = AppID::Package::from_raw(package);
    auto app = AppID::AppName::from_raw(appname);

    for (auto tools : discoverTools)
    {
        try
        {
            if (tools.verifyPackage(pkg, registry) && tools.verifyAppname(pkg, app, registry))
            {
                auto ver = tools.findVersion(pkg, app, registry);
                return AppID{pkg, app, ver};
            }
        }
        catch (std::runtime_error& e)
        {
            /* Normal, try another */
            continue;
        }
    }

    return {};
}

AppID AppID::discover(const std::string& package, const std::string& appname, const std::string& version)
{
    auto registry = Registry::getDefault();
    return discover(registry, package, appname, version);
}

AppID AppID::discover(const std::string& package, ApplicationWildcard appwildcard, VersionWildcard versionwildcard)
{
    auto registry = Registry::getDefault();
    return discover(registry, package, appwildcard, versionwildcard);
}

AppID AppID::discover(const std::string& package, const std::string& appname, VersionWildcard versionwildcard)
{
    auto registry = Registry::getDefault();
    return discover(registry, package, appname, versionwildcard);
}

enum class oom::Score : std::int32_t
{
    FOCUSED = 100,
    UNTRUSTED_HELPER = 200,
    PAUSED = 900,
};

const oom::Score oom::focused()
{
    return oom::Score::FOCUSED;
}

const oom::Score oom::paused()
{
    return oom::Score::PAUSED;
}

const oom::Score oom::fromLabelAndValue(std::int32_t value, const std::string& label)
{
    g_debug("Creating new OOM value type '%s' with a value of: '%d'", label.c_str(), value);

    if (value < static_cast<std::int32_t>(oom::Score::FOCUSED))
    {
        g_warning("The new OOM type '%s' is giving higher priority than focused apps!", label.c_str());
    }
    if (value > static_cast<std::int32_t>(oom::Score::PAUSED))
    {
        g_warning("The new OOM type '%s' is giving lower priority than paused apps!", label.c_str());
    }

    if (value < -1000 || value > 1000)
    {
        throw std::runtime_error("OOM type '" + label + "' is not in the valid range of [-1000, 1000] at " +
                                 std::to_string(value));
    }

    return static_cast<oom::Score>(value);
}

}  // namespace app_launch
}  // namespace ubuntu
