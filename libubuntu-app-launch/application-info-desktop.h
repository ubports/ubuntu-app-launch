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

#include "application.h"
#include <glib.h>
#include <mutex>

#pragma once

namespace Ubuntu
{
namespace AppLaunch
{
namespace AppInfo
{

class Desktop : public Application::Info
{
public:
    Desktop(std::shared_ptr<GKeyFile> keyfile,
            const std::string& basePath);

    const Application::Info::Name& name() override
    {
        return _name;
    }
    const Application::Info::Description& description() override
    {
        return _description;
    }
    const Application::Info::IconPath& iconPath() override
    {
        return _iconPath;
    }

    Application::Info::SplashInfo splash() override
    {
        return _splashInfo;
    }

    Application::Info::Orientations supportedOrientations() override
    {
        return _supportedOrientations;
    }

    Application::Info::UbuntuLifecycle ubuntuLifecycle() override
    {
        return _ubuntuLifecycle;
    }

private:
    std::shared_ptr<GKeyFile> _keyfile;
    std::string _basePath;

    Application::Info::Name _name;
    Application::Info::Description _description;
    Application::Info::IconPath _iconPath;

    Application::Info::SplashInfo _splashInfo;
    Application::Info::Orientations _supportedOrientations;
    Application::Info::UbuntuLifecycle _ubuntuLifecycle;
};


}; // namespace AppInfo
}; // namespace AppLaunch
}; // namespace Ubuntu
