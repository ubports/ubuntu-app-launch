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

std::map<std::string, std::shared_ptr<IconFinder>> IconFinder::_instances;

IconFinder::IconFinder(std::string basePath)
    : _searchPaths(getSearchPaths(basePath))
    , _basePath(basePath)
{
}

std::shared_ptr<IconFinder> IconFinder::fromBasePath(std::string basePath)
{
    if (_instances.find(basePath) == _instances.end())
    {
        _instances[basePath] = std::shared_ptr<IconFinder>(new IconFinder(basePath));
    }
    return _instances[basePath];
}

std::string IconFinder::find(const std::string& iconName)
{
    auto defaultPath = g_build_filename(_basePath.c_str(), iconName, nullptr);
    std::string iconPath = defaultPath;
    g_free(defaultPath);

    if (iconName[0] == '/') // explicit icon path received
    {
        return iconName;
    }
    else if (hasImageExtension(iconName.c_str()))
    {
        if (g_file_test((_basePath + "/usr/share/pixmaps/" + iconName).c_str(), G_FILE_TEST_EXISTS))
        {
            return _basePath + "/usr/share/pixmaps/" + iconName;
        }
        return iconPath;
    }
    auto size = 0;
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
    return iconPath;
}

bool IconFinder::hasImageExtension(const char* filename)
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

bool IconFinder::findExistingIcon(const std::string& path, const gchar* iconName, std::string &iconPath)
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

void IconFinder::addSubdirectoryByType(std::shared_ptr<GKeyFile> themefile, gchar* directory, std::string themePath, std::list<IconFinder::ThemeSubdirectory>& subdirs)
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

std::list<IconFinder::ThemeSubdirectory> IconFinder::searchIconPaths(std::shared_ptr<GKeyFile> themefile, gchar** directories, std::string themePath)
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

std::list<IconFinder::ThemeSubdirectory> IconFinder::getSearchPaths(const std::string& basePath)
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

} // namesapce app_launch
} // namespace ubuntu
