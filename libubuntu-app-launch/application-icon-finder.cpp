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
 *     Larry Price <larry.price@canonical.com>
 */

#include "application-icon-finder.h"

namespace ubuntu
{
namespace app_launch
{
namespace
{
constexpr auto HICOLOR_THEME_DIR = "/usr/share/icons/hicolor/";
constexpr auto HICOLOR_THEME_FILE = "/usr/share/icons/hicolor/index.theme";
constexpr auto LOCAL_HICOLOR_THEME_DIR = "/.local/share/icons/hicolor/";
constexpr auto HOME_DIR = "/home/";
constexpr auto APPLICATIONS_TYPE = "Applications";
constexpr auto SIZE_PROPERTY = "Size";
constexpr auto MAXSIZE_PROPERTY = "MaxSize";
constexpr auto THRESHOLD_PROPERTY = "Threshold";
constexpr auto FIXED_CONTEXT = "Fixed";
constexpr auto SCALABLE_CONTEXT = "Scalable";
constexpr auto THRESHOLD_CONTEXT = "Threshold";
constexpr auto CONTEXT_PROPERTY = "Context";
constexpr auto TYPE_PROPERTY = "Type";
constexpr auto DIRECTORIES_PROPERTY = "Directories";
constexpr auto ICON_THEME_KEY = "Icon Theme";
constexpr auto PIXMAPS_PATH = "/usr/share/pixmaps/";
constexpr auto ICON_TYPES = {".png", ".svg", ".xpm"};
}  // anonymous namespace

IconFinder::IconFinder(std::string basePath)
    : _searchPaths(getSearchPaths(basePath))
    , _basePath(basePath)
{
}

Application::Info::IconPath IconFinder::find(const std::string& iconName)
{
    if (iconName[0] == '/')  // explicit icon path received
    {
        return Application::Info::IconPath::from_raw(iconName);
    }
    auto size = 0;
    std::string iconPath;
    for (const auto& path : _searchPaths)
    {
        if (path.size > size)
        {
            auto foundIcon = findExistingIcon(path.path, iconName);
            if (!foundIcon.empty())
            {
                size = path.size;
                iconPath = foundIcon;
            }
        }
    }
    if (iconPath == "")  // attempt to fallback to pixmaps
    {
        auto pixmap = g_build_filename(_basePath.c_str(), PIXMAPS_PATH, nullptr);
        iconPath = findExistingIcon(pixmap, iconName);
        g_free(pixmap);
        if (iconPath.empty())
        {
            auto defaultPath = g_build_filename(_basePath.c_str(), iconName.c_str(), nullptr);
            iconPath = defaultPath;
            g_free(defaultPath);
        }
    }
    return Application::Info::IconPath::from_raw(iconPath);
}

bool IconFinder::hasImageExtension(const char* filename)
{
    for (const auto& extension : ICON_TYPES)
    {
        if (g_str_has_suffix(filename, extension))
        {
            return true;
        }
    }
    return false;
}

std::string IconFinder::findExistingIcon(const std::string& path, const std::string& iconName)
{
    std::string iconPath;
    if (hasImageExtension(iconName.c_str()))
    {
        auto fullpath = g_build_filename(path.c_str(), iconName.c_str(), nullptr);
        if (g_file_test(fullpath, G_FILE_TEST_EXISTS))
        {
            iconPath = fullpath;
        }
        g_free(fullpath);
        return iconPath;
    }

    for (const auto& extension : ICON_TYPES)
    {
        auto fullpath = g_build_filename(path.c_str(), (iconName + extension).c_str(), nullptr);
        if (g_file_test(fullpath, G_FILE_TEST_EXISTS))
        {
            iconPath = fullpath;
            g_free(fullpath);
            break;
        }
        g_free(fullpath);
    }
    return iconPath;
}

std::list<IconFinder::ThemeSubdirectory> IconFinder::validDirectories(std::string basePath, gchar* directory, int size)
{
    std::list<IconFinder::ThemeSubdirectory> dirs;
    auto globalHicolorTheme = g_build_filename(basePath.c_str(), HICOLOR_THEME_DIR, directory, nullptr);
    if (g_file_test(globalHicolorTheme, G_FILE_TEST_EXISTS))
    {
        dirs.push_back(ThemeSubdirectory{std::string(globalHicolorTheme), size});
    }
    g_free(globalHicolorTheme);

    return dirs;
}

std::list<IconFinder::ThemeSubdirectory> IconFinder::addSubdirectoryByType(std::shared_ptr<GKeyFile> themefile,
                                                                           gchar* directory,
                                                                           std::string basePath)
{
    GError* error = nullptr;
    auto gType = g_key_file_get_string(themefile.get(), directory, TYPE_PROPERTY, &error);
    if (error != nullptr)
    {
        g_error_free(error);
        return std::list<ThemeSubdirectory>{};
    }
    std::string type(gType);
    g_free(gType);

    if (type == FIXED_CONTEXT)
    {
        auto size = g_key_file_get_integer(themefile.get(), directory, SIZE_PROPERTY, &error);
        if (error != nullptr)
        {
            g_error_free(error);
        }
        else
        {
            return validDirectories(basePath, directory, size);
        }
    }
    else if (type == SCALABLE_CONTEXT)
    {
        auto size = g_key_file_get_integer(themefile.get(), directory, MAXSIZE_PROPERTY, &error);
        if (error != nullptr)
        {
            g_error_free(error);
        }
        else
        {
            return validDirectories(basePath, directory, size);
        }
    }
    else if (type == THRESHOLD_CONTEXT)
    {
        auto size = g_key_file_get_integer(themefile.get(), directory, SIZE_PROPERTY, &error);
        if (error != nullptr)
        {
            g_error_free(error);
        }
        else
        {
            auto threshold = g_key_file_get_integer(themefile.get(), directory, THRESHOLD_PROPERTY, &error);
            if (error != nullptr)
            {
                threshold = 2;  // threshold defaults to 2
                g_error_free(error);
            }
            return validDirectories(basePath, directory, size + threshold);
        }
    }
    return std::list<ThemeSubdirectory>{};
}

std::list<IconFinder::ThemeSubdirectory> IconFinder::searchIconPaths(std::shared_ptr<GKeyFile> themefile,
                                                                     gchar** directories,
                                                                     std::string basePath)
{
    std::list<ThemeSubdirectory> subdirs;
    for (auto i = 0; directories[i] != nullptr; ++i)
    {
        GError* error = nullptr;
        auto context = g_key_file_get_string(themefile.get(), directories[i], CONTEXT_PROPERTY, &error);
        if (error != nullptr)
        {
            g_error_free(error);
            continue;
        }
        if (g_strcmp0(context, APPLICATIONS_TYPE) == 0)
        {
            auto newDirs = addSubdirectoryByType(themefile, directories[i], basePath);
            subdirs.insert(subdirs.end(), newDirs.begin(), newDirs.end());
        }
        g_free(context);
    }
    return subdirs;
}

std::list<IconFinder::ThemeSubdirectory> IconFinder::getSearchPaths(const std::string& basePath)
{
    GError* error = nullptr;
    auto themefile = std::shared_ptr<GKeyFile>(g_key_file_new(), g_key_file_free);
    g_key_file_load_from_file(themefile.get(), (basePath + HICOLOR_THEME_FILE).c_str(), G_KEY_FILE_NONE, &error);
    if (error != nullptr)
    {
        g_error_free(error);
        return std::list<ThemeSubdirectory>();
    }

    g_key_file_set_list_separator(themefile.get(), ',');
    auto directories =
        g_key_file_get_string_list(themefile.get(), ICON_THEME_KEY, DIRECTORIES_PROPERTY, nullptr, &error);
    if (error != nullptr)
    {
        g_error_free(error);
        return std::list<ThemeSubdirectory>();
    }

    // find icons sorted by size, highest to lowest
    auto iconPaths = searchIconPaths(themefile, directories, basePath);
    iconPaths.sort([](const ThemeSubdirectory& lhs, const ThemeSubdirectory& rhs) { return lhs.size > rhs.size; });

    g_strfreev(directories);
    return iconPaths;
}

}  // namesapce app_launch
}  // namespace ubuntu
