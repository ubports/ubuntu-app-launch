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

TEST_F(HelperTest, DesktopExecParse)
{
	GArray * output;

	output = desktop_exec_parse("foo", NULL);
	ASSERT_EQ(output->len, 1);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	g_array_free(output, TRUE);

	output = desktop_exec_parse("foo", "http://ubuntu.com");
	ASSERT_EQ(output->len, 1);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	g_array_free(output, TRUE);

	output = desktop_exec_parse("foo %u", "http://ubuntu.com");
	ASSERT_EQ(output->len, 2);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "http://ubuntu.com");
	g_array_free(output, TRUE);

	output = desktop_exec_parse("foo %U", "http://ubuntu.com");
	ASSERT_EQ(output->len, 2);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "http://ubuntu.com");
	g_array_free(output, TRUE);

	output = desktop_exec_parse("foo %u", "http://ubuntu.com http://slashdot.org");
	ASSERT_EQ(output->len, 2);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "http://ubuntu.com");
	g_array_free(output, TRUE);

	output = desktop_exec_parse("foo \"%u\"", "http://ubuntu.com");
	ASSERT_EQ(output->len, 2);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "http://ubuntu.com");
	g_array_free(output, TRUE);

	output = desktop_exec_parse("foo \\\"%u", "http://ubuntu.com");
	ASSERT_EQ(output->len, 2);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "\"http://ubuntu.com");
	g_array_free(output, TRUE);

	output = desktop_exec_parse("foo %u", "\"");
	ASSERT_EQ(output->len, 2);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "\"");
	g_array_free(output, TRUE);

	output = desktop_exec_parse("foo \\\"\"%u\"", "\"");
	ASSERT_EQ(output->len, 2);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "\"\"");
	g_array_free(output, TRUE);

	output = desktop_exec_parse("foo\\ %u", "bar");
	ASSERT_EQ(output->len, 1);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo bar");
	g_array_free(output, TRUE);

	output = desktop_exec_parse("foo %U", "http://ubuntu.com http://slashdot.org");
	ASSERT_EQ(output->len, 2);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "http://ubuntu.com http://slashdot.org");
	g_array_free(output, TRUE);

	output = desktop_exec_parse("foo %f", "file:///proc/version");
	ASSERT_EQ(output->len, 2);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "/proc/version");
	g_array_free(output, TRUE);

	output = desktop_exec_parse("foo %f", "file:///proc/version file:///proc/uptime");
	ASSERT_EQ(output->len, 2);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "/proc/version");
	g_array_free(output, TRUE);

	output = desktop_exec_parse("foo %F", "file:///proc/version file:///proc/uptime");
	ASSERT_EQ(output->len, 2);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "/proc/version /proc/uptime");
	g_array_free(output, TRUE);

	output = desktop_exec_parse("foo %% %% %%%%", NULL);
	ASSERT_EQ(output->len, 4);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "%");
	ASSERT_STREQ(g_array_index(output, gchar *, 2), "%");
	ASSERT_STREQ(g_array_index(output, gchar *, 3), "%%");
	g_array_free(output, TRUE);



	return;
}
