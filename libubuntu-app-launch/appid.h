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

/** \brief The set of information that is used to uniquely identify an
           application in Ubuntu.

    Application ID's are derived from the packaging system and the applications
    that are defined to work in it. It resolves down to a specific version of
    the package to resolve problems with upgrades and reduce race conditions that
    come from installing and removing them while trying to launch them. While
    it always resolves down to a specific version, there are functions avilable
    here that search in various ways for the current version so higher level apps
    can save just the package and application strings and discover the version
    when it is required.
*/
struct AppID
{
    /** \private */
    struct PackageTag;
    /** \private */
    struct AppNameTag;
    /** \private */
    struct VersionTag;

    /** \private */
    typedef TypeTagger<PackageTag, std::string> Package;
    /** \private */
    typedef TypeTagger<AppNameTag, std::string> AppName;
    /** \private */
    typedef TypeTagger<VersionTag, std::string> Version;

    /** The package name of the application. Typically this is in the form of
        $app.$developer so it could be my-app.my-name, though other formats do
        exist and are used in the wild.

        In the case of legacy applications this will be the empty string. */
    Package package;
    /** The string that uniquely identifies the application. This comes from
        the package manifest. In a Click package this is the string that exists
        under the "hooks" key in the JSON manifest. */
    AppName appname;
    /** Version of the package that is installed. This is always resolved when
        creating the struct. */
    Version version;

    /** Turn the structure into a string. This is required for many older C based
        interfaces that work with AppID's, but is generally not recommended for
        anyting other than debug messages. */
    operator std::string() const;

    /** Empty constructor for an AppID. Makes coding with them easier, but generally
        there is nothing useful about an empty AppID. */
    AppID();
    /** Checks to see if an AppID is empty. */
    bool empty() const;

    /** Constructor for an AppID if all the information is known about the package.
        Provides a precise and fast way to create an AppID if all the information
        is already known.

        \param package Name of the package
        \param appname Name of the application
        \param version Version of the package
    */
    AppID(Package pkg, AppName app, Version ver);

    /** Parse a string and turn it into an AppID. This assumes that the string is
        in the form: $(package)_$(app)_$(version) and will return an empty AppID
        if not.

        \param appid String with the concatenated AppID
    */
    static AppID parse(const std::string& appid);
    /** Find is a more tollerant version of parse(), it handles legacy applications,
        short AppIDs ($package_$app) and other forms of that are in common usage.
        It can be used, but is slower than parse() if you've got well formed data
        already.

        \param sappid String with the concatenated AppID
    */
    static AppID find(const std::string& sappid);
    /** Check to see whether a string is a valid AppID string

        \param sappid String with the concatenated AppID
    */
    static bool valid(const std::string& sappid);

    /** Control how the application list of a package is searched in the discover()
        functions. */
    enum class ApplicationWildcard
    {
        FIRST_LISTED, /**< First application listed in the manifest */
        LAST_LISTED,  /**< Last application listed in the manifest */
        ONLY_LISTED,  /**< Only application listed in the manifest */
    };
    /** Control how the versions are searched in the discover() set of functions */
    enum class VersionWildcard
    {
        CURRENT_USER_VERSION, /**< The current installed version */
    };

    /** Find the AppID for an application where you only know the package
        name.

        \param package Name of the package
        \param appwildcard Specification of how to search the manifest for apps
        \param versionwildcard Specification of how to search for the version
    */
    static AppID discover(const std::string& package,
                          ApplicationWildcard appwildcard = ApplicationWildcard::FIRST_LISTED,
                          VersionWildcard versionwildcard = VersionWildcard::CURRENT_USER_VERSION);
    /** Find the AppID for an application where you know the package
        name and application name.

        \param package Name of the package
        \param appname Name of the application
        \param versionwildcard Specification of how to search for the version
    */
    static AppID discover(const std::string& package,
                          const std::string& appname,
                          VersionWildcard versionwildcard = VersionWildcard::CURRENT_USER_VERSION);
    /** Create an AppID providing known strings of packages and names

        \param package Name of the package
        \param appname Name of the application
        \param version Version of the package
    */
    static AppID discover(const std::string& package, const std::string& appname, const std::string& version);
};

bool operator==(const AppID& a, const AppID& b);
bool operator!=(const AppID& a, const AppID& b);

};  // namespace app_launch
};  // namespace ubuntu

#pragma GCC visibility pop
