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
 *     Ted Gould <ted.gould@canonical.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusMessage>
#include <QObject>

static int set_oom_adj(const int pidval, const int oomval, const unsigned int calleruid)
{
	/* Not we turn the pid into an integer and back so that we can ensure we don't
	   get used for nefarious tasks. */
	if ((pidval < 1) || (pidval >= 32768)) {
		fprintf(stderr, "PID passed is invalid: %d\n", pidval);
		return EXIT_FAILURE;
	}

	/* Not we turn the oom value into an integer and back so that we can ensure we don't
	   get used for nefarious tasks. */
	if ((oomval < -1000) || (oomval >= 1000)) {
		fprintf(stderr, "OOM Value passed is invalid: %d\n", oomval);
		return EXIT_FAILURE;
	}

	/* Open up the PID directory first, to ensure that it is actually one of
	   ours, so that we can't be used to set a OOM value on just anything */
	char pidpath[32];
	snprintf(pidpath, sizeof(pidpath), "/proc/%d", pidval);

	int piddir = open(pidpath, O_RDONLY | O_DIRECTORY);
	if (piddir < 0) {
		fprintf(stderr, "Unable open PID directory '%s' for '%d': %s\n", pidpath, pidval, strerror(errno));
		return EXIT_FAILURE;
	}

	struct stat piddirstat = {0};
	if (fstat(piddir, &piddirstat) < 0) {
		close(piddir);
		fprintf(stderr, "Unable stat PID directory '%s' for '%d': %s\n", pidpath, pidval, strerror(errno));
		return EXIT_FAILURE;
	}

	if (calleruid != piddirstat.st_uid) {
		close(piddir);
		fprintf(stderr, "PID directory '%s' is not owned by %d but by %d\n", pidpath, calleruid, piddirstat.st_uid);
		return EXIT_FAILURE;
	}

	/* Looks good, let's try to get the actual oom_adj_score file to write
	   the value to it. */
	int adj = openat(piddir, "oom_score_adj", O_WRONLY);
	int openerr = errno;

	if (adj < 0) {
		close(piddir);

		/* ENOENT happens a fair amount because of races, so it's not
		   worth printing a warning about */
		if (openerr != ENOENT) {
			fprintf(stderr, "Unable to set OOM value of '%d' on '%d': %s\n", oomval, pidval, strerror(openerr));
			return EXIT_FAILURE;
		} else {
			return EXIT_SUCCESS;
		}
	}

	char oomstring[32];
	snprintf(oomstring, sizeof(oomstring), "%d", oomval);

	size_t writesize = write(adj, oomstring, strlen(oomstring));
	int writeerr = errno;

	close(adj);
	close(piddir);

	if (writesize == strlen(oomstring))
		return EXIT_SUCCESS;
	
	if (writeerr != 0)
		fprintf(stderr, "Unable to set OOM value of '%d' on '%d': %s\n", oomval, pidval, strerror(writeerr));
	else
		/* No error, but yet, wrong size. Not sure, what could cause this. */
		fprintf(stderr, "Unable to set OOM value of '%d' on '%d': Wrote %d bytes\n", oomval, pidval, (int)writesize);

	return EXIT_FAILURE;
}

#define DBUS_SERVICE "com.ubports.oom-adjust-helper"
#define DBUS_PATH "/"
#define DBUS_INTERFACE "com.ubports.oom-adjust-helper"

class DBusHandler : public QObject, protected QDBusContext
{
	Q_OBJECT
	Q_CLASSINFO("D-Bus Interface", DBUS_INTERFACE)

public Q_SLOTS:
	void setOomValue(const int pid, const int oomval)
	{
		const unsigned int calleruid = connection().interface()->serviceUid(message().service());
		set_oom_adj(pid, oomval, calleruid);
	}
};

#include "oom-adjust-helper.moc"

int main (int argc, char * argv[])
{
	QCoreApplication app(argc, argv);
	DBusHandler dbusHandler;

	// We only need one instance
	if (!QDBusConnection::systemBus().registerService(DBUS_SERVICE)) {
		fprintf(stderr, "Failed to register service '%s'\n", DBUS_SERVICE);
		exit(EXIT_FAILURE);
	}

	if (!QDBusConnection::systemBus().registerObject(DBUS_PATH, &dbusHandler, QDBusConnection::ExportAllSlots)) {
		fprintf(stderr, "Failed to register DBus object at path '%s'\n", DBUS_PATH);
		exit(EXIT_FAILURE);
	}

	return app.exec();
}
