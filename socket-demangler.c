/*
 * Copyright © 2014 Canonical Ltd.
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
 *   Ted Gould <ted.gould@canonical.com>
 */

#define _POSIX_C_SOURCE 200112L

#include <gio/gio.h>
#include <gio/gunixfdlist.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

int
main (int argc, char * argv[])
{
	const gchar * mir_name =   g_getenv("UBUNTU_APP_LAUNCH_DEMANGLE_NAME");
	const gchar * mir_socket = g_getenv("UBUNTU_APP_LAUNCH_DEMANGLE_PATH");
	if (mir_socket == NULL || mir_socket[0] == '\0') {
		g_error("Unable to find Mir path for service");
		return -1;
	}
	if (mir_name == NULL || mir_name[0] == '\0') {
		g_error("Unable to find Mir name for service");
		return -1;
	}

	GError * error = NULL;
	GDBusConnection * bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);

	if (error != NULL) {
		g_error("Unable to get session bus: %s", error->message);
		g_error_free(error);
		return -1;
	}

	GVariant * retval;
	GUnixFDList * fdlist;

	retval = g_dbus_connection_call_with_unix_fd_list_sync(
		bus,
		mir_name,
		mir_socket,
		"com.canonical.UbuntuAppLaunch.SocketDemangler",
		"GetMirSocket",
		NULL,
		G_VARIANT_TYPE("(h)"),
		G_DBUS_CALL_FLAGS_NO_AUTO_START,
		-1, /* timeout */
		NULL, /* fd list in */
		&fdlist,
		NULL, /* cancelable */
		&error);

	g_clear_object(&bus);

	if (error != NULL) {
		g_error("Unable to get Mir socket over dbus: %s", error->message);
		g_error_free(error);
		return -1;
	}

	GVariant * outhandle = g_variant_get_child_value(retval, 0);

	if (outhandle == NULL) {
		g_error("Unable to get data from function");
		return -1;
	}

	gint32 handle = g_variant_get_handle(outhandle);
	g_variant_unref(outhandle);
	g_variant_unref(retval);

	if (handle >= g_unix_fd_list_get_length(fdlist)) {
		g_error("Handle is %d but the FD list only has %d entries", handle, g_unix_fd_list_get_length(fdlist));
		g_clear_object(&fdlist);
		return -1;
	}

	gint32 fd = g_unix_fd_list_get(fdlist, handle, &error);
	g_clear_object(&fdlist);

	if (error != NULL) {
		g_error("Unable to Unix FD: %s", error->message);
		g_error_free(error);
		return -1;
	}

	errno = 0;
	fcntl(fd, F_GETFD);
	if (errno != 0) {
		perror("File descriptor is invalid");
		return -1;
	}

	/* Make sure the FD doesn't close on exec */
	fcntl(fd, F_SETFD, 0);

	gchar * mirsocketbuf = g_strdup_printf("fd://%d", fd);
	setenv("MIR_SOCKET", mirsocketbuf, 1);

	g_free(mirsocketbuf);

	/* Thought, is argv NULL terminated? */
	return execvp(argv[1], argv + 1);
}
