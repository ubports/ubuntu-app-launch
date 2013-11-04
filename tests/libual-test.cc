/*
 * Copyright 2013 Canonical Ltd.
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

#include <gtest/gtest.h>
#include <gio/gio.h>

extern "C" {
#include "upstart-app-launch.h"
}

class LibUAL : public ::testing::Test
{
	protected:
		virtual void SetUp() {
			g_setenv("UPSTART_APP_LAUNCH_USE_SESSION", "1", TRUE);

			/* NOTE: We're doing the bus in each test here */

			return;
		}
		virtual void TearDown() {

			return;
		}

};

TEST_F(LibUAL, Dummy)
{
	return;
}
