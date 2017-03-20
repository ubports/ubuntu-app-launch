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
#include "string-util.h"
#include <regex>

#include <unity/util/GlibMemory.h>

using namespace unity::util;

namespace ubuntu
{
namespace app_launch
{
namespace
{
constexpr auto ICONS_DIR = "/icons";
constexpr auto ICON_THEMES = {"suru", "Humanity", "Adwaita", "gnome", "hicolor"};
constexpr auto THEME_INDEX_FILE = "index.theme";
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
constexpr auto PIXMAPS_PATH = "/pixmaps/";
constexpr auto ICON_TYPES = {".png", ".svg", ".xpm"};
constexpr auto METAGUI_PATH = "/meta/gui";

static const std::regex ICON_SIZE_DIRNAME = std::regex("^(\\d+)x\\1$");
static const std::regex SCALABLE_WITH_REGEX = std::regex("^scalable-up-to-(\\d+)$");
}  // anonymous namespace

IconFinder::IconFinder(std::string basePath)
    : _searchPaths(getSearchPaths(basePath))
    , _basePath(basePath)
{
}

/** Finds an icon in the search paths that we have for this path */
Application::Info::IconPath IconFinder::find(const std::string& iconName)
{
    if (iconName[0] == '/')  // explicit icon path received
    {
        return Application::Info::IconPath::from_raw(iconName);
    }

    /* Look in each directory slowly decreasing the size until we find
       an icon */
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

    return Application::Info::IconPath::from_raw(iconPath);
}

/** Check to see if this is an icon name or an icon filename */
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

/** Check in a given path if there is an existing file in it that satifies our name */
std::string IconFinder::findExistingIcon(const std::string& path, const std::string& iconName)
{
    std::string iconPath;

    /* If it already has an extension, only check that one */
    if (hasImageExtension(iconName.c_str()))
    {
        auto fullpath = unique_gchar(g_build_filename(path.c_str(), iconName.c_str(), nullptr));
        if (g_file_test(fullpath.get(), G_FILE_TEST_EXISTS))
        {
            iconPath = fullpath.get();
        }
        return iconPath;
    }

    /* Otherwise check all the valid extensions to see if they exist */
    for (const auto& extension : ICON_TYPES)
    {
        auto fullpath = unique_gchar(g_build_filename(path.c_str(), (iconName + extension).c_str(), nullptr));
        if (g_file_test(fullpath.get(), G_FILE_TEST_EXISTS))
        {
            iconPath = fullpath.get();
            break;
        }
    }
    return iconPath;
}

/** Create a directory item if the directory exists */
std::list<IconFinder::ThemeSubdirectory> IconFinder::validDirectories(const std::string& themePath,
                                                                      gchar* directory,
                                                                      int size)
{
    std::list<IconFinder::ThemeSubdirectory> dirs;
    auto globalHicolorTheme = unique_gchar(g_build_filename(themePath.c_str(), directory, nullptr));
    if (g_file_test(globalHicolorTheme.get(), G_FILE_TEST_EXISTS))
    {
        dirs.emplace_back(ThemeSubdirectory{std::string(globalHicolorTheme.get()), size});
    }

    return dirs;
}

/** Take the data in a directory stanza and turn it into an actual directory */
std::list<IconFinder::ThemeSubdirectory> IconFinder::addSubdirectoryByType(std::shared_ptr<GKeyFile> themefile,
                                                                           gchar* directory,
                                                                           const std::string& themePath)
{
    GError* error = nullptr;
    auto gType = unique_gchar(g_key_file_get_string(themefile.get(), directory, TYPE_PROPERTY, &error));
    if (error != nullptr)
    {
        g_error_free(error);
        return std::list<ThemeSubdirectory>{};
    }
    std::string type(gType.get());

    if (type == FIXED_CONTEXT)
    {
        auto size = g_key_file_get_integer(themefile.get(), directory, SIZE_PROPERTY, &error);
        if (error != nullptr)
        {
            g_error_free(error);
        }
        else
        {
            return validDirectories(themePath, directory, size);
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
            return validDirectories(themePath, directory, size);
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
            return validDirectories(themePath, directory, size + threshold);
        }
    }
    return std::list<ThemeSubdirectory>{};
}

/** Parse a theme file's various stanzas for each directory */
std::list<IconFinder::ThemeSubdirectory> IconFinder::searchIconPaths(std::shared_ptr<GKeyFile> themefile,
                                                                     gchar** directories,
                                                                     const std::string& themePath)
{
    std::list<ThemeSubdirectory> subdirs;
    for (auto i = 0; directories[i] != nullptr; ++i)
    {
        GError* error = nullptr;
        auto context = unique_gchar(g_key_file_get_string(themefile.get(), directories[i], CONTEXT_PROPERTY, &error));
        if (error != nullptr)
        {
            g_error_free(error);
            continue;
        }

        auto newDirs = addSubdirectoryByType(themefile, directories[i], themePath);
        subdirs.splice(subdirs.end(), newDirs);

    }
    return subdirs;
}

/** Try to get theme subdirectories using .theme file in the given theme path
    if it exists */
std::list<IconFinder::ThemeSubdirectory> IconFinder::themeFileSearchPaths(const std::string& themePath)
{
    auto themeFilePath = unique_gchar(g_build_filename(themePath.c_str(), THEME_INDEX_FILE, nullptr));
    GError* error = nullptr;
    auto themefile = share_glib(g_key_file_new());
    g_key_file_load_from_file(themefile.get(), themeFilePath.get(), G_KEY_FILE_NONE, &error);
    if (error != nullptr)
    {
        g_debug("Unable to find theme file: %s", themeFilePath.get());
        g_error_free(error);
        return std::list<ThemeSubdirectory>();
    }

    g_key_file_set_list_separator(themefile.get(), ',');
    auto directories = unique_gcharv(
        g_key_file_get_string_list(themefile.get(), ICON_THEME_KEY, DIRECTORIES_PROPERTY, nullptr, &error));
    if (error != nullptr)
    {
        g_debug("Theme file '%s' didn't have any directories", themeFilePath.get());
        g_error_free(error);
        return std::list<ThemeSubdirectory>();
    }

    auto iconPaths = searchIconPaths(themefile, directories.get(), themePath);
    return iconPaths;
}

/** Look into a theme directory and see if we can use the subdirectories
    as icon folders. This is a fallback, and is sadly inefficient. */
std::list<IconFinder::ThemeSubdirectory> IconFinder::themeDirSearchPaths(const std::string& themeDir)
{
    std::list<IconFinder::ThemeSubdirectory> searchPaths;
    GError* error = nullptr;
    auto gdir = unique_glib(g_dir_open(themeDir.c_str(), 0, &error));

    if (error != nullptr)
    {
        g_debug("Unable to open directory '%s' becuase: %s", themeDir.c_str(), error->message);
        g_error_free(error);
        return searchPaths;
    }

    const gchar* dirname = nullptr;
    while ((dirname = g_dir_read_name(gdir.get())) != nullptr)
    {
        /* Iterate over size-based directories */
        auto sizePath = unique_gchar(g_build_filename(themeDir.c_str(), dirname, nullptr));
        if (g_file_test(sizePath.get(), G_FILE_TEST_IS_DIR))
        {
            std::smatch match;
            std::string dirstr(dirname);
            int size;
            if (std::regex_match(dirstr, match, ICON_SIZE_DIRNAME))
            {
                size = std::atoi(match[1].str().c_str());
            }
            else if (g_strcmp0(dirname, "scalable") == 0)
            {
                /* We don't really know what to do with scalable icons, let's
                   call them 256 images */
                size = 256;
            }
            else
            {
                /* Some directories are "scalable with", meaning that they
                   are meant to be used up to a certain size */
                std::smatch scalablewith;
                if (std::regex_match(dirstr, scalablewith, SCALABLE_WITH_REGEX))
                {
                    size = std::atoi(scalablewith[1].str().c_str());
                }
                else
                {
                    /* Otherwise we don't know what to do with this dir,
                       ignore for now */
                    continue;
                }
            }

            auto subdir = unique_glib(g_dir_open(sizePath.get(), 0, &error));
            const gchar* subdirname = nullptr;
            while ((subdirname = g_dir_read_name(subdir.get())) != nullptr)
            {
                auto fullPath = unique_gchar(g_build_filename(sizePath.get(), subdirname, nullptr));
                if (g_file_test(fullPath.get(), G_FILE_TEST_IS_DIR))
                {
                    searchPaths.emplace_back(IconFinder::ThemeSubdirectory{fullPath.get(), size});
                }
            }
        }
    }

    return searchPaths;
}

/** Gets all search paths from a given theme directory via theme file or
    manually scanning the directory. */
std::list<IconFinder::ThemeSubdirectory> IconFinder::iconsFromThemePath(const gchar* themeDir)
{
    std::list<IconFinder::ThemeSubdirectory> iconPaths;
    if (g_file_test(themeDir, G_FILE_TEST_IS_DIR))
    {
        /* If the directory exists, it could have icons of unknown size */
        iconPaths.emplace_back(IconFinder::ThemeSubdirectory{themeDir, 1});

        /* Now see if we can get directories from a theme file */
        auto themeDirs = themeFileSearchPaths(themeDir);
        if (themeDirs.size() > 0)
        {
            iconPaths.splice(iconPaths.end(), themeDirs);
        }
        else
        {
            /* If we didn't get from a theme file, we need to manually scan the directories */
            auto dirPaths = themeDirSearchPaths(themeDir);
            iconPaths.splice(iconPaths.end(), dirPaths);
        }
    }
    return iconPaths;
}

/** Gets search paths based on common icon directories including themes and pixmaps. */
std::list<IconFinder::ThemeSubdirectory> IconFinder::getSearchPaths(const std::string& basePath)
{
    std::list<IconFinder::ThemeSubdirectory> iconPaths;

    for (const auto& theme : ICON_THEMES)
    {
        auto dir = unique_gchar(g_build_filename(basePath.c_str(), ICONS_DIR, theme, nullptr));
        auto icons = iconsFromThemePath(dir.get());
        iconPaths.splice(iconPaths.end(), icons);
    }

    /* Add root icons directory as potential path */
    auto iconsPath = unique_gchar(g_build_filename(basePath.c_str(), ICONS_DIR, nullptr));
    if (g_file_test(iconsPath.get(), G_FILE_TEST_IS_DIR))
    {
        iconPaths.emplace_back(IconFinder::ThemeSubdirectory{iconsPath.get(), 1});
    }

    /* Add the pixmaps path as a fallback if it exists */
    auto pixmapsPath = unique_gchar(g_build_filename(basePath.c_str(), PIXMAPS_PATH, nullptr));
    if (g_file_test(pixmapsPath.get(), G_FILE_TEST_IS_DIR))
    {
        iconPaths.emplace_back(IconFinder::ThemeSubdirectory{pixmapsPath.get(), 1});
    }

    /* Add the snap meta/gui path as a fallback if it exists */
    auto snapMetaGuiPath = unique_gchar(g_build_filename(basePath.c_str(), METAGUI_PATH, nullptr));
    if (g_file_test(snapMetaGuiPath.get(), G_FILE_TEST_IS_DIR))
    {
        iconPaths.emplace_back(IconFinder::ThemeSubdirectory{snapMetaGuiPath.get(), 1});
    }

    /* Add the base directory itself as a fallback, for "foo.png" icon names */
    iconPaths.emplace_back(IconFinder::ThemeSubdirectory{basePath, 1});

    // find icons sorted by size, highest to lowest
    iconPaths.sort([](const ThemeSubdirectory& lhs, const ThemeSubdirectory& rhs) { return lhs.size > rhs.size; });
    return iconPaths;
}

}  // namespace app_launch
}  // namespace ubuntu
