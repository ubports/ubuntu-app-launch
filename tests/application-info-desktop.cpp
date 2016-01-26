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

#include <gtest/gtest.h>

class ApplicationInfoDesktop : public ::testing::Test
{
    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }
};

TEST_F(ApplicationInfoDesktop, NullKeyfile)
{
    EXPECT_THROW(Ubuntu::AppLaunch::AppInfo::Desktop({}, "/"), std::runtime_error);


}
