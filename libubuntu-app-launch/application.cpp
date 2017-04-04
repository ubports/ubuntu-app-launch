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

#include "application.h"
#include "info-watcher.h"
#include "jobs-base.h"
#include "registry-impl.h"
#include "registry.h"

#include <functional>
#include <iomanip>
#include <iostream>
#include <locale>
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

    if (!registry || !registry->impl)
    {
        throw std::runtime_error("Invalid registry object");
    }

    return registry->impl->createApp(appid);
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
    return registry->impl->find(sappid);
}

AppID AppID::find(const std::shared_ptr<Registry>& registry, const std::string& sappid)
{
    return registry->impl->find(sappid);
}

AppID Registry::Impl::find(const std::string& sappid)
{
    std::smatch match;

    if (std::regex_match(sappid, match, full_appid_regex))
    {
        return {AppID::Package::from_raw(match[1].str()), AppID::AppName::from_raw(match[2].str()),
                AppID::Version::from_raw(match[3].str())};
    }
    else if (std::regex_match(sappid, match, short_appid_regex))
    {
        return discover(match[1].str(), match[2].str(), AppID::VersionWildcard::CURRENT_USER_VERSION);
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

std::string AppID::persistentID() const
{
    if (package.value().empty())
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

    return package.value() + "_" + appname.value();
}

std::string AppID::dbusID() const
{
    std::string bytes = operator std::string();
    std::string encoded;

    for (size_t i = 0; i < bytes.size(); ++i) {
        char chr = bytes[i];

        if (std::isalpha(chr, std::locale::classic()) ||
            (std::isdigit(chr, std::locale::classic()) && i != 0)) {
            encoded += chr;
        } else {
            std::ostringstream hex;
            hex << std::setw(2) << std::setfill('0') << std::hex;
            hex << int(chr);
            encoded += '_' + hex.str();
        }
    }

    return encoded;
}

AppID AppID::parseDBusID(const std::string& dbusid)
{
    std::string decoded;

    for (size_t i = 0; i < dbusid.size(); ++i) {
        char chr = dbusid[i];

        if (chr == '_' && i + 2 < dbusid.size()) {
            int result;
            std::istringstream hex(dbusid.substr(i + 1, 2));
            hex >> std::hex >> result;
            decoded += (char)result;
            i += 2;
        } else {
            decoded += chr;
        }
    }

    return parse(decoded);
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

/** Convert each AppID to a string and then compare the strings */
bool operator<(const AppID& a, const AppID& b)
{
    return std::string(a) < std::string(b);
}

bool AppID::empty() const
{
    return package.value().empty() && appname.value().empty() && version.value().empty();
}

AppID AppID::discover(const std::shared_ptr<Registry>& registry,
                      const std::string& package,
                      const std::string& appname,
                      const std::string& version)
{
    return registry->impl->discover(package, appname, version);
}

AppID Registry::Impl::discover(const std::string& package, const std::string& appname, const std::string& version)
{
    auto pkg = AppID::Package::from_raw(package);

    for (const auto& appStore : appStores())
    {
        /* Figure out which type we have */
        try
        {
            if (appStore->verifyPackage(pkg))
            {
                auto app = AppID::AppName::from_raw({});

                if (appname.empty() || appname == "first-listed-app")
                {
                    app = appStore->findAppname(pkg, AppID::ApplicationWildcard::FIRST_LISTED);
                }
                else if (appname == "last-listed-app")
                {
                    app = appStore->findAppname(pkg, AppID::ApplicationWildcard::LAST_LISTED);
                }
                else if (appname == "only-listed-app")
                {
                    app = appStore->findAppname(pkg, AppID::ApplicationWildcard::ONLY_LISTED);
                }
                else
                {
                    app = AppID::AppName::from_raw(appname);
                    if (!appStore->verifyAppname(pkg, app))
                    {
                        throw std::runtime_error("App name passed in is not valid for this package type");
                    }
                }

                auto ver = AppID::Version::from_raw({});
                if (version.empty() || version == "current-user-version")
                {
                    ver = appStore->findVersion(pkg, app);
                }
                else
                {
                    ver = AppID::Version::from_raw(version);
                    if (!appStore->hasAppId({pkg, app, ver}))
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
    return registry->impl->discover(package, appwildcard, versionwildcard);
}

AppID Registry::Impl::discover(const std::string& package,
                               AppID::ApplicationWildcard appwildcard,
                               AppID::VersionWildcard versionwildcard)
{
    auto pkg = AppID::Package::from_raw(package);

    for (const auto& appStore : appStores())
    {
        try
        {
            if (appStore->verifyPackage(pkg))
            {
                auto app = appStore->findAppname(pkg, appwildcard);
                auto ver = appStore->findVersion(pkg, app);
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
    return registry->impl->discover(package, appname, versionwildcard);
}

AppID Registry::Impl::discover(const std::string& package,
                               const std::string& appname,
                               AppID::VersionWildcard versionwildcard)
{
    auto pkg = AppID::Package::from_raw(package);
    auto app = AppID::AppName::from_raw(appname);

    for (const auto& appStore : appStores())
    {
        try
        {
            if (appStore->verifyPackage(pkg) && appStore->verifyAppname(pkg, app))
            {
                auto ver = appStore->findVersion(pkg, app);
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
    return registry->impl->discover(package, appname, version);
}

AppID AppID::discover(const std::string& package, ApplicationWildcard appwildcard, VersionWildcard versionwildcard)
{
    auto registry = Registry::getDefault();
    return registry->impl->discover(package, appwildcard, versionwildcard);
}

AppID AppID::discover(const std::string& package, const std::string& appname, VersionWildcard versionwildcard)
{
    auto registry = Registry::getDefault();
    return registry->impl->discover(package, appname, versionwildcard);
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
