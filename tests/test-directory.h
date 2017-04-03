/*
 * Copyright © 2017 Canonical Ltd.
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

#include <unity/util/GlibMemory.h>
#include <string>

class TestDirectory
{
    std::string dirname_;
    std::string appdir_;

public:
    TestDirectory()
    {
        GError *error{nullptr};

        auto dirname = ubuntu::app_launch::unique_gchar(g_dir_make_tmp("xdg-data-tmp-XXXXXX", &error));
        if (error != nullptr)
        {
            auto message = std::string{"Unable to create temporary directory: "} + error->message;
            g_error_free(error);
            throw std::runtime_error{message};
        }
        dirname_ = dirname.get();
        g_debug("Setting temp XDG_DATA directory: %s", dirname.get());

        appdir_ = ubuntu::app_launch::unique_gchar(g_build_filename(dirname.get(), "applications", nullptr)).get();
        g_mkdir_with_parents(appdir_.c_str(), 0700);

        auto datadirs =
            ubuntu::app_launch::unique_gchar(g_strdup_printf("%s:%s", dirname.get(), g_getenv("XDG_DATA_DIRS")));
        setenv("XDG_DATA_DIRS", datadirs.get(), 1);
    }

    ~TestDirectory()
    {
        auto command = ubuntu::app_launch::unique_gchar(g_strdup_printf("rm -rf %s", dirname_.c_str()));
        g_spawn_command_line_sync(command.get(), nullptr, nullptr, nullptr, nullptr);
        g_debug("Removing test directory: %s", dirname_.c_str());
    }

    void addApp(const std::string &appname,
                const std::list<std::pair<std::string, std::list<std::pair<std::string, std::string>>>> &keydata)
    {
        auto keyfile = unity::util::unique_glib(g_key_file_new());

        for (const auto &groupset : keydata)
        {
            auto groupname = groupset.first;
            for (const auto &dataset : groupset.second)
            {
                auto key = dataset.first;
                auto value = dataset.second;

                g_key_file_set_string(keyfile.get(), groupname.c_str(), key.c_str(), value.c_str());
            }
        }

        auto path = ubuntu::app_launch::unique_gchar(
            g_build_filename(appdir_.c_str(), (appname + ".desktop").c_str(), nullptr));
        GError *error{nullptr};

        g_key_file_save_to_file(keyfile.get(), path.get(), &error);
        if (error != nullptr)
        {
            auto message = std::string{"Unable to write desktop file for '"} + appname + "': " + error->message;
            g_error_free(error);
            throw std::runtime_error{message};
        }
    }

    void removeApp(const std::string &appname)
    {
        auto path = ubuntu::app_launch::unique_gchar(
            g_build_filename(appdir_.c_str(), (appname + ".desktop").c_str(), nullptr));
        unlink(path.get());
    }
};
