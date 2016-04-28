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

#pragma once

#include <list>
#include <map>
#include <memory>
#include <glib.h>

namespace ubuntu
{
namespace app_launch
{
class IconFinder {
public:
    static std::shared_ptr<IconFinder> fromBasePath(const std::string& basePath);
    virtual std::string find(const std::string& iconName);
    virtual ~IconFinder() = default;

private:
    struct ThemeSubdirectory
    {
        std::string path;
        int size;
    };

    static std::map<std::string, std::shared_ptr<IconFinder>> _instances;
    std::list<ThemeSubdirectory> _searchPaths;
    std::string _basePath;

    static bool hasImageExtension(const char* filename);
    static bool findExistingIcon(const std::string& path, const gchar* iconName, std::string &iconPath);
    static void addSubdirectoryByType(std::shared_ptr<GKeyFile> themefile, gchar* directory, std::string themePath, std::list<ThemeSubdirectory>& subdirs);
    static std::list<ThemeSubdirectory> searchIconPaths(std::shared_ptr<GKeyFile> themefile, gchar** directories, std::string themePath);
    static std::list<ThemeSubdirectory> getSearchPaths(const std::string& basePath);

    explicit IconFinder(std::string basePath);
};

} // namespace app_launch
} // namesapce ubuntu
