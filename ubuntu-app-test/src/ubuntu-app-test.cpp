/*
 * Copyright © 2015 Canonical Ltd.
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

#include <gio/gio.h>
#include "ubuntu-app-launch.h"

#include <mir_toolkit/mir_connection.h>
#include <mir_toolkit/mir_prompt_session.h>

#include <sys/wait.h>

#include <future>

gboolean
timeout (gpointer ploop)
{
	g_main_loop_quit((GMainLoop *)ploop);
	return G_SOURCE_REMOVE;
}

void
app_failed (const gchar * appid, UbuntuAppLaunchAppFailed failure_type, gpointer ploop)
{
	if (g_strcmp0(appid, "ubuntu-app-test") != 0) {
		return;
	}

	g_warning("Starting 'ubuntu-app-test' failed with error: %d", failure_type);

	g_main_loop_quit((GMainLoop *)ploop);
}

void
fd_getter (MirPromptSession * session, size_t count, int const * fdsin, void * pfds)
{
	auto promise = reinterpret_cast<std::promise<int> *>(pfds);

	if (count != 1) {
		g_warning("Didn't get the right number of FDs");
		promise->set_value(0);
		return;
	}

	promise->set_value(fdsin[0]);
}

int
main (int argc, char * argv[])
{
	if (argc == 1) {
		g_printerr("Usage: %s <command you want to execute>\n", argv[0]);
		return -1;
	}

	gchar * mirpath = g_build_filename(g_get_user_runtime_dir(), "mir_socket_trusted", (gchar *)NULL);
	if (!g_file_test(mirpath, G_FILE_TEST_EXISTS)) {
		g_free(mirpath);
		g_debug("No Mir detected, exec'ing assuming we're under X11");
		return execvp(argv[1], argv + 1);
	}

	GMainLoop * loop = g_main_loop_new(NULL, FALSE);

	ubuntu_app_launch_observer_add_app_failed(app_failed, loop);

	ubuntu_app_launch_start_application("ubuntu-app-test", NULL);

	g_timeout_add_seconds(1, timeout, loop);

	g_main_loop_run(loop);

	ubuntu_app_launch_observer_delete_app_failed(app_failed, loop);

	g_main_loop_unref(loop);

	GPid pid;
	pid = ubuntu_app_launch_get_primary_pid("ubuntu-app-test");

	if (pid == 0) {
		g_critical("Unable to get PID for 'ubuntu-app-test' application");
		return -1;
	}

	MirConnection * mir = mir_connect_sync(mirpath, "ubuntu-app-test");
	g_free(mirpath);

	MirPromptSession * session = mir_connection_create_prompt_session_sync(mir, pid, NULL, NULL);

	std::promise<int> fdpromise;
	mir_prompt_session_new_fds_for_prompt_providers(session, 1, fd_getter, &fdpromise);

	auto fd = fdpromise.get_future().get(); 

	if (fd == 0) {
		g_critical("Unable to get FD for prompt session");
		return -1;
	}

	gchar * sock = g_strdup_printf("fd://%d", fd);
	g_setenv("MIR_SOCKET", sock, TRUE);
	g_free(sock);

	pid_t subpid = 0;
	int exit_status = -1;
	if ((subpid = fork()) == 0) {
		return execvp(argv[1], argv + 1);
	}
	waitpid(subpid, &exit_status, 0);

	mir_prompt_session_release_sync(session);
	mir_connection_release(mir);

	return WEXITSTATUS(exit_status);
}
