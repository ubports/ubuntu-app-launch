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

#pragma once

#include "registry-impl.h"
#include "registry.h"

#include <gmock/gmock.h>

class RegistryImplMock : public ubuntu::app_launch::Registry::Impl
{
public:
    RegistryImplMock(ubuntu::app_launch::Registry* reg)
        : ubuntu::app_launch::Registry::Impl(reg)
    {
        g_debug("Registry Mock Implementation Created");
    }

    ~RegistryImplMock()
    {
        g_debug("Registry Mock Implementation taken down");
    }

    MOCK_METHOD2(zgSendEvent, void(ubuntu::app_launch::AppID, const std::string& eventtype));
};

class RegistryMock : public ubuntu::app_launch::Registry
{
public:
    RegistryMock()
    {
        g_debug("Registry Mock Created");
        impl = std::unique_ptr<RegistryImplMock>(new RegistryImplMock(this));
    }

    ~RegistryMock()
    {
        g_debug("Registry Mock taken down");
    }
};
