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
#include "../helpers.h"
}

class HelperTest : public ::testing::Test
{
	private:

	protected:
		virtual void SetUp() {
			return;
		}
};

TEST_F(HelperTest, AppIdTest)
{
	ASSERT_TRUE(app_id_to_triplet("com.ubuntu.test_test_123", NULL, NULL, NULL));
	ASSERT_FALSE(app_id_to_triplet("inkscape", NULL, NULL, NULL));
	ASSERT_FALSE(app_id_to_triplet("music-app", NULL, NULL, NULL));

	gchar * pkg;
	gchar * app;
	gchar * version;

	ASSERT_TRUE(app_id_to_triplet("com.ubuntu.test_test_123", &pkg, &app, &version));
	ASSERT_STREQ(pkg, "com.ubuntu.test");
	ASSERT_STREQ(app, "test");
	ASSERT_STREQ(version, "123");

	g_free(pkg);
	g_free(app);
	g_free(version);

	return;
}
