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
#include <regex>

namespace ubuntu
{
namespace app_launch
{
namespace
{
constexpr auto HICOLOR_THEME_DIR = "/icons/hicolor/";
constexpr auto HICOLOR_THEME_FILE = "/icons/hicolor/index.theme";
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
constexpr auto PIXMAPS_PATH = "/pixmaps/";
constexpr auto ICON_TYPES = {".png", ".svg", ".xpm"};

static const std::regex iconSizeDirname = std::regex("$(\\d+)x(\\d+)^");
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
       and icon */
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
        auto fullpath = g_build_filename(path.c_str(), iconName.c_str(), nullptr);
        if (g_file_test(fullpath, G_FILE_TEST_EXISTS))
        {
            iconPath = fullpath;
        }
        g_free(fullpath);
        return iconPath;
    }

    /* Otherwise check all the valid extensions to see if they exist */
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

/** Create a directory item if the directory exists */
std::list<IconFinder::ThemeSubdirectory> IconFinder::validDirectories(const std::string& basePath,
                                                                      gchar* directory,
                                                                      int size)
{
    std::list<IconFinder::ThemeSubdirectory> dirs;
    auto globalHicolorTheme = g_build_filename(basePath.c_str(), HICOLOR_THEME_DIR, directory, nullptr);
    if (g_file_test(globalHicolorTheme, G_FILE_TEST_EXISTS))
    {
        dirs.emplace_back(ThemeSubdirectory{std::string(globalHicolorTheme), size});
    }
    g_free(globalHicolorTheme);

    return dirs;
}

/** Take the data in a directory stanza and turn it into an actual directory */
std::list<IconFinder::ThemeSubdirectory> IconFinder::addSubdirectoryByType(std::shared_ptr<GKeyFile> themefile,
                                                                           gchar* directory,
                                                                           const std::string& basePath)
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

/** Parse a theme file's various stanzas for each directory */
std::list<IconFinder::ThemeSubdirectory> IconFinder::searchIconPaths(std::shared_ptr<GKeyFile> themefile,
                                                                     gchar** directories,
                                                                     const std::string& basePath)
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
            subdirs.splice(subdirs.end(), newDirs);
        }
        g_free(context);
    }
    return subdirs;
}

/** Try to get theme subdirectories using .theme file in the hicolor theme file
    if it exists */
std::list<IconFinder::ThemeSubdirectory> IconFinder::themeFileSearchPaths(const std::string& basePath)
{
    std::string themeFilePath = basePath + HICOLOR_THEME_FILE;
    GError* error = nullptr;
    auto themefile = std::shared_ptr<GKeyFile>(g_key_file_new(), g_key_file_free);
    g_key_file_load_from_file(themefile.get(), themeFilePath.c_str(), G_KEY_FILE_NONE, &error);
    if (error != nullptr)
    {
        g_debug("Unable to find Hicolor theme file: %s", themeFilePath.c_str());
        g_error_free(error);
        return std::list<ThemeSubdirectory>();
    }

    g_key_file_set_list_separator(themefile.get(), ',');
    auto directories =
        g_key_file_get_string_list(themefile.get(), ICON_THEME_KEY, DIRECTORIES_PROPERTY, nullptr, &error);
    if (error != nullptr)
    {
        g_debug("Hicolor theme file '%s' didn't have any directories", themeFilePath.c_str());
        g_error_free(error);
        return std::list<ThemeSubdirectory>();
    }

    auto iconPaths = searchIconPaths(themefile, directories, basePath);
    g_strfreev(directories);
    return iconPaths;
}

/** Look into a theme directory and see if we can use the subdirectories
    as icon folders. Sadly inefficient. */
std::list<IconFinder::ThemeSubdirectory> IconFinder::themeDirSearchPaths(const std::string& themeDir)
{
    std::list<IconFinder::ThemeSubdirectory> searchPaths;
    GError* error = nullptr;
    auto gdir = g_dir_open(themeDir.c_str(), 0, &error);

    if (error != nullptr)
    {
        g_debug("Unable to open directory '%s' becuase: %s", themeDir.c_str(), error->message);
        g_error_free(error);
        return searchPaths;
    }

    const gchar* dirname = nullptr;
    while ((dirname = g_dir_read_name(gdir)) != nullptr)
    {
        std::string fullPath = themeDir + "/" + dirname;

        /* Directories only */
        if (!g_file_test(fullPath.c_str(), G_FILE_TEST_IS_DIR))
            continue;

        if (dirname == std::string("scalable"))
        {
            /* We don't really know what to do with scalable here, let's
               call them 1024 images */
            searchPaths.emplace_back(IconFinder::ThemeSubdirectory{fullPath, 1024});
            continue;
        }

        std::smatch match;
        std::string dirstr(dirname);
        /* We want it to match and have the same values for the first and second size */
        if (std::regex_match(dirstr, match, iconSizeDirname) && match[1].str() == match[2].str())
        {
            searchPaths.emplace_back(IconFinder::ThemeSubdirectory{fullPath, std::atoi(match[1].str().c_str())});
        }
    }

    g_dir_close(gdir);
    return searchPaths;
}

/** Gets all the search paths, from either a theme file or just
    looking at the directory. And possibly pixmaps as well */
std::list<IconFinder::ThemeSubdirectory> IconFinder::getSearchPaths(const std::string& basePath)
{
    std::list<IconFinder::ThemeSubdirectory> iconPaths;

    std::string hicolorDir = basePath + HICOLOR_THEME_DIR;
    if (g_file_test(hicolorDir.c_str(), (GFileTest)(G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)))
    {
        /* If the directory exists, it could have icons */
        iconPaths.emplace_back(IconFinder::ThemeSubdirectory{hicolorDir, 1});

        /* Now see if we can get directories from a theme file */
        auto themeDirs = themeFileSearchPaths(basePath);
        if (themeDirs.size() > 0)
        {
            iconPaths.splice(iconPaths.end(), themeDirs);
        }
        else
        {
            /* If we didn't get from a theme file, we need to look
               at the directories ourselves :-( */
            auto dirPaths = themeDirSearchPaths(hicolorDir);
            iconPaths.splice(iconPaths.end(), dirPaths);
        }
    }

    /* Add the pixmaps path as a fallback if it exists */
    std::string pixmapsPath = basePath + PIXMAPS_PATH;
    if (g_file_test(pixmapsPath.c_str(), (GFileTest)(G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)))
    {
        iconPaths.emplace_back(IconFinder::ThemeSubdirectory{pixmapsPath, 1});
    }

    // find icons sorted by size, highest to lowest
    iconPaths.sort([](const ThemeSubdirectory& lhs, const ThemeSubdirectory& rhs) { return lhs.size > rhs.size; });
    return iconPaths;
}

}  // namesapce app_launch
}  // namespace ubuntu
