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
#define HICOLOR_THEME_DIR    "/usr/share/icons/hicolor/"
#define HICOLOR_THEME_FILE   HICOLOR_THEME_DIR + "index.theme"
#define APPLICATIONS_TYPE    "Applications"
#define SIZE_PROPERTY        "Size"
#define MAXSIZE_PROPERTY     "MaxSize"
#define THRESHOLD_PROPERTY   "Threshold"
#define FIXED_CONTEXT        "Fixed"
#define SCALABLE_CONTEXT     "Scalable"
#define THRESHOLD_CONTEXT    "Threshold"
#define CONTEXT_PROPERTY     "Context"
#define TYPE_PROPERTY        "Type"
#define DIRECTORIES_PROPERTY "Directories"
#define ICON_THEME_KEY       "Icon Theme"
#define PIXMAPS_PATH         "/usr/share/pixmaps/"
#define ICON_TYPES           {".png", ".svg", ".xpm"}
} // anonymous namespace

std::map<std::string, std::shared_ptr<IconFinder>> IconFinder::_instances;

IconFinder::IconFinder(std::string basePath)
    : _searchPaths(getSearchPaths(basePath))
    , _basePath(basePath)
{
}

std::shared_ptr<IconFinder> IconFinder::fromBasePath(const std::string& basePath)
{
    if (_instances.find(basePath) == _instances.end())
    {
        _instances[basePath] = std::shared_ptr<IconFinder>(new IconFinder(basePath));
    }
    return _instances[basePath];
}

std::string IconFinder::find(const std::string& iconName)
{
    auto defaultPath = g_build_filename(_basePath.c_str(), iconName.c_str(), nullptr);

    if (g_str_has_prefix(iconName.c_str(), "/")) // explicit icon path received
    {
        return iconName;
    }
    else if (hasImageExtension(iconName.c_str()))
    {
        if (g_file_test((_basePath + PIXMAPS_PATH + iconName).c_str(), G_FILE_TEST_EXISTS))
        {
            return _basePath + PIXMAPS_PATH + iconName;
        }
        return defaultPath;
    }
    auto size = 0;
    std::string iconPath;
    for (const auto& path: _searchPaths)
    {
        if (path.size > size)
        {
            if (findExistingIcon(path.path, iconName.c_str(), iconPath))
            {
                size = path.size;
            }
        }
    }
    if (iconPath == "")
    {
        if (!findExistingIcon(_basePath + PIXMAPS_PATH, iconName.c_str(), iconPath))
        {
            iconPath = defaultPath;
        }
    }

    g_free(defaultPath);
    return iconPath;
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

bool IconFinder::findExistingIcon(const std::string& path, const gchar* iconName, std::string &iconPath)
{
    if (hasImageExtension(iconName))
    {
        if (g_file_test((path + iconName).c_str(), G_FILE_TEST_EXISTS))
        {
            iconPath = path + iconName;
            return true;
        }
        return false;
    }
    for (const auto& extension : ICON_TYPES)
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

void IconFinder::addSubdirectoryByType(std::shared_ptr<GKeyFile> themefile, gchar* directory, std::string themePath, std::list<IconFinder::ThemeSubdirectory>& subdirs)
{
    GError* error = nullptr;
    auto type = g_key_file_get_string(themefile.get(), directory, TYPE_PROPERTY, &error);
    if (error != nullptr)
    {
        g_error_free(error);
        return;
    }
    if (g_strcmp0(type, FIXED_CONTEXT) == 0)
    {
        auto size = g_key_file_get_integer(themefile.get(), directory, SIZE_PROPERTY, &error);
        if (error != nullptr)
        {
            g_error_free(error);
        }
        else
        {
            subdirs.push_back(ThemeSubdirectory{themePath + directory + "/", size});
        }
    }
    else if (g_strcmp0(type, SCALABLE_CONTEXT) == 0)
    {
        auto size = g_key_file_get_integer(themefile.get(), directory, MAXSIZE_PROPERTY, &error);
        if (error != nullptr)
        {
            g_error_free(error);
        }
        else
        {
            subdirs.push_back(ThemeSubdirectory{themePath + directory + "/", size});
        }
    }
    else if (g_strcmp0(type, THRESHOLD_CONTEXT) == 0)
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
                threshold = 2; // threshold defaults to 2
                g_error_free(error);
            }
            subdirs.push_back(ThemeSubdirectory{themePath + directory + "/", size + threshold});
        }
    }
    g_free(type);
}

std::list<IconFinder::ThemeSubdirectory> IconFinder::searchIconPaths(std::shared_ptr<GKeyFile> themefile, gchar** directories, std::string themePath)
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
            addSubdirectoryByType(themefile, directories[i], themePath, subdirs);
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
    auto directories = g_key_file_get_string_list(themefile.get(), ICON_THEME_KEY, DIRECTORIES_PROPERTY, nullptr, &error);
    if (error != nullptr)
    {
        g_error_free(error);
        return std::list<ThemeSubdirectory>();
    }

    auto iconPaths = searchIconPaths(themefile, directories, basePath + HICOLOR_THEME_DIR);
    g_strfreev(directories);
    return iconPaths;
}

} // namesapce app_launch
} // namespace ubuntu
