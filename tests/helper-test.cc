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
#include <glib/gstdio.h>
#include <libdbustest/dbus-test.h>
#include <gio/gio.h>

extern "C" {
#include "../helpers.h"
}

class HelperTest : public ::testing::Test
{
	private:

	protected:
		virtual void SetUp() {
			g_setenv("XDG_DATA_DIRS", CMAKE_SOURCE_DIR, TRUE);
			g_setenv("DATA_WRITE_DIR", CMAKE_BINARY_DIR, TRUE);
			g_setenv("UPSTART_JOB", "made-up-job", TRUE);
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

	/* No %s and no URLs */
	output = desktop_exec_parse("foo", NULL);
	ASSERT_EQ(1u, output->len);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	g_array_free(output, TRUE);

	/* URL without any % items */
	output = desktop_exec_parse("foo", "http://ubuntu.com");
	ASSERT_EQ(guint(1), output->len);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	g_array_free(output, TRUE);

	/* Little u with a single URL */
	output = desktop_exec_parse("foo %u", "http://ubuntu.com");
	ASSERT_EQ(guint(2), output->len);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "http://ubuntu.com");
	g_array_free(output, TRUE);

	/* Little u with a NULL string */
	output = desktop_exec_parse("foo %u", "");
	ASSERT_EQ(guint(1), output->len);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	g_array_free(output, TRUE);

	/* Big %U with a single URL */
	output = desktop_exec_parse("foo %U", "http://ubuntu.com");
	ASSERT_EQ(guint(2), output->len);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "http://ubuntu.com");
	g_array_free(output, TRUE);

	/* Little %u by itself */
	output = desktop_exec_parse("foo %u", "http://ubuntu.com http://slashdot.org");
	ASSERT_EQ(guint(2), output->len);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "http://ubuntu.com");
	g_array_free(output, TRUE);

	/* Little %u in quotes */
	output = desktop_exec_parse("foo %u \"%u\" %u%u", "http://ubuntu.com");
	ASSERT_EQ(guint(4), output->len);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "http://ubuntu.com");
	ASSERT_STREQ(g_array_index(output, gchar *, 2), "http://ubuntu.com");
	ASSERT_STREQ(g_array_index(output, gchar *, 3), "http://ubuntu.comhttp://ubuntu.com");
	g_array_free(output, TRUE);

	/* Single escaped " before the URL as a second param */
	output = desktop_exec_parse("foo \\\"%u", "http://ubuntu.com");
	ASSERT_EQ(guint(2), output->len);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "\"http://ubuntu.com");
	g_array_free(output, TRUE);

	/* URL is a quote, make sure we handle the error */
	output = desktop_exec_parse("foo %u", "\"");
	ASSERT_EQ(guint(1), output->len);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	g_array_free(output, TRUE);

	/* Lots of quotes, escaped and not */
	output = desktop_exec_parse("foo \\\"\"%u\"", "'\"'");
	ASSERT_EQ(guint(2), output->len);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "\"\"");
	g_array_free(output, TRUE);

	/* Let's have no params, but a little %u */
	output = desktop_exec_parse("foo\\ %u", "bar");
	ASSERT_EQ(guint(1), output->len);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo bar");
	g_array_free(output, TRUE);

	/* Big U with two URLs */
	output = desktop_exec_parse("foo %U", "http://ubuntu.com http://slashdot.org");
	ASSERT_EQ(guint(3), output->len);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "http://ubuntu.com");
	ASSERT_STREQ(g_array_index(output, gchar *, 2), "http://slashdot.org");
	g_array_free(output, TRUE);

	/* Big U with no URLs */
	output = desktop_exec_parse("foo %U", NULL);
	ASSERT_EQ(guint(1), output->len);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	g_array_free(output, TRUE);

	/* Big U with quoted URL */
	output = desktop_exec_parse("foo %U", "'http://ubuntu.com'");
	ASSERT_EQ(guint(2), output->len);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "http://ubuntu.com");
	g_array_free(output, TRUE);

	/* Big U with URLs that have spaces */
	output = desktop_exec_parse("foo %u", "'http://bob.com/foo bar/' http://slashdot.org");
	ASSERT_EQ(guint(2), output->len);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "http://bob.com/foo bar/");
	g_array_free(output, TRUE);

	/* %f with a valid file */
	output = desktop_exec_parse("foo %f", "file:///proc/version");
	ASSERT_EQ(guint(2), output->len);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "/proc/version");
	g_array_free(output, TRUE);

	/* A %f with a NULL string */
	output = desktop_exec_parse("foo %f", "");
	ASSERT_EQ(guint(1), output->len);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	g_array_free(output, TRUE);

	/* %f with a URL that isn't a file */
	output = desktop_exec_parse("foo %f", "torrent://moviephone.com/hot-new-movie");
	ASSERT_EQ(guint(1), output->len);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	g_array_free(output, TRUE);

	/* Lots of %f combinations */
	output = desktop_exec_parse("foo %f \"%f\" %f%f %f\\ %f", "file:///proc/version");
	ASSERT_EQ(guint(5), output->len);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "/proc/version");
	ASSERT_STREQ(g_array_index(output, gchar *, 2), "/proc/version");
	ASSERT_STREQ(g_array_index(output, gchar *, 3), "/proc/version/proc/version");
	ASSERT_STREQ(g_array_index(output, gchar *, 4), "/proc/version /proc/version");
	g_array_free(output, TRUE);

	/* Little f with two files */
	output = desktop_exec_parse("foo %f", "file:///proc/version file:///proc/uptime");
	ASSERT_EQ(guint(2), output->len);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "/proc/version");
	g_array_free(output, TRUE);

	/* Big F with two files */
	output = desktop_exec_parse("foo %F", "file:///proc/version file:///proc/uptime");
	ASSERT_EQ(guint(3), output->len);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "/proc/version");
	ASSERT_STREQ(g_array_index(output, gchar *, 2), "/proc/uptime");
	g_array_free(output, TRUE);

	/* Big F with no files */
	output = desktop_exec_parse("foo %F", NULL);
	ASSERT_EQ(guint(1), output->len);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	g_array_free(output, TRUE);

	/* Groups of percents */
	output = desktop_exec_parse("foo %% \"%%\" %%%%", NULL);
	ASSERT_EQ(guint(4), output->len);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "%");
	ASSERT_STREQ(g_array_index(output, gchar *, 2), "%");
	ASSERT_STREQ(g_array_index(output, gchar *, 3), "%%");
	g_array_free(output, TRUE);

	/* All the % sequences we don't support */
	output = desktop_exec_parse("foo %d %D %n %N %v %m %i %c %k", "file:///proc/version");
	ASSERT_EQ(guint(1), output->len);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	g_array_free(output, TRUE);

	return;
}

TEST_F(HelperTest, KeyfileForAppid)
{
	GKeyFile * keyfile = NULL;
	gchar * desktop = NULL;

	g_debug("XDG_DATA_DIRS=%s", g_getenv("XDG_DATA_DIRS"));

	keyfile = keyfile_for_appid("bar", &desktop);
	ASSERT_TRUE(keyfile == NULL);
	ASSERT_TRUE(desktop == NULL);

	keyfile = keyfile_for_appid("foo", &desktop);
	ASSERT_TRUE(keyfile != NULL);
	ASSERT_TRUE(desktop != NULL);
	g_key_file_free(keyfile);
	g_free(desktop);
	desktop = NULL;

	keyfile = keyfile_for_appid("no-exec", &desktop);
	ASSERT_TRUE(keyfile == NULL);
	ASSERT_TRUE(desktop == NULL);

	keyfile = keyfile_for_appid("no-entry", &desktop);
	ASSERT_TRUE(keyfile == NULL);
	ASSERT_TRUE(desktop == NULL);

	return;
}

TEST_F(HelperTest, DesktopToExec)
{
	GKeyFile * keyfile = NULL;
	gchar * exec = NULL;

	keyfile = g_key_file_new();
	ASSERT_TRUE(g_key_file_load_from_file(keyfile, CMAKE_SOURCE_DIR "/applications/foo.desktop", G_KEY_FILE_NONE, NULL));
	exec = desktop_to_exec(keyfile, "");
	ASSERT_TRUE(exec != NULL);
	ASSERT_STREQ(exec, "foo");
	g_free(exec);
	g_key_file_free(keyfile);

	keyfile = g_key_file_new();
	ASSERT_TRUE(g_key_file_load_from_file(keyfile, CMAKE_SOURCE_DIR "/applications/hidden.desktop", G_KEY_FILE_NONE, NULL));
	exec = desktop_to_exec(keyfile, "");
	ASSERT_TRUE(exec == NULL);
	g_key_file_free(keyfile);

	keyfile = g_key_file_new();
	ASSERT_TRUE(g_key_file_load_from_file(keyfile, CMAKE_SOURCE_DIR "/applications/nodisplay.desktop", G_KEY_FILE_NONE, NULL));
	exec = desktop_to_exec(keyfile, "");
	ASSERT_TRUE(exec == NULL);
	g_key_file_free(keyfile);

	keyfile = g_key_file_new();
	ASSERT_TRUE(g_key_file_load_from_file(keyfile, CMAKE_SOURCE_DIR "/applications/no-entry.desktop", G_KEY_FILE_NONE, NULL));
	exec = desktop_to_exec(keyfile, "");
	ASSERT_TRUE(exec == NULL);
	g_key_file_free(keyfile);

	keyfile = g_key_file_new();
	ASSERT_TRUE(g_key_file_load_from_file(keyfile, CMAKE_SOURCE_DIR "/applications/no-exec.desktop", G_KEY_FILE_NONE, NULL));
	exec = desktop_to_exec(keyfile, "");
	ASSERT_TRUE(exec == NULL);
	g_key_file_free(keyfile);

	keyfile = g_key_file_new();
	ASSERT_TRUE(g_key_file_load_from_file(keyfile, CMAKE_SOURCE_DIR "/applications/scope.desktop", G_KEY_FILE_NONE, NULL));
	exec = desktop_to_exec(keyfile, "");
	ASSERT_TRUE(exec == NULL);
	g_key_file_free(keyfile);

	keyfile = g_key_file_new();
	ASSERT_TRUE(g_key_file_load_from_file(keyfile, CMAKE_SOURCE_DIR "/applications/terminal.desktop", G_KEY_FILE_NONE, NULL));
	exec = desktop_to_exec(keyfile, "");
	ASSERT_TRUE(exec == NULL);
	g_key_file_free(keyfile);

	return;
}

TEST_F(HelperTest, ManifestToDesktop)
{
	gchar * desktop = NULL;

	g_setenv("TEST_CLICK_DB", "click-db-dir", TRUE);
	g_setenv("TEST_CLICK_USER", "test-user", TRUE);

	desktop = manifest_to_desktop(CMAKE_SOURCE_DIR "/click-app-dir/", "com.test.good_application_1.2.3");
	ASSERT_STREQ(CMAKE_SOURCE_DIR "/click-app-dir/application.desktop", desktop);
	g_free(desktop);
	desktop = NULL;

	desktop = manifest_to_desktop(CMAKE_SOURCE_DIR "/click-app-dir/", "com.test.bad-version_application_1.2.3");
	ASSERT_TRUE(desktop == NULL);

	desktop = manifest_to_desktop(CMAKE_SOURCE_DIR "/click-app-dir/", "com.test.no-app_application_1.2.3");
	ASSERT_TRUE(desktop == NULL);

	desktop = manifest_to_desktop(CMAKE_SOURCE_DIR "/click-app-dir/", "com.test.no-hooks_application_1.2.3");
	ASSERT_TRUE(desktop == NULL);

	desktop = manifest_to_desktop(CMAKE_SOURCE_DIR "/click-app-dir/", "com.test.no-version_application_1.2.3");
	ASSERT_TRUE(desktop == NULL);

	desktop = manifest_to_desktop(CMAKE_SOURCE_DIR "/click-app-dir/", "com.test.no-exist_application_1.2.3");
	ASSERT_TRUE(desktop == NULL);

	desktop = manifest_to_desktop(CMAKE_SOURCE_DIR "/click-app-dir/", "com.test.no-json_application_1.2.3");
	ASSERT_TRUE(desktop == NULL);

	desktop = manifest_to_desktop(CMAKE_SOURCE_DIR "/click-app-dir/", "com.test.no-object_application_1.2.3");
	ASSERT_TRUE(desktop == NULL);

	/* Bad App ID */
	desktop = manifest_to_desktop(CMAKE_SOURCE_DIR "/click-app-dir/", "com.test.good_application-1.2.3");
	ASSERT_TRUE(desktop == NULL);

	return;
}
