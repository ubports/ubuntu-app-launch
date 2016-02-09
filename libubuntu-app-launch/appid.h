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

#include <string>

#include "type-tagger.h"

#pragma once
#pragma GCC visibility push(default)

namespace ubuntu
{
namespace app_launch
{

struct AppID
{
    struct PackageTag;
    struct AppNameTag;
    struct VersionTag;

    typedef TypeTagger<PackageTag, std::string> Package;
    typedef TypeTagger<AppNameTag, std::string> AppName;
    typedef TypeTagger<VersionTag, std::string> Version;

    Package package;
    AppName appname;
    Version version;

    operator std::string() const;
    int operator==(const AppID& other) const;
    int operator!=(const AppID& other) const;

    AppID();
    AppID(Package pkg, AppName app, Version ver);
    bool empty() const;

    static AppID parse(const std::string& appid);

    enum ApplicationWildcard
    {
        FIRST_LISTED,
        LAST_LISTED,
        ONLY_LISTED
    };
    enum VersionWildcard
    {
        CURRENT_USER_VERSION
    };

    static AppID discover(const std::string& package,
                          ApplicationWildcard appwildcard = ApplicationWildcard::FIRST_LISTED,
                          VersionWildcard versionwildcard = VersionWildcard::CURRENT_USER_VERSION);
    static AppID discover(const std::string& package,
                          const std::string& appname,
                          VersionWildcard versionwildcard = VersionWildcard::CURRENT_USER_VERSION);
    static AppID discover(const std::string& package, const std::string& appname, const std::string& version);

    static AppID find(const std::string& sappid);
    static bool valid(const std::string& sappid);
};

};  // namespace app_launch
};  // namespace ubuntu

#pragma GCC visibility pop
