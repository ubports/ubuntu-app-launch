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
	ASSERT_EQ(output->len, 1);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	g_array_free(output, TRUE);

	/* URL without any % items */
	output = desktop_exec_parse("foo", "http://ubuntu.com");
	ASSERT_EQ(output->len, 1);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	g_array_free(output, TRUE);

	/* Little u with a single URL */
	output = desktop_exec_parse("foo %u", "http://ubuntu.com");
	ASSERT_EQ(output->len, 2);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "http://ubuntu.com");
	g_array_free(output, TRUE);

	/* Little u with a NULL string */
	output = desktop_exec_parse("foo %u", "");
	ASSERT_EQ(output->len, 1);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	g_array_free(output, TRUE);

	/* Big %U with a single URL */
	output = desktop_exec_parse("foo %U", "http://ubuntu.com");
	ASSERT_EQ(output->len, 2);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "http://ubuntu.com");
	g_array_free(output, TRUE);

	/* Little %u by itself */
	output = desktop_exec_parse("foo %u", "http://ubuntu.com http://slashdot.org");
	ASSERT_EQ(output->len, 2);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "http://ubuntu.com");
	g_array_free(output, TRUE);

	/* Little %u in quotes */
	output = desktop_exec_parse("foo %u \"%u\" %u%u", "http://ubuntu.com");
	ASSERT_EQ(output->len, 4);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "http://ubuntu.com");
	ASSERT_STREQ(g_array_index(output, gchar *, 2), "http://ubuntu.com");
	ASSERT_STREQ(g_array_index(output, gchar *, 3), "http://ubuntu.comhttp://ubuntu.com");
	g_array_free(output, TRUE);

	/* Single escaped " before the URL as a second param */
	output = desktop_exec_parse("foo \\\"%u", "http://ubuntu.com");
	ASSERT_EQ(output->len, 2);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "\"http://ubuntu.com");
	g_array_free(output, TRUE);

	/* URL is a quote, make sure we handle the error */
	output = desktop_exec_parse("foo %u", "\"");
	ASSERT_EQ(output->len, 1);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	g_array_free(output, TRUE);

	/* Lots of quotes, escaped and not */
	output = desktop_exec_parse("foo \\\"\"%u\"", "'\"'");
	ASSERT_EQ(output->len, 2);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "\"\"");
	g_array_free(output, TRUE);

	/* Let's have no params, but a little %u */
	output = desktop_exec_parse("foo\\ %u", "bar");
	ASSERT_EQ(output->len, 1);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo bar");
	g_array_free(output, TRUE);

	/* Big U with two URLs */
	output = desktop_exec_parse("foo %U", "http://ubuntu.com http://slashdot.org");
	ASSERT_EQ(output->len, 3);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "http://ubuntu.com");
	ASSERT_STREQ(g_array_index(output, gchar *, 2), "http://slashdot.org");
	g_array_free(output, TRUE);

	/* Big U with no URLs */
	output = desktop_exec_parse("foo %U", NULL);
	ASSERT_EQ(output->len, 1);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	g_array_free(output, TRUE);

	/* Big U with quoted URL */
	output = desktop_exec_parse("foo %U", "'http://ubuntu.com'");
	ASSERT_EQ(output->len, 2);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "http://ubuntu.com");
	g_array_free(output, TRUE);

	/* Big U with URLs that have spaces */
	output = desktop_exec_parse("foo %u", "'http://bob.com/foo bar/' http://slashdot.org");
	ASSERT_EQ(output->len, 2);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "http://bob.com/foo bar/");
	g_array_free(output, TRUE);

	/* %f with a valid file */
	output = desktop_exec_parse("foo %f", "file:///proc/version");
	ASSERT_EQ(output->len, 2);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "/proc/version");
	g_array_free(output, TRUE);

	/* A %f with a NULL string */
	output = desktop_exec_parse("foo %f", "");
	ASSERT_EQ(output->len, 1);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	g_array_free(output, TRUE);

	/* %f with a URL that isn't a file */
	output = desktop_exec_parse("foo %f", "torrent://moviephone.com/hot-new-movie");
	ASSERT_EQ(output->len, 1);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	g_array_free(output, TRUE);

	/* Lots of %f combinations */
	output = desktop_exec_parse("foo %f \"%f\" %f%f %f\\ %f", "file:///proc/version");
	ASSERT_EQ(output->len, 5);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "/proc/version");
	ASSERT_STREQ(g_array_index(output, gchar *, 2), "/proc/version");
	ASSERT_STREQ(g_array_index(output, gchar *, 3), "/proc/version/proc/version");
	ASSERT_STREQ(g_array_index(output, gchar *, 4), "/proc/version /proc/version");
	g_array_free(output, TRUE);

	/* Little f with two files */
	output = desktop_exec_parse("foo %f", "file:///proc/version file:///proc/uptime");
	ASSERT_EQ(output->len, 2);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "/proc/version");
	g_array_free(output, TRUE);

	/* Big F with two files */
	output = desktop_exec_parse("foo %F", "file:///proc/version file:///proc/uptime");
	ASSERT_EQ(output->len, 3);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "/proc/version");
	ASSERT_STREQ(g_array_index(output, gchar *, 2), "/proc/uptime");
	g_array_free(output, TRUE);

	/* Big F with no files */
	output = desktop_exec_parse("foo %F", NULL);
	ASSERT_EQ(output->len, 1);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	g_array_free(output, TRUE);

	/* Groups of percents */
	output = desktop_exec_parse("foo %% \"%%\" %%%%", NULL);
	ASSERT_EQ(output->len, 4);
	ASSERT_STREQ(g_array_index(output, gchar *, 0), "foo");
	ASSERT_STREQ(g_array_index(output, gchar *, 1), "%");
	ASSERT_STREQ(g_array_index(output, gchar *, 2), "%");
	ASSERT_STREQ(g_array_index(output, gchar *, 3), "%%");
	g_array_free(output, TRUE);

	/* All the % sequences we don't support */
	output = desktop_exec_parse("foo %d %D %n %N %v %m %i %c %k", "file:///proc/version");
	ASSERT_EQ(output->len, 1);
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

TEST_F(HelperTest, SetConfinedEnvvars)
{
	DbusTestService * service = dbus_test_service_new(NULL);
	DbusTestDbusMock * mock = dbus_test_dbus_mock_new("com.ubuntu.Upstart");

	DbusTestDbusMockObject * obj = dbus_test_dbus_mock_get_object(mock, "/com/ubuntu/Upstart", "com.ubuntu.Upstart0_6", NULL);

	dbus_test_dbus_mock_object_add_method(mock, obj,
		"SetEnvMulti",
		G_VARIANT_TYPE("(asasb)"),
		NULL,
		"",
		NULL);

	dbus_test_service_add_task(service, DBUS_TEST_TASK(mock));
	dbus_test_service_start_tasks(service);

	GDBusConnection * bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	g_dbus_connection_set_exit_on_close(bus, FALSE);
	g_object_add_weak_pointer(G_OBJECT(bus), (gpointer *)&bus);

	/* Not a test other than "don't crash" */
	EnvHandle * handle = env_handle_start();
	set_confined_envvars(handle, "foo-app-pkg", "/foo/bar");
	env_handle_finish(handle);

	guint len = 0;
	const DbusTestDbusMockCall * calls = dbus_test_dbus_mock_object_get_method_calls(mock, obj, "SetEnvMulti", &len, NULL);

	ASSERT_EQ(len, 1);
	ASSERT_NE(calls, nullptr);
	ASSERT_STREQ("SetEnvMulti", calls[0].name);

	unsigned int i;

	bool got_app_isolation = false;
	bool got_cache_home = false;
	bool got_config_home = false;
	bool got_data_home = false;
	bool got_runtime_dir = false;
	bool got_data_dirs = false;
	bool got_temp_dir = false;
	bool got_shader_dir = false;

	GVariant * envarray = g_variant_get_child_value(calls[0].params, 1);
	GVariantIter iter;
	g_variant_iter_init(&iter, envarray);
	gchar * envvar = NULL;

	while (g_variant_iter_loop(&iter, "s", &envvar)) {
		gchar * var = g_strdup(envvar);

		gchar * equal = g_strstr_len(var, -1, "=");
		ASSERT_NE(equal, nullptr);

		equal[0] = '\0';
		gchar * value = &(equal[1]);

		if (g_strcmp0(var, "UBUNTU_APPLICATION_ISOLATION") == 0) {
			ASSERT_STREQ(value, "1");
			got_app_isolation = true;
		} else if (g_strcmp0(var, "XDG_CACHE_HOME") == 0) {
			got_cache_home = true;
		} else if (g_strcmp0(var, "XDG_CONFIG_HOME") == 0) {
			got_config_home = true;
		} else if (g_strcmp0(var, "XDG_DATA_HOME") == 0) {
			got_data_home = true;
		} else if (g_strcmp0(var, "XDG_RUNTIME_DIR") == 0) {
			got_runtime_dir = true;
		} else if (g_strcmp0(var, "XDG_DATA_DIRS") == 0) {
			ASSERT_TRUE(g_str_has_prefix(value, "/foo/bar:"));
			got_data_dirs = true;
		} else if (g_strcmp0(var, "TMPDIR") == 0) {
			ASSERT_TRUE(g_str_has_suffix(value, "foo-app-pkg"));
			got_temp_dir = true;
		} else if (g_strcmp0(var, "__GL_SHADER_DISK_CACHE_PATH") == 0) {
			ASSERT_TRUE(g_str_has_suffix(value, "foo-app-pkg"));
			got_shader_dir = true;
		} else {
			g_warning("Unknown variable! %s", var);
			ASSERT_TRUE(false);
		}

		g_free(var);
	}

	g_variant_unref(envarray);

	ASSERT_TRUE(got_app_isolation);
	ASSERT_TRUE(got_cache_home);
	ASSERT_TRUE(got_config_home);
	ASSERT_TRUE(got_data_home);
	ASSERT_TRUE(got_runtime_dir);
	ASSERT_TRUE(got_data_dirs);
	ASSERT_TRUE(got_temp_dir);
	ASSERT_TRUE(got_shader_dir);

	g_object_unref(bus);
	g_object_unref(mock);
	g_object_unref(service);

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
	ASSERT_STREQ(desktop, CMAKE_SOURCE_DIR "/click-app-dir/application.desktop");
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
