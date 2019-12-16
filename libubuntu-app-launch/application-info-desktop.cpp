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
#include <cstdlib>

namespace ubuntu
{
namespace app_launch
{
namespace app_info
{
namespace
{
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
    auto keyval = g_key_file_get_locale_string(keyfile.get(), DESKTOP_GROUP, key.c_str(), nullptr, &error);

    if (error != nullptr)
    {
        auto perror = std::shared_ptr<GError>(error, g_error_free);
        if (!exceptionText.empty())
        {
            throw std::runtime_error(exceptionText + perror.get()->message);
        }

        return T::from_raw({});
    }

    T retval = T::from_raw(keyval);
    g_free(keyval);
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
    auto keyval = g_key_file_get_locale_string(keyfile.get(), DESKTOP_GROUP, key.c_str(), nullptr, &error);

    if (error != nullptr)
    {
        auto perror = std::shared_ptr<GError>(error, g_error_free);
        if (!exceptionText.empty())
        {
            throw std::runtime_error(exceptionText + perror.get()->message);
        }

        return T::from_raw({});
    }

    /* If we're already an absolute path, don't prepend the base path */
    if (keyval[0] == '/')
    {
        T retval = T::from_raw(keyval);

        if (!rootDir.empty())
        {
            auto fullpath = g_build_filename(rootDir.c_str(), keyval, nullptr);
            retval = T::from_raw(fullpath);
            g_free(fullpath);
        }

        g_free(keyval);
        return retval;
    }

    auto cpath = g_build_filename(basePath.c_str(), keyval, nullptr);

    T retval = T::from_raw(cpath);

    g_free(keyval);
    g_free(cpath);

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
        auto perror = std::shared_ptr<GError>(error, g_error_free);
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
    auto keyval = g_key_file_get_locale_string_list(keyfile.get(), DESKTOP_GROUP, key, nullptr, nullptr, &error);

    if (error != nullptr)
    {
        auto perror = std::shared_ptr<GError>(error, g_error_free);
        if (!exceptionText.empty())
        {
            throw std::runtime_error(exceptionText + perror.get()->message);
        }

        return T::from_raw({});
    }

    std::vector<std::string> results;
    for (auto i = 0; keyval[i] != nullptr; ++i)
    {
        if (strlen(keyval[i]) != 0)
        {
            results.emplace_back(keyval[i]);
        }
    }
    g_strfreev(keyval);

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
    auto results = g_key_file_get_string_list(keyfile.get(), DESKTOP_GROUP, key, nullptr, &error);
    if (error != nullptr)
    {
        g_error_free(error);
        return defaultValue;
    }

    bool result = false;
    for (auto i = 0; results[i] != nullptr; ++i)
    {
        if (results[i] == match)
        {
            result = true;
            break;
        }
    }
    g_strfreev(results);

    return result;
}

Desktop::Desktop(const std::shared_ptr<GKeyFile>& keyfile,
                 const std::string& basePath,
                 const std::string& rootDir,
                 std::bitset<2> flags,
                 std::shared_ptr<Registry> registry)
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
            if (stringlistFromKeyfileContains(keyfile, "NotShowIn", xdg_current_desktop, false) ||
                !stringlistFromKeyfileContains(keyfile, "OnlyShowIn", xdg_current_desktop, true))
            {
                g_warning("Application is not shown in Unity");
                // Exception removed for OTA11 as a temporary fix
                // throw std::runtime_error("Application is not shown in Unity");
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
                return registry->impl->getIconFinder(basePath)->find(iconName);
            }
        }
        auto iconPath = fileFromKeyfile<Application::Info::IconPath>(keyfile, basePath, rootDir, "Icon");
        if (!g_file_test(iconPath.value().c_str(), G_FILE_TEST_EXISTS)) {
            static const std::vector<std::string> extensions { ".svg", ".png" };
            for (const auto extension: extensions) {
                std::string testIconPath = iconPath.value() + extension;
                if (g_file_test(testIconPath.c_str(), G_FILE_TEST_EXISTS)) {
                    iconPath = Application::Info::IconPath::from_raw(testIconPath);
                    break;
                }
            }
        }
        return iconPath;
    }())
    , _defaultDepartment(
          stringFromKeyfile<Application::Info::DefaultDepartment>(keyfile, "X-Ubuntu-Default-Department-ID"))
    , _screenshotPath([keyfile, basePath, rootDir]() {
        return fileFromKeyfile<Application::Info::IconPath>(keyfile, basePath, rootDir, "X-Screenshot");
    }())
    , _keywords(stringlistFromKeyfile<Application::Info::Keywords>(keyfile, "Keywords"))
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
        auto orientationStrv = g_key_file_get_string_list(keyfile.get(), DESKTOP_GROUP,
                                                          "X-Ubuntu-Supported-Orientations", nullptr, &error);

        if (error != nullptr)
        {
            g_error_free(error);
            return all;
        }

        Orientations retval = {false, false, false, false};

        try
        {
            for (auto i = 0; orientationStrv[i] != nullptr; i++)
            {
                g_strstrip(orientationStrv[i]); /* remove whitespace */

                if (g_ascii_strcasecmp("portrait", orientationStrv[i]) == 0)
                {
                    retval.portrait = true;
                }
                else if (g_ascii_strcasecmp("landscape", orientationStrv[i]) == 0)
                {
                    retval.landscape = true;
                }
                else if (g_ascii_strcasecmp("invertedPortrait", orientationStrv[i]) == 0)
                {
                    retval.invertedPortrait = true;
                }
                else if (g_ascii_strcasecmp("invertedLandscape", orientationStrv[i]) == 0)
                {
                    retval.invertedLandscape = true;
                }
                else if (g_ascii_strcasecmp("primary", orientationStrv[i]) == 0 && i == 0)
                {
                    /* Pass, we'll let primary be the first entry, it should be the only. */
                }
                else
                {
                    throw std::runtime_error("Invalid orientation string '" + std::string(orientationStrv[i]) + "'");
                }
            }
        }
        catch (...)
        {
            retval = all;
        }

        g_strfreev(orientationStrv);
        return retval;
    }())
    , _rotatesWindow(
          boolFromKeyfile<Application::Info::RotatesWindow>(keyfile, "X-Ubuntu-Rotates-Window-Contents", false))
    , _ubuntuLifecycle(boolFromKeyfile<Application::Info::UbuntuLifecycle>(keyfile, "X-Ubuntu-Touch", false))
    , _xMirEnable(
          boolFromKeyfile<XMirEnable>(keyfile, "X-Ubuntu-XMir-Enable", (flags & DesktopFlags::XMIR_DEFAULT).any()))
    , _exec(stringFromKeyfile<Exec>(keyfile, "Exec"))
{
}

}  // namespace app_info
}  // namespace app_launch
}  // namespace ubuntu
