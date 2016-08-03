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
#include "app-info.h"
#include "ubuntu-app-launch.h"
}

#include "application-impl-click.h"
#include "application-impl-legacy.h"
#include "application-impl-libertine.h"
#include "application.h"

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

    std::string sappid = appid;
    if (app_info_click(sappid.c_str(), NULL, NULL))
    {
        return std::make_shared<app_impls::Click>(appid, registry);
    }
    else if (app_info_libertine(sappid.c_str(), NULL, NULL))
    {
        return std::make_shared<app_impls::Libertine>(appid.package, appid.appname, registry);
    }
    else if (app_info_legacy(sappid.c_str(), NULL, NULL))
    {
        return std::make_shared<app_impls::Legacy>(appid.appname, registry);
    }
    else
    {
        throw std::runtime_error("Invalid app ID: " + sappid);
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
    std::smatch match;

    if (std::regex_match(sappid, match, full_appid_regex))
    {
        return {AppID::Package::from_raw(match[1].str()), AppID::AppName::from_raw(match[2].str()),
                AppID::Version::from_raw(match[3].str())};
    }
    else if (std::regex_match(sappid, match, short_appid_regex))
    {
        return discover(match[1].str(), match[2].str());
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

bool AppID::empty() const
{
    return package.value().empty() && appname.value().empty() && version.value().empty();
}

std::string app_wildcard(AppID::ApplicationWildcard card)
{
    switch (card)
    {
        case AppID::ApplicationWildcard::FIRST_LISTED:
            return "first-listed-app";
        case AppID::ApplicationWildcard::LAST_LISTED:
            return "last-listed-app";
        case AppID::ApplicationWildcard::ONLY_LISTED:
            return "only-listed-app";
    }

    return "";
}

std::string ver_wildcard(AppID::VersionWildcard card)
{
    switch (card)
    {
        case AppID::VersionWildcard::CURRENT_USER_VERSION:
            return "current-user-version";
    }

    return "";
}

AppID AppID::discover(const std::string& package, const std::string& appname, const std::string& version)
{
    auto cappid = ubuntu_app_launch_triplet_to_app_id(package.c_str(), appname.c_str(), version.c_str());

    auto appid = cappid != nullptr ? AppID::parse(cappid) : AppID::parse("");

    g_free(cappid);

    return appid;
}

AppID AppID::discover(const std::string& package, ApplicationWildcard appwildcard, VersionWildcard versionwildcard)
{
    return discover(package, app_wildcard(appwildcard), ver_wildcard(versionwildcard));
}

AppID AppID::discover(const std::string& package, const std::string& appname, VersionWildcard versionwildcard)
{
    auto appid = discover(package, appname, ver_wildcard(versionwildcard));

    if (appid.empty())
    {
        /* If we weren't able to go that route, we can see if it's libertine */
        if (app_info_libertine((package + "_" + appname + "_0.0").c_str(), nullptr, nullptr))
        {
            appid = AppID(Package::from_raw(package), AppName::from_raw(appname), Version::from_raw("0.0"));
        }
    }

    return appid;
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

};  // namespace app_launch
};  // namespace ubuntu
