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
#include <cstdlib>
#include <iostream>
#include <map>

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
} // anonymous namespace

template <typename T>
auto stringFromKeyfile(std::shared_ptr<GKeyFile> keyfile, const std::string& key, const std::string& exceptionText = {})
    -> T
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
auto fileFromKeyfile(std::shared_ptr<GKeyFile> keyfile,
                     const std::string basePath,
                     const std::string& key,
                     const std::string& exceptionText = {}) -> T
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
auto boolFromKeyfile(std::shared_ptr<GKeyFile> keyfile,
                     const std::string& key,
                     bool defaultReturn,
                     const std::string& exceptionText = {}) -> T
{
    GError* error = nullptr;
    auto keyval = g_key_file_get_boolean(keyfile.get(), DESKTOP_GROUP, key.c_str(), &error);

    if (error != nullptr)
    {
        auto perror = std::shared_ptr<GError>(error, g_error_free);
        if (!exceptionText.empty())
        {
            throw std::runtime_error(exceptionText + perror.get()->message);
        }

        return T::from_raw(defaultReturn);
    }

    T retval = T::from_raw(keyval == TRUE);
    return retval;
}

bool stringlistFromKeyfileContains(std::shared_ptr<GKeyFile> keyfile,
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

class IconFinder {
private:
    IconFinder(std::string basePath)
        : _searchPaths(getSearchPaths(basePath))
        , _basePath(basePath)
    {
    }
    static std::map<std::string, std::shared_ptr<IconFinder>> _instances;
public:
    static std::shared_ptr<IconFinder> fromBasePath(std::string basePath)
    {
        if (_instances.find(basePath) == _instances.end())
        {
            _instances[basePath] = std::shared_ptr<IconFinder>(new IconFinder(basePath));
        }
        return _instances[basePath];
    }

    std::string find(std::shared_ptr<GKeyFile> keyfile)
    {
        GError* error = nullptr;
        auto iconName = g_key_file_get_locale_string(keyfile.get(), DESKTOP_GROUP, "Icon", nullptr, &error);

        if (error != nullptr)
        {
            auto perror = std::shared_ptr<GError>(error, g_error_free);
            throw std::runtime_error(std::string("Missing icon for desktop file:") + perror.get()->message);
        }

        auto defaultPath = g_build_filename(_basePath.c_str(), iconName, nullptr);
        std::string iconPath = defaultPath;
        g_free(defaultPath);

        if (iconName[0] == '/') // explicit icon path received
        {
            auto retval = Application::Info::IconPath::from_raw(iconName);
            g_free(iconName);
            return retval;
        }
        else if (hasImageExtension(iconName))
        {
            if (g_file_test((_basePath + "/usr/share/pixmaps/" + iconName).c_str(), G_FILE_TEST_EXISTS))
            {
                auto retval = Application::Info::IconPath::from_raw(_basePath + "/usr/share/pixmaps/" + iconName);
                g_free(iconName);
                return retval;
            }
            g_free(iconName);
            return iconPath;
        }
        auto size = 0;
        for (const auto& path: _searchPaths)
        {
            if (path.size > size)
            {
                if (findExistingIcon(path.path, iconName, iconPath))
                {
                    size = path.size;
                }
            }
        }
        g_free(iconName);
        return iconPath;
    }

private:
    struct ThemeSubdirectory
    {
        std::string path;
        int size;
    };

    std::list<ThemeSubdirectory> _searchPaths;
    std::string _basePath;

    static bool hasImageExtension(const char* filename)
    {
        for (const auto& extension : {".png", ".svg", ".xpm"})
        {
            if (g_str_has_suffix(filename, extension))
            {
                return true;
            }
        }
        return false;
    }

    static bool findExistingIcon(std::string path, gchar* iconName, std::string &iconPath)
    {
        for (const auto& extension : {".png", ".svg", ".xpm"})
        {
            std::string name = path + iconName + extension;
            if (g_file_test(name.c_str(), G_FILE_TEST_EXISTS))
            {
                iconPath = name;
                return true;
            }
        }
        return false;
    }

    static void addSubdirectoryByType(std::shared_ptr<GKeyFile> themefile, gchar* directory, std::string themePath, std::list<ThemeSubdirectory>& subdirs)
    {
        GError* error = nullptr;
        auto type = g_key_file_get_string(themefile.get(), directory, "Type", &error);
        if (error == nullptr)
        {
            if (g_strcmp0(type, "Fixed") == 0)
            {
                auto size = g_key_file_get_integer(themefile.get(), directory, "Size", &error);
                if (error == nullptr)
                {
                    subdirs.push_back(ThemeSubdirectory{themePath + directory + "/", size});
                }
            }
            else if (g_strcmp0(type, "Scalable") == 0)
            {
                auto size = g_key_file_get_integer(themefile.get(), directory, "MaxSize", &error);
                if (error == nullptr)
                {
                    subdirs.push_back(ThemeSubdirectory{themePath + directory + "/", size});
                }
            }
            else if (g_strcmp0(type, "Threshold") == 0)
            {
                auto size = g_key_file_get_integer(themefile.get(), directory, "Size", &error);
                if (error == nullptr)
                {
                    auto threshold = g_key_file_get_integer(themefile.get(), directory, "Threshold", &error);
                    if (error != nullptr)
                    {
                        threshold = 2; // threshold defaults to 2
                    }
                    subdirs.push_back(ThemeSubdirectory{themePath + directory + "/", size + threshold});
                }
            }
            g_free(type);
        }
        g_error_free(error);
    }

    static std::list<ThemeSubdirectory> searchIconPaths(std::shared_ptr<GKeyFile> themefile, gchar** directories, std::string themePath)
    {
        std::list<ThemeSubdirectory> subdirs;
        for (auto i = 0; directories[i] != nullptr; ++i)
        {
            GError* error = nullptr;
            auto context = g_key_file_get_string(themefile.get(), directories[i], "Context", &error);
            if (error != nullptr)
            {
                g_error_free(error);
                continue;
            }
            if (g_strcmp0(context, "Applications") == 0)
            {
                addSubdirectoryByType(themefile, directories[i], themePath, subdirs);
            }
            g_free(context);
        }
        return subdirs;
    }

    static std::list<ThemeSubdirectory> getSearchPaths(const std::string& basePath)
    {
        GError* error = nullptr;
        auto themefile = std::shared_ptr<GKeyFile>(g_key_file_new(), g_key_file_free);
        g_key_file_load_from_file(themefile.get(), (basePath + "/usr/share/icons/hicolor/index.theme").c_str(), G_KEY_FILE_NONE, &error);
        if (error != nullptr)
        {
            g_error_free(error);
            return std::list<ThemeSubdirectory>();
        }

        // parse hicolor.theme
        g_key_file_set_list_separator(themefile.get(), ',');
        auto directories = g_key_file_get_string_list(themefile.get(), "Icon Theme", "Directories", nullptr, &error);
        if (error != nullptr)
        {
            g_error_free(error);
            return std::list<ThemeSubdirectory>();
        }

        auto iconPaths = searchIconPaths(themefile, directories, basePath + "/usr/share/icons/hicolor/");
        g_strfreev(directories);
        return iconPaths;
    }
};
std::map<std::string, std::shared_ptr<IconFinder>> IconFinder::_instances;


Desktop::Desktop(std::shared_ptr<GKeyFile> keyfile, const std::string& basePath)
    : _keyfile([keyfile]() {
        if (!keyfile)
        {
            throw std::runtime_error("Can not build a desktop application info object with a null keyfile");
        }
        if (stringFromKeyfile<Type>(keyfile, "Type").value() != "Application")
        {
            throw std::runtime_error("Keyfile does not represent application type");
        }
        if (boolFromKeyfile<NoDisplay>(keyfile, "NoDisplay", false).value())
        {
            throw std::runtime_error("Application is not meant to be displayed");
        }
        if (boolFromKeyfile<Hidden>(keyfile, "Hidden", false).value())
        {
            throw std::runtime_error("Application keyfile is hidden");
        }
        auto xdg_current_desktop = getenv("XDG_CURRENT_DESKTOP");
        if (stringlistFromKeyfileContains(keyfile, "NotShowIn", xdg_current_desktop, false)
                 || !stringlistFromKeyfileContains(keyfile, "OnlyShowIn", xdg_current_desktop, true))
        {
            throw std::runtime_error("Application is not shown in Unity");
        }

        return keyfile;
    }())
    , _basePath(basePath)
    , _name(stringFromKeyfile<Application::Info::Name>(keyfile, "Name", "Unable to get name from keyfile"))
    , _description(stringFromKeyfile<Application::Info::Description>(keyfile, "Comment"))
    , _iconPath(Application::Info::IconPath::from_raw(IconFinder::fromBasePath(basePath)->find(keyfile)))
    , _splashInfo({
        title : stringFromKeyfile<Application::Info::Splash::Title>(keyfile, "X-Ubuntu-Splash-Title"),
        image : fileFromKeyfile<Application::Info::Splash::Image>(keyfile, basePath, "X-Ubuntu-Splash-Image"),
        backgroundColor : stringFromKeyfile<Application::Info::Splash::Color>(keyfile, "X-Ubuntu-Splash-Color"),
        headerColor : stringFromKeyfile<Application::Info::Splash::Color>(keyfile, "X-Ubuntu-Splash-Color-Header"),
        footerColor : stringFromKeyfile<Application::Info::Splash::Color>(keyfile, "X-Ubuntu-Splash-Color-Footer"),
        showHeader :
            boolFromKeyfile<Application::Info::Splash::ShowHeader>(keyfile, "X-Ubuntu-Splash-Show-Header", false)
    })
    , _supportedOrientations([keyfile]() {
        Orientations all = {portrait : true, landscape : true, invertedPortrait : true, invertedLandscape : true};

        GError* error = nullptr;
        auto orientationStrv = g_key_file_get_string_list(keyfile.get(), DESKTOP_GROUP,
                                                          "X-Ubuntu-Supported-Orientations", nullptr, &error);

        if (error != nullptr)
        {
            g_error_free(error);
            return all;
        }

        Orientations retval =
            {portrait : false, landscape : false, invertedPortrait : false, invertedLandscape : false};

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
          boolFromKeyfile<Application::Info::RotatesWindow>(keyfile, "X-Ubuntu-Rotates-Window-Content", false))
    , _ubuntuLifecycle(boolFromKeyfile<Application::Info::UbuntuLifecycle>(keyfile, "X-Ubuntu-Touch", false))
{
}

};  // namespace app_info
};  // namespace app_launch
};  // namespace ubuntu
