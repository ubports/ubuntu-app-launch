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

#include "application-info-desktop.h"
#include "application-icon-finder.h"
#include "registry-impl.h"
#include "string-util.h"
#include <algorithm>
#include <cstdlib>
#include <unity/util/GlibMemory.h>

using namespace unity::util;

namespace ubuntu
{
namespace app_launch
{
namespace app_info
{
namespace
{

static std::set<std::string> strvToSet(gchar** strv)
{
    std::set<std::string> retval;

    if (strv != nullptr)
    {
        for (int i = 0; strv[i] != nullptr; i++)
        {
            if (strv[i][0] != '\0')
            {
                retval.emplace(strv[i]);
            }
        }
    }

    return retval;
}

constexpr const char* DESKTOP_GROUP = "Desktop Entry";

struct TypeTag;
typedef TypeTagger<TypeTag, std::string> Type;

struct HiddenTag;
typedef TypeTagger<HiddenTag, bool> Hidden;

struct NoDisplayTag;
typedef TypeTagger<NoDisplayTag, bool> NoDisplay;
}  // anonymous namespace

template <typename T>
auto stringFromKeyfileRequired(const std::shared_ptr<GKeyFile>& keyfile,
                               const std::string& key,
                               const std::string& exceptionText) -> T
{
    GError* error = nullptr;
    auto keyval =
        unique_gchar(g_key_file_get_locale_string(keyfile.get(), DESKTOP_GROUP, key.c_str(), nullptr, &error));

    if (error != nullptr)
    {
        auto perror = unique_glib(error);
        error = nullptr;
        if (!exceptionText.empty())
        {
            throw std::runtime_error(exceptionText + perror.get()->message);
        }

        return T::from_raw({});
    }

    T retval = T::from_raw(keyval.get());
    return retval;
}

template <typename T>
auto stringFromKeyfile(const std::shared_ptr<GKeyFile>& keyfile, const std::string& key) -> T
{
    return stringFromKeyfileRequired<T>(keyfile, key, {});
}

template <typename T>
auto fileFromKeyfileRequired(const std::shared_ptr<GKeyFile>& keyfile,
                             const std::string& basePath,
                             const std::string& rootDir,
                             const std::string& key,
                             const std::string& exceptionText) -> T
{
    GError* error = nullptr;
    auto keyval =
        unique_gchar(g_key_file_get_locale_string(keyfile.get(), DESKTOP_GROUP, key.c_str(), nullptr, &error));

    if (error != nullptr)
    {
        auto perror = unique_glib(error);
        if (!exceptionText.empty())
        {
            throw std::runtime_error(exceptionText + perror.get()->message);
        }

        return T::from_raw({});
    }

    /* If we're already an absolute path, don't prepend the base path */
    if (keyval.get()[0] == '/')
    {
        T retval = T::from_raw(keyval.get());

        if (!rootDir.empty())
        {
            auto fullpath = unique_gchar(g_build_filename(rootDir.c_str(), keyval.get(), nullptr));
            retval = T::from_raw(fullpath.get());
        }

        return retval;
    }

    auto cpath = unique_gchar(g_build_filename(basePath.c_str(), keyval.get(), nullptr));

    T retval = T::from_raw(cpath.get());

    return retval;
}

template <typename T>
auto fileFromKeyfile(const std::shared_ptr<GKeyFile>& keyfile,
                     const std::string& basePath,
                     const std::string& rootDir,
                     const std::string& key) -> T
{
    return fileFromKeyfileRequired<T>(keyfile, basePath, rootDir, key, {});
}

template <typename T>
auto boolFromKeyfileRequired(const std::shared_ptr<GKeyFile>& keyfile,
                             const std::string& key,
                             const std::string& exceptionText) -> T
{
    GError* error = nullptr;
    auto keyval = g_key_file_get_boolean(keyfile.get(), DESKTOP_GROUP, key.c_str(), &error);

    if (error != nullptr)
    {
        auto perror = unique_glib(error);
        error = nullptr;
        throw std::runtime_error(exceptionText + perror.get()->message);
    }

    T retval = T::from_raw(keyval == TRUE);
    return retval;
}

template <typename T>
auto boolFromKeyfile(const std::shared_ptr<GKeyFile>& keyfile, const std::string& key, bool defaultReturn) -> T
{
    try
    {
        return boolFromKeyfileRequired<T>(keyfile, key, "Boolean value not available, but not required");
    }
    catch (std::runtime_error& e)
    {
        return T::from_raw(defaultReturn);
    }
}

template <typename T>
auto stringlistFromKeyfileRequired(const std::shared_ptr<GKeyFile>& keyfile,
                                   const gchar* key,
                                   const std::string& exceptionText) -> T
{
    GError* error = nullptr;
    auto keyval =
        unique_gcharv(g_key_file_get_locale_string_list(keyfile.get(), DESKTOP_GROUP, key, nullptr, nullptr, &error));

    if (error != nullptr)
    {
        auto perror = unique_glib(error);
        error = nullptr;
        if (!exceptionText.empty())
        {
            throw std::runtime_error(exceptionText + perror.get()->message);
        }

        return T::from_raw({});
    }

    std::vector<std::string> results;
    for (auto i = 0; keyval.get()[i] != nullptr; ++i)
    {
        if (strlen(keyval.get()[i]) != 0)
        {
            results.emplace_back(keyval.get()[i]);
        }
    }

    return T::from_raw(results);
}

template <typename T>
auto stringlistFromKeyfile(const std::shared_ptr<GKeyFile>& keyfile, const gchar* key) -> T
{
    return stringlistFromKeyfileRequired<T>(keyfile, key, {});
}

bool stringlistFromKeyfileContains(const std::shared_ptr<GKeyFile>& keyfile,
                                   const gchar* key,
                                   const std::string& match,
                                   bool defaultValue)
{
    GError* error = nullptr;
    auto results = unique_gcharv(g_key_file_get_string_list(keyfile.get(), DESKTOP_GROUP, key, nullptr, &error));
    if (error != nullptr)
    {
        g_error_free(error);
        return defaultValue;
    }

    bool result = false;
    for (auto i = 0; results.get()[i] != nullptr; ++i)
    {
        if (results.get()[i] == match)
        {
            result = true;
            break;
        }
    }

    return result;
}

std::set<std::string> stringlistFromKeyfileSet(const std::shared_ptr<GKeyFile>& keyfile, const gchar* key)
{
    GError* error = nullptr;
    auto results = unique_gcharv(g_key_file_get_string_list(keyfile.get(), DESKTOP_GROUP, key, nullptr, &error));
    if (error != nullptr)
    {
        g_error_free(error);
        return {};
    }

    auto retval = strvToSet(results.get());
    return retval;
}

Desktop::Desktop(const AppID& appid,
                 const std::shared_ptr<GKeyFile>& keyfile,
                 const std::string& basePath,
                 const std::string& rootDir,
                 std::bitset<2> flags,
                 const std::shared_ptr<Registry::Impl>& registry)
    : _keyfile([keyfile, flags]() {
        if (!keyfile)
        {
            throw std::runtime_error("Can not build a desktop application info object with a null keyfile");
        }
        if (stringFromKeyfile<Type>(keyfile, "Type").value() != "Application")
        {
            throw std::runtime_error("Keyfile does not represent application type");
        }
        if (boolFromKeyfile<NoDisplay>(keyfile, "NoDisplay", false).value() &&
            (flags & DesktopFlags::ALLOW_NO_DISPLAY).none())
        {
            throw std::runtime_error("Application is not meant to be displayed");
        }
        if (boolFromKeyfile<Hidden>(keyfile, "Hidden", false).value())
        {
            throw std::runtime_error("Application keyfile is hidden");
        }
        auto xdg_current_desktop = getenv("XDG_CURRENT_DESKTOP");
        if (xdg_current_desktop != nullptr)
        {
            /* Split the CURRENT_DESKTOP by colons if there are multiple */
            auto current_desktops = unique_gcharv(g_strsplit(xdg_current_desktop, ":", -1));
            auto cdesktops = strvToSet(current_desktops.get());

            auto onlyshowin = stringlistFromKeyfileSet(keyfile, "OnlyShowIn");
            auto noshowin = stringlistFromKeyfileSet(keyfile, "NotShowIn");

            auto hasAnyOf = [](std::set<std::string>& a, std::set<std::string>& b) {
                return std::find_first_of(a.cbegin(), a.cend(), b.cbegin(), b.cend()) != a.cend();
            };

            if ((!hasAnyOf(onlyshowin, cdesktops) && !onlyshowin.empty()) || hasAnyOf(noshowin, cdesktops))
            {
                throw std::runtime_error("Application is not shown in '" + std::string{xdg_current_desktop} + "'");
            }
        }

        return keyfile;
    }())
    , _basePath(basePath)
    , _rootDir(rootDir)
    , _name(stringFromKeyfileRequired<Application::Info::Name>(keyfile, "Name", "Unable to get name from keyfile"))
    , _description(stringFromKeyfile<Application::Info::Description>(keyfile, "Comment"))
    , _iconPath([keyfile, basePath, rootDir, registry]() {
        if (registry != nullptr)
        {
            auto iconName = stringFromKeyfile<Application::Info::IconPath>(keyfile, "Icon");

            if (!iconName.value().empty() && iconName.value()[0] != '/')
            {
                /* If it is not a direct filename look it up */
                return registry->getIconFinder(basePath)->find(iconName);
            }
        }
        return fileFromKeyfile<Application::Info::IconPath>(keyfile, basePath, rootDir, "Icon");
    }())
    , _defaultDepartment(
          stringFromKeyfile<Application::Info::DefaultDepartment>(keyfile, "X-Ubuntu-Default-Department-ID"))
    , _screenshotPath([keyfile, basePath, rootDir]() {
        return fileFromKeyfile<Application::Info::IconPath>(keyfile, basePath, rootDir, "X-Screenshot");
    }())
    , _keywords(stringlistFromKeyfile<Application::Info::Keywords>(keyfile, "Keywords"))
    , _popularity([registry, appid] {
        if (registry)
            return registry->getZgWatcher()->lookupAppPopularity(appid);
        else
            return Application::Info::Popularity::from_raw(0);
    }())
    , _splashInfo(
          {stringFromKeyfile<Application::Info::Splash::Title>(keyfile, "X-Ubuntu-Splash-Title"),
           fileFromKeyfile<Application::Info::Splash::Image>(keyfile, basePath, rootDir, "X-Ubuntu-Splash-Image"),
           stringFromKeyfile<Application::Info::Splash::Color>(keyfile, "X-Ubuntu-Splash-Color"),
           stringFromKeyfile<Application::Info::Splash::Color>(keyfile, "X-Ubuntu-Splash-Color-Header"),
           stringFromKeyfile<Application::Info::Splash::Color>(keyfile, "X-Ubuntu-Splash-Color-Footer"),
           boolFromKeyfile<Application::Info::Splash::ShowHeader>(keyfile, "X-Ubuntu-Splash-Show-Header", false)})
    , _supportedOrientations([keyfile]() {
        Orientations all = {true, true, true, true};

        GError* error = nullptr;
        auto orientationStrv = unique_gcharv(g_key_file_get_string_list(
            keyfile.get(), DESKTOP_GROUP, "X-Ubuntu-Supported-Orientations", nullptr, &error));

        if (error != nullptr)
        {
            g_error_free(error);
            return all;
        }

        Orientations retval = {false, false, false, false};

        try
        {
            for (auto i = 0; orientationStrv.get()[i] != nullptr; i++)
            {
                auto item = orientationStrv.get()[i];
                g_strstrip(item); /* remove whitespace */

                if (g_ascii_strcasecmp("portrait", item) == 0)
                {
                    retval.portrait = true;
                }
                else if (g_ascii_strcasecmp("landscape", item) == 0)
                {
                    retval.landscape = true;
                }
                else if (g_ascii_strcasecmp("invertedPortrait", item) == 0)
                {
                    retval.invertedPortrait = true;
                }
                else if (g_ascii_strcasecmp("invertedLandscape", item) == 0)
                {
                    retval.invertedLandscape = true;
                }
                else if (g_ascii_strcasecmp("primary", item) == 0 && i == 0)
                {
                    /* Pass, we'll let primary be the first entry, it should be the only. */
                }
                else
                {
                    throw std::runtime_error("Invalid orientation string '" + std::string(item) + "'");
                }
            }
        }
        catch (...)
        {
            retval = all;
        }

        return retval;
    }())
    , _rotatesWindow(
          boolFromKeyfile<Application::Info::RotatesWindow>(keyfile, "X-Ubuntu-Rotates-Window-Contents", false))
    , _ubuntuLifecycle(boolFromKeyfile<Application::Info::UbuntuLifecycle>(keyfile, "X-Ubuntu-Touch", false))
    , _xMirEnable(false)
    , _exec(stringFromKeyfile<Exec>(keyfile, "Exec"))
    , _singleInstance(boolFromKeyfile<SingleInstance>(keyfile, "X-Ubuntu-Single-Instance", false))
{
}

}  // namespace app_info
}  // namespace app_launch
}  // namespace ubuntu
