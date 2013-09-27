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

extern "C" {
#include "../second-exec-core.h"
#include "upstart-app-launch.h"
#include "upstart-app-launch-mock.h"
}

class SecondExecTest : public ::testing::Test
{
	private:
		gchar * last_focus_appid = NULL;
		gchar * last_resume_appid = NULL;

		static void focus_cb (const gchar * appid, gpointer user_data) {
			SecondExecTest * _this = static_cast<SecondExecTest *>(user_data);
			g_free(_this->last_focus_appid);
			_this->last_focus_appid = g_strdup(appid);
		}

		static void resume_cb (const gchar * appid, gpointer user_data) {
			SecondExecTest * _this = static_cast<SecondExecTest *>(user_data);
			g_free(_this->last_resume_appid);
			_this->last_resume_appid = g_strdup(appid);
		}

	protected:
		virtual void SetUp() {
			upstart_app_launch_observer_add_app_focus(focus_cb, this);
			upstart_app_launch_observer_add_app_resume(resume_cb, this);

			return;
		}
		virtual void TearDown() {
			upstart_app_launch_observer_delete_app_focus(focus_cb, this);
			upstart_app_launch_observer_delete_app_resume(resume_cb, this);

			return;
		}
};

TEST_F(SecondExecTest, StubTest)
{
	ASSERT_TRUE(TRUE);
}
