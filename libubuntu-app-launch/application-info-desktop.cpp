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

namespace ubuntu
{
namespace app_launch
{
namespace app_info
{

const char* DESKTOP_GROUP = "Desktop Entry";

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

Desktop::Desktop(std::shared_ptr<GKeyFile> keyfile, const std::string& basePath)
    : _keyfile([keyfile]()
               {
                   if (!keyfile)
                   {
                       throw std::runtime_error("Can not build a desktop application info object with a null keyfile");
                   }
                   return keyfile;
               }())
    , _basePath(basePath)
    , _name(stringFromKeyfile<Application::Info::Name>(keyfile, "Name", "Unable to get name from keyfile"))
    , _description(stringFromKeyfile<Application::Info::Description>(keyfile, "Comment"))
    , _iconPath(
          fileFromKeyfile<Application::Info::IconPath>(keyfile, basePath, "Icon", "Missing icon for desktop file"))
    , _splashInfo({
        title : stringFromKeyfile<Application::Info::SplashTitle>(keyfile, "X-Ubuntu-Splash-Title"),
        image : fileFromKeyfile<Application::Info::SplashImage>(keyfile, basePath, "X-Ubuntu-Splash-Image"),
        backgroundColor : stringFromKeyfile<Application::Info::SplashColor>(keyfile, "X-Ubuntu-Splash-Color"),
        headerColor : stringFromKeyfile<Application::Info::SplashColor>(keyfile, "X-Ubuntu-Splash-Color-Header"),
        footerColor : stringFromKeyfile<Application::Info::SplashColor>(keyfile, "X-Ubuntu-Splash-Color-Footer"),
        showHeader : boolFromKeyfile<Application::Info::SplashShowHeader>(keyfile, "X-Ubuntu-Splash-Show-Header", false)
    })
    , _supportedOrientations(
          [keyfile]()
          {
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
                          throw std::runtime_error("Invalid orientation string '" + std::string(orientationStrv[i]) +
                                                   "'");
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
