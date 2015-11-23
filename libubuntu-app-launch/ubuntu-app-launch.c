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

#include "ubuntu-app-launch.h"
#include <upstart.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <zeitgeist.h>

#include "ubuntu-app-launch-trace.h"
#include "second-exec-core.h"
#include "helpers.h"
#include "ual-tracepoint.h"
#include "click-exec.h"
#include "desktop-exec.h"
#include "recoverable-problem.h"
#include "proxy-socket-demangler.h"
#include "app-info.h"

static void apps_for_job (GDBusConnection * con, const gchar * name, GArray * apps, gboolean truncate_legacy);
static void free_helper (gpointer value);
static GList * pids_for_appid (const gchar * appid);
int kill (pid_t pid, int signal);
static gchar * escape_dbus_string (const gchar * input);

G_DEFINE_QUARK(UBUNTU_APP_LAUNCH_PROXY_PATH, proxy_path);
G_DEFINE_QUARK(UBUNTU_APP_LAUNCH_MIR_FD, mir_fd);

/* Function to take the urls and escape them so that they can be
   parsed on the other side correctly. */
static gchar *
app_uris_string (const gchar * const * uris)
{
	guint i = 0;
	GArray * array = g_array_new(TRUE, TRUE, sizeof(gchar *));
	g_array_set_clear_func(array, free_helper);

	for (i = 0; i < g_strv_length((gchar**)uris); i++) {
		gchar * escaped = g_shell_quote(uris[i]);
		g_array_append_val(array, escaped);
	}

	gchar * urisjoin = g_strjoinv(" ", (gchar**)array->data);
	g_array_unref(array);

	return urisjoin;
}

typedef struct {
	gchar * appid;
	gchar * uris;
} app_start_t;

static void
application_start_cb (GObject * obj, GAsyncResult * res, gpointer user_data)
{
	app_start_t * data = (app_start_t *)user_data;
	GError * error = NULL;
	GVariant * result = NULL;

	ual_tracepoint(libual_start_message_callback, data->appid);

	g_debug("Started Message Callback: %s", data->appid);

	result = g_dbus_connection_call_finish(G_DBUS_CONNECTION(obj), res, &error);

	if (result != NULL)
		g_variant_unref(result);
	
	if (error != NULL) {
		if (g_dbus_error_is_remote_error(error)) {
			gchar * remote_error = g_dbus_error_get_remote_error(error);
			g_debug("Remote error: %s", remote_error);
			if (g_strcmp0(remote_error, "com.ubuntu.Upstart0_6.Error.AlreadyStarted") == 0) {
				second_exec(data->appid, data->uris);
			}

			g_free(remote_error);
		} else {
			g_warning("Unable to emit event to start application: %s", error->message);
		}
		g_error_free(error);
	}

	g_free(data->appid);
	g_free(data->uris);
	g_free(data);
}

/* Get the path of the job from Upstart, if we've got it already, we'll just
   use the cache of the value */
static const gchar *
get_jobpath (GDBusConnection * con, const gchar * jobname)
{
	gchar * cachepath = g_strdup_printf("ubuntu-app-lauch-job-path-cache-%s", jobname);
	gpointer cachedata = g_object_get_data(G_OBJECT(con), cachepath);

	if (cachedata != NULL) {
		g_free(cachepath);
		return cachedata;
	}

	GError * error = NULL;
	GVariant * job_path_variant = g_dbus_connection_call_sync(con,
		DBUS_SERVICE_UPSTART,
		DBUS_PATH_UPSTART,
		DBUS_INTERFACE_UPSTART,
		"GetJobByName",
		g_variant_new("(s)", jobname),
		G_VARIANT_TYPE("(o)"),
		G_DBUS_CALL_FLAGS_NONE,
		-1, /* timeout: default */
		NULL, /* cancelable */
		&error);

	if (error != NULL) {	
		g_warning("Unable to find job '%s': %s", jobname, error->message);
		g_error_free(error);
		g_free(cachepath);
		return NULL;
	}

	gchar * job_path = NULL;
	g_variant_get(job_path_variant, "(o)", &job_path);
	g_variant_unref(job_path_variant);

	g_object_set_data_full(G_OBJECT(con), cachepath, job_path, g_free);
	g_free(cachepath);

	return job_path;
}

/* Check to see if a legacy app wants us to manage whether they're
   single instance or not */
static gboolean
legacy_single_instance (const gchar * appid)
{
	ual_tracepoint(desktop_single_start, appid);

	GKeyFile * keyfile = keyfile_for_appid(appid, NULL);

	if (keyfile == NULL) {
		g_warning("Unable to find keyfile for application '%s'", appid);
		return FALSE;
	}

	ual_tracepoint(desktop_single_found, appid);

	gboolean singleinstance = FALSE;

	if (g_key_file_has_key(keyfile, "Desktop Entry", "X-Ubuntu-Single-Instance", NULL)) {
		GError * error = NULL;

		singleinstance = g_key_file_get_boolean(keyfile, "Desktop Entry", "X-Ubuntu-Single-Instance", &error);

		if (error != NULL) {
			g_warning("Unable to get single instance key for app '%s': %s", appid, error->message);
			g_error_free(error);
			/* Ensure that if we got an error, we assume standard case */
			singleinstance = FALSE;
		}
	}
	
	g_key_file_free(keyfile);

	ual_tracepoint(desktop_single_finished, appid, singleinstance ? "single" : "unmanaged");

	return singleinstance;
}

/* Determine whether it's a click package by looking for the symlink
   that is created by the desktop hook */
static gboolean
is_click (const gchar * appid)
{
	gchar * appiddesktop = g_strdup_printf("%s.desktop", appid);
	gchar * click_link = NULL;
	const gchar * link_farm_dir = g_getenv("UBUNTU_APP_LAUNCH_LINK_FARM");
	if (G_LIKELY(link_farm_dir == NULL)) {
		click_link = g_build_filename(g_get_home_dir(), ".cache", "ubuntu-app-launch", "desktop", appiddesktop, NULL);
	} else {
		click_link = g_build_filename(link_farm_dir, appiddesktop, NULL);
	}
	g_free(appiddesktop);
	gboolean click = g_file_test(click_link, G_FILE_TEST_EXISTS);
	g_free(click_link);

	return click;
}

/* Determine whether an AppId is realated to a Libertine container by
   checking the container and program name. */
static gboolean
is_libertine (const gchar * appid)
{
	if (app_info_libertine(appid, NULL, NULL)) {
		g_debug("Libertine application detected: %s", appid);
		return TRUE;
	} else {
		return FALSE;
	}
}

static gboolean
start_application_core (const gchar * appid, const gchar * const * uris, gboolean test)
{
	ual_tracepoint(libual_start, appid);

	g_return_val_if_fail(appid != NULL, FALSE);

	GDBusConnection * con = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	g_return_val_if_fail(con != NULL, FALSE);

	gboolean click = is_click(appid);
	ual_tracepoint(libual_determine_type, appid, click ? "click" : "legacy");

	/* Figure out if it is libertine */
	gboolean libertine = FALSE;
	if (!click) {
		libertine = is_libertine(appid);
	}

	ual_tracepoint(libual_determine_libertine, appid, libertine ? "container" : "host");

	/* Figure out the DBus path for the job */
	const gchar * jobpath = NULL;
	if (click) {
		jobpath = get_jobpath(con, "application-click");
	} else {
		jobpath = get_jobpath(con, "application-legacy");
	}

	if (jobpath == NULL) {
		g_object_unref(con);
		g_warning("Unable to get job path");
		return FALSE;
	}

	ual_tracepoint(libual_job_path_determined, appid, jobpath);

	/* Callback data */
	app_start_t * app_start_data = g_new0(app_start_t, 1);
	app_start_data->appid = g_strdup(appid);

	/* Build up our environment */
	GVariantBuilder builder;
	g_variant_builder_init(&builder, G_VARIANT_TYPE_TUPLE);

	g_variant_builder_open(&builder, G_VARIANT_TYPE_ARRAY);

	g_variant_builder_add_value(&builder, g_variant_new_take_string(g_strdup_printf("APP_ID=%s", appid)));
	g_variant_builder_add_value(&builder, g_variant_new_take_string(g_strdup_printf("APP_LAUNCHER_PID=%d", getpid())));

	if (uris != NULL) {
		gchar * urisjoin = app_uris_string(uris);
		gchar * urienv = g_strdup_printf("APP_URIS=%s", urisjoin);
		app_start_data->uris = urisjoin;
		g_variant_builder_add_value(&builder, g_variant_new_take_string(urienv));
	}

	if (!click) {
		if (libertine || legacy_single_instance(appid)) {
			g_variant_builder_add_value(&builder, g_variant_new_string("INSTANCE_ID="));
		} else {
			gchar * instanceid = g_strdup_printf("INSTANCE_ID=%" G_GUINT64_FORMAT, g_get_real_time());
			g_variant_builder_add_value(&builder, g_variant_new_take_string(instanceid));
		}
	}

	if (test) {
		g_variant_builder_add_value(&builder, g_variant_new_string("QT_LOAD_TESTABILITY=1"));
	}

	gboolean setup_complete = FALSE;
	if (click) {
		setup_complete = click_task_setup(con, appid, (EnvHandle*)&builder);
	} else {
		setup_complete = desktop_task_setup(con, appid, (EnvHandle*)&builder, libertine);
	}

	if (setup_complete) {
		g_variant_builder_close(&builder);
		g_variant_builder_add_value(&builder, g_variant_new_boolean(TRUE));
	
		/* Call the job start function */
		g_dbus_connection_call(con,
		                       DBUS_SERVICE_UPSTART,
		                       jobpath,
		                       DBUS_INTERFACE_UPSTART_JOB,
		                       "Start",
		                       g_variant_builder_end(&builder),
		                       NULL,
		                       G_DBUS_CALL_FLAGS_NONE,
		                       -1,
		                       NULL, /* cancelable */
		                       application_start_cb,
		                       app_start_data);

		ual_tracepoint(libual_start_message_sent, appid);
	} else {
		g_variant_builder_clear(&builder);
	}

	g_object_unref(con);

	return setup_complete;
}

gboolean
ubuntu_app_launch_start_application (const gchar * appid, const gchar * const * uris)
{
	return start_application_core(appid, uris, FALSE);
}

gboolean
ubuntu_app_launch_start_application_test (const gchar * appid, const gchar * const * uris)
{
	return start_application_core(appid, uris, TRUE);
}

static void
stop_job (GDBusConnection * con, const gchar * jobname, const gchar * appname, const gchar * instanceid)
{
	g_debug("Stopping job %s app_id %s instance_id %s", jobname, appname, instanceid);

	const gchar * job_path = get_jobpath(con, jobname);
	if (job_path == NULL)
		return;

	GVariantBuilder builder;
	g_variant_builder_init(&builder, G_VARIANT_TYPE_TUPLE);
	g_variant_builder_open(&builder, G_VARIANT_TYPE_ARRAY);

	g_variant_builder_add_value(&builder,
		g_variant_new_take_string(g_strdup_printf("APP_ID=%s", appname)));
	
	if (instanceid != NULL) {
		g_variant_builder_add_value(&builder,
			g_variant_new_take_string(g_strdup_printf("INSTANCE_ID=%s", instanceid)));
	}

	g_variant_builder_close(&builder);
	g_variant_builder_add_value(&builder, g_variant_new_boolean(FALSE)); /* wait */

	GError * error = NULL;
	GVariant * stop_variant = g_dbus_connection_call_sync(con,
		DBUS_SERVICE_UPSTART,
		job_path,
		DBUS_INTERFACE_UPSTART_JOB,
		"Stop",
		g_variant_builder_end(&builder),
		NULL,
		G_DBUS_CALL_FLAGS_NONE,
		-1, /* timeout: default */
		NULL, /* cancelable */
		&error);

	if (error != NULL) {
		g_warning("Unable to stop job %s app_id %s instance_id %s: %s", jobname, appname, instanceid, error->message);
		g_error_free(error);
	}

	g_variant_unref(stop_variant);
}

static void
free_helper (gpointer value)
{
	gchar ** strp = (gchar **)value;
	g_free(*strp);
}

gboolean
ubuntu_app_launch_stop_application (const gchar * appid)
{
	g_return_val_if_fail(appid != NULL, FALSE);

	GDBusConnection * con = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	g_return_val_if_fail(con != NULL, FALSE);

	gboolean found = FALSE;
	int i;

	GArray * apps = g_array_new(TRUE, TRUE, sizeof(gchar *));
	g_array_set_clear_func(apps, free_helper);

	/* Look through the click jobs and see if any match.  There can
	   only be one instance for each ID in the click world */
	apps_for_job(con, "application-click", apps, FALSE);
	for (i = 0; i < apps->len; i++) {
		const gchar * array_id = g_array_index(apps, const gchar *, i);
		if (g_strcmp0(array_id, appid) == 0) {
			stop_job(con, "application-click", appid, NULL);
			found = TRUE;
			break; /* There can be only one with click */
		}
	}

	if (apps->len > 0)
		g_array_remove_range(apps, 0, apps->len);

	/* Look through the legacy apps.  Trickier because we know that there
	   can be many instances of the legacy jobs out there, so we might
	   have to kill more than one of them. */
	apps_for_job(con, "application-legacy", apps, FALSE);
	gchar * appiddash = g_strdup_printf("%s-", appid); /* Probably could go RegEx here, but let's start with just a prefix lookup */
	for (i = 0; i < apps->len; i++) {
		const gchar * array_id = g_array_index(apps, const gchar *, i);
		if (g_str_has_prefix(array_id, appiddash)) {
			gchar * instanceid = g_strrstr(array_id, "-");
			stop_job(con, "application-legacy", appid, &(instanceid[1]));
			found = TRUE;
		}
	}
	g_free(appiddash);

	g_array_free(apps, TRUE);
	g_object_unref(con);

	return found;
}

/* Set the OOM value using the helper as an async task */
static gboolean
use_oom_helper (GPid pid, const gchar * oomscore)
{
	GError * error = NULL;
	const gchar * args[4] = {
		OOM_HELPER,
		NULL, /* pid */
		oomscore,
		NULL
	};
	gchar * pidstr = g_strdup_printf("%d", pid);
	args[1] = pidstr;

	g_spawn_async(NULL, /* working dir */
		(gchar **)args,
		NULL, /* env */
		G_SPAWN_DEFAULT,
		NULL, NULL, /* child setup */
		NULL, /* pid */
		&error); /* error */

	g_free(pidstr);

	if (error != NULL) {
		g_warning("Unable to launch OOM helper '" OOM_HELPER "' on PID '%d': %s", pid, error->message);
		g_error_free(error);
		return FALSE;
	}

	return TRUE;
}

/* Sets the OOM score to a particular value, returns true on NULL */
static gboolean
set_oom_value (GPid pid, const gchar * oomscore)
{
	static const gchar * procpath = NULL;
	if (G_UNLIKELY(procpath == NULL)) {
		/* Set by the test suite, probably not anyone else */
		procpath = g_getenv("UBUNTU_APP_LAUNCH_OOM_PROC_PATH");
		if (G_LIKELY(procpath == NULL)) {
			procpath = "/proc";
		}
	}

	gchar * path = g_strdup_printf("%s/%d/oom_score_adj", procpath, pid);
	FILE * adj = fopen(path, "w");
	int openerr = errno;
	g_free(path);

	if (adj == NULL) {
		switch (openerr) {
		case ENOENT:
			/* ENOENT happens a fair amount because of races, so it's not
			   worth printing a warning about */
			return TRUE;
		case EACCES: {
			/* We can get this error when trying to set the OOM value on
			   Oxide renderers because they're started by the sandbox and
			   don't have their adjustment value available for us to write.
			   We have a helper to deal with this, but it's kinda expensive
			   so we only use it when we have to. */
			return use_oom_helper(pid, oomscore);
		}
		default:
			g_warning("Unable to set OOM value for '%d' to '%s': %s", pid, oomscore, strerror(openerr));
			return FALSE;
		}
	}

	size_t writesize = fwrite(oomscore, 1, strlen(oomscore), adj);
	int writeerr = errno;
	fclose(adj);

	if (writesize == strlen(oomscore))
		return TRUE;
	
	if (writeerr != 0)
		g_warning("Unable to set OOM value for '%d' to '%s': %s", pid, oomscore, strerror(writeerr));
	else
		/* No error, but yet, wrong size. Not sure, what could cause this. */
		g_debug("Unable to set OOM value for '%d' to '%s': Wrote %d bytes", pid, oomscore, (int)writesize);

	return FALSE;
}

/* Throw out a DBus signal that we've signalled all of these processes. This
   is the fun GVariant building part. */
static void
notify_signalling (GList * pids, const gchar * appid, const gchar * signal_name)
{
	GDBusConnection * conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	if (conn == NULL) {
		return;
	}

	/* Pull together a PID array */
	GVariant *pidarray = NULL;
	if (pids == NULL) {
		pidarray = g_variant_new_array(G_VARIANT_TYPE_UINT64, NULL, 0);
	} else {
		GList * i;
		GVariantBuilder builder;
		g_variant_builder_init(&builder, G_VARIANT_TYPE_ARRAY);

		for (i = pids; i != NULL; i = g_list_next(i))
			g_variant_builder_add_value(&builder, g_variant_new_uint64(GPOINTER_TO_INT(i->data)));

		pidarray = g_variant_builder_end(&builder);
	}

	/* Combine into the wrapping tuple */
	GVariantBuilder btuple;
	g_variant_builder_init(&btuple, G_VARIANT_TYPE_TUPLE);
	g_variant_builder_add_value(&btuple, g_variant_new_string(appid));
	g_variant_builder_add_value(&btuple, pidarray);

	/* Emit !!! */
	GError * error = NULL;
	g_dbus_connection_emit_signal(conn,
		NULL, /* destination */
		"/", /* path */
		"com.canonical.UbuntuAppLaunch", /* interface */
		signal_name, /* signal */
		g_variant_builder_end(&btuple), /* params, the same */
		&error);

	if (error != NULL) {
		g_warning("Unable to emit signal '%s' for appid '%s': %s", signal_name, appid, error->message);
		g_error_free(error);
	} else {
		g_debug("Emmitted '%s' to DBus", signal_name);
	}

	g_object_unref(conn);
}

/* Gets all the pids for an appid and sends a signal to all of them. This also
   loops to ensure no new pids are added while we're signaling */
static gboolean
signal_to_cgroup (const gchar * appid, int signal, const gchar * oomscore, const gchar * signal_name)
{
	GHashTable * pidssignaled = g_hash_table_new(g_direct_hash, g_direct_equal);
	guint hash_table_size = 0;
	gboolean retval = TRUE;

	do {
		hash_table_size = g_hash_table_size(pidssignaled);
		GList * pidlist = pids_for_appid(appid);
		GList * iter;

		if (pidlist == NULL) {
			g_warning("Unable to get pids for '%s' to send signal %d", appid, signal);
			retval = FALSE;
			break;
		}

		for (iter = pidlist; iter != NULL; iter = g_list_next(iter)) {
			if (g_hash_table_contains(pidssignaled, iter->data)) {
				continue;
			}

			/* We've got a PID that we've not previously signaled */
			GPid pid = GPOINTER_TO_INT(iter->data);
			if (-1 == kill(pid, signal)) {
				/* While that didn't work, we still want to try as many as we can */
				g_warning("Unable to send signal %d to pid %d", signal, pid);
				retval = FALSE;
			}

			if (!set_oom_value(pid, oomscore)) {
				retval = FALSE;
			}

			g_hash_table_add(pidssignaled, iter->data);
		}

		g_list_free(pidlist);
	/* If it grew, then try again */
	} while (hash_table_size != g_hash_table_size(pidssignaled));

	notify_signalling(g_hash_table_get_keys(pidssignaled), appid, signal_name);
	g_hash_table_destroy(pidssignaled);

	return retval;
}

/* Mostly here to just print a warning if we can't submit the event, they're
   not critical to have */
static void
zg_insert_complete (GObject * obj, GAsyncResult * res, gpointer user_data)
{
	GError * error = NULL;
	GArray * result = NULL;

	result = zeitgeist_log_insert_event_finish(ZEITGEIST_LOG(obj), res, &error);

	if (error != NULL) {
		g_warning("Unable to submit Zeitgeist Event: %s", error->message);
		g_error_free(error);
	}

	g_array_free(result, TRUE);
	return;
}

/* Function to report the access and leaving events to Zeitgeist so we
   can track application usage */
static void
report_zg_event (const gchar * appid, const gchar * eventtype)
{
	gchar * uri = NULL;
	gchar * pkg = NULL;
	gchar * app = NULL;

	if (ubuntu_app_launch_app_id_parse(appid, &pkg, &app, NULL)) {
		/* If it's parseable, use the short form */
		uri = g_strdup_printf("application://%s_%s.desktop", pkg, app);
		g_free(pkg);
		g_free(app);
	} else {
		uri = g_strdup_printf("application://%s.desktop", appid);
	}

	ZeitgeistLog * log = zeitgeist_log_get_default();

	ZeitgeistEvent * event = zeitgeist_event_new();
	zeitgeist_event_set_actor(event, "application://ubuntu-app-launch.desktop");
	zeitgeist_event_set_interpretation(event, eventtype);
	zeitgeist_event_set_manifestation(event, ZEITGEIST_ZG_USER_ACTIVITY);

	ZeitgeistSubject * subject = zeitgeist_subject_new();
	zeitgeist_subject_set_interpretation(subject, ZEITGEIST_NFO_SOFTWARE);
	zeitgeist_subject_set_manifestation(subject, ZEITGEIST_NFO_SOFTWARE_ITEM);
	zeitgeist_subject_set_mimetype(subject, "application/x-desktop");
	zeitgeist_subject_set_uri(subject, uri);

	zeitgeist_event_add_subject(event, subject);

	zeitgeist_log_insert_event(log, event, NULL, zg_insert_complete, NULL);

	g_free(uri);
	g_object_unref(log);
	g_object_unref(event);
	g_object_unref(subject);
}

gboolean
ubuntu_app_launch_pause_application (const gchar * appid)
{
	report_zg_event(appid, ZEITGEIST_ZG_LEAVE_EVENT);
	return signal_to_cgroup(appid, SIGSTOP, "900", "ApplicationPaused");
}

gboolean
ubuntu_app_launch_resume_application (const gchar * appid)
{
	report_zg_event(appid, ZEITGEIST_ZG_ACCESS_EVENT);
	return signal_to_cgroup(appid, SIGCONT, "100", "ApplicationResumed");
}

gchar *
ubuntu_app_launch_application_log_path (const gchar * appid)
{
	gchar * path = NULL;
	g_return_val_if_fail(appid != NULL, NULL);

	if (is_click(appid)) {
		gchar * appfile = g_strdup_printf("application-click-%s.log", appid);
		path =  g_build_filename(g_get_user_cache_dir(), "upstart", appfile, NULL);
		g_free(appfile);
		return path;
	}

	if (!is_libertine(appid) && legacy_single_instance(appid)) {
		gchar * appfile = g_strdup_printf("application-legacy-%s-.log", appid);
		path =  g_build_filename(g_get_user_cache_dir(), "upstart", appfile, NULL);
		g_free(appfile);
		return path;
	}

	/* If we're not single instance, we can't recreate the instance ID
	   but if it's running we can grab it. */
	unsigned int i;
	GDBusConnection * con = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	g_return_val_if_fail(con != NULL, NULL);

	GArray * apps = g_array_new(TRUE, TRUE, sizeof(gchar *));
	g_array_set_clear_func(apps, free_helper);

	apps_for_job(con, "application-legacy", apps, FALSE);
	gchar * appiddash = g_strdup_printf("%s-", appid); /* Probably could go RegEx here, but let's start with just a prefix lookup */
	for (i = 0; i < apps->len && path == NULL; i++) {
		const gchar * array_id = g_array_index(apps, const gchar *, i);
		if (g_str_has_prefix(array_id, appiddash)) {
			gchar * appfile = g_strdup_printf("application-legacy-%s.log", array_id);
			path =  g_build_filename(g_get_user_cache_dir(), "upstart", appfile, NULL);
			g_free(appfile);
		}
	}
	g_free(appiddash);

	g_array_free(apps, TRUE);
	g_object_unref(con);

	return path;
}

gboolean
ubuntu_app_launch_application_info (const gchar * appid, gchar ** appdir, gchar ** appdesktop)
{
	if (is_click(appid)) {
		return app_info_click(appid, appdir, appdesktop);
	} else if (is_libertine(appid)) {
		return app_info_libertine(appid, appdir, appdesktop);
	} else {
		return app_info_legacy(appid, appdir, appdesktop);
	}
}

static GDBusConnection *
gdbus_upstart_ref (void) {
	static GDBusConnection * gdbus_upstart = NULL;

	if (gdbus_upstart != NULL) {
		return g_object_ref(gdbus_upstart);
	}

	GError * error = NULL;
	gdbus_upstart = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);

	if (error != NULL) {
		g_warning("Unable to connect to Upstart bus: %s", error->message);
		g_error_free(error);
		return NULL;
	}

	g_object_add_weak_pointer(G_OBJECT(gdbus_upstart), (gpointer)&gdbus_upstart);

	return gdbus_upstart;
}

/* The data we keep for each observer */
typedef struct _observer_t observer_t;
struct _observer_t {
	GDBusConnection * conn;
	guint sighandle;
	UbuntuAppLaunchAppObserver func;
	gpointer user_data;
};

/* The data we keep for each failed observer */
typedef struct _failed_observer_t failed_observer_t;
struct _failed_observer_t {
	GDBusConnection * conn;
	guint sighandle;
	UbuntuAppLaunchAppFailedObserver func;
	gpointer user_data;
};

/* The data we keep for each failed observer */
typedef struct _paused_resumed_observer_t paused_resumed_observer_t;
struct _paused_resumed_observer_t {
	GDBusConnection * conn;
	guint sighandle;
	UbuntuAppLaunchAppPausedResumedObserver func;
	gpointer user_data;
	const gchar * lttng_signal;
};

/* The lists of Observers */
static GList * starting_array = NULL;
static GList * started_array = NULL;
static GList * stop_array = NULL;
static GList * focus_array = NULL;
static GList * resume_array = NULL;
static GList * failed_array = NULL;
static GList * paused_array = NULL;
static GList * resumed_array = NULL;

static void
observer_cb (GDBusConnection * conn, const gchar * sender, const gchar * object, const gchar * interface, const gchar * signal, GVariant * params, gpointer user_data)
{
	observer_t * observer = (observer_t *)user_data;

	const gchar * signalname = NULL;
	g_variant_get_child(params, 0, "&s", &signalname);

	ual_tracepoint(observer_start, signalname);

	gchar * env = NULL;
	GVariant * envs = g_variant_get_child_value(params, 1);
	GVariantIter iter;
	g_variant_iter_init(&iter, envs);

	gboolean job_found = FALSE;
	gboolean job_legacy = FALSE;
	gchar * instance = NULL;

	while (g_variant_iter_loop(&iter, "s", &env)) {
		if (g_strcmp0(env, "JOB=application-click") == 0) {
			job_found = TRUE;
		} else if (g_strcmp0(env, "JOB=application-legacy") == 0) {
			job_found = TRUE;
			job_legacy = TRUE;
		} else if (g_str_has_prefix(env, "INSTANCE=")) {
			instance = g_strdup(env + strlen("INSTANCE="));
		}
	}

	g_variant_unref(envs);

	if (job_legacy && instance != NULL) {
		gchar * dash = g_strrstr(instance, "-");
		if (dash != NULL) {
			dash[0] = '\0';
		}
	}

	if (job_found && instance != NULL) {
		observer->func(instance, observer->user_data);
	}

	ual_tracepoint(observer_finish, signalname);

	g_free(instance);
}

/* Creates the observer structure and registers for the signal with
   GDBus so that we can get a callback */
static gboolean
add_app_generic (UbuntuAppLaunchAppObserver observer, gpointer user_data, const gchar * signal, GList ** list)
{
	GDBusConnection * conn = gdbus_upstart_ref();

	if (conn == NULL) {
		return FALSE;
	}

	observer_t * observert = g_new0(observer_t, 1);

	observert->conn = conn;
	observert->func = observer;
	observert->user_data = user_data;

	*list = g_list_prepend(*list, observert);

	observert->sighandle = g_dbus_connection_signal_subscribe(conn,
		NULL, /* sender */
		DBUS_INTERFACE_UPSTART, /* interface */
		"EventEmitted", /* signal */
		DBUS_PATH_UPSTART, /* path */
		signal, /* arg0 */
		G_DBUS_SIGNAL_FLAGS_NONE,
		observer_cb,
		observert,
		NULL); /* user data destroy */

	return TRUE;
}

gboolean
ubuntu_app_launch_observer_add_app_started (UbuntuAppLaunchAppObserver observer, gpointer user_data)
{
	return add_app_generic(observer, user_data, "started", &started_array);
}

gboolean
ubuntu_app_launch_observer_add_app_stop (UbuntuAppLaunchAppObserver observer, gpointer user_data)
{
	return add_app_generic(observer, user_data, "stopped", &stop_array);
}

/* Creates the observer structure and registers for the signal with
   GDBus so that we can get a callback */
static gboolean
add_session_generic (UbuntuAppLaunchAppObserver observer, gpointer user_data, const gchar * signal, GList ** list, GDBusSignalCallback session_cb)
{
	GDBusConnection * conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);

	if (conn == NULL) {
		return FALSE;
	}

	observer_t * observert = g_new0(observer_t, 1);

	observert->conn = conn;
	observert->func = observer;
	observert->user_data = user_data;

	*list = g_list_prepend(*list, observert);

	observert->sighandle = g_dbus_connection_signal_subscribe(conn,
		NULL, /* sender */
		"com.canonical.UbuntuAppLaunch", /* interface */
		signal, /* signal */
		"/", /* path */
		NULL, /* arg0 */
		G_DBUS_SIGNAL_FLAGS_NONE,
		session_cb,
		observert,
		NULL); /* user data destroy */

	return TRUE;
}

/* Generic handler for a bunch of our signals */
static inline void
generic_signal_cb (GDBusConnection * conn, const gchar * sender, const gchar * object, const gchar * interface, const gchar * signal, GVariant * params, gpointer user_data)
{
	observer_t * observer = (observer_t *)user_data;
	const gchar * appid = NULL;

	if (observer->func != NULL) {
		g_variant_get(params, "(&s)", &appid);
		observer->func(appid, observer->user_data);
	}
}

/* Handle the focus signal when it occurs, call the observer */
static void
focus_signal_cb (GDBusConnection * conn, const gchar * sender, const gchar * object, const gchar * interface, const gchar * signal, GVariant * params, gpointer user_data)
{
	ual_tracepoint(observer_start, "focus");

	generic_signal_cb(conn, sender, object, interface, signal, params, user_data);

	ual_tracepoint(observer_finish, "focus");
}

gboolean
ubuntu_app_launch_observer_add_app_focus (UbuntuAppLaunchAppObserver observer, gpointer user_data)
{
	return add_session_generic(observer, user_data, "UnityFocusRequest", &focus_array, focus_signal_cb);
}

/* Handle the resume signal when it occurs, call the observer, then send a signal back when we're done */
static void
resume_signal_cb (GDBusConnection * conn, const gchar * sender, const gchar * object, const gchar * interface, const gchar * signal, GVariant * params, gpointer user_data)
{
	ual_tracepoint(observer_start, "resume");

	generic_signal_cb(conn, sender, object, interface, signal, params, user_data);

	GError * error = NULL;
	g_dbus_connection_emit_signal(conn,
		sender, /* destination */
		"/", /* path */
		"com.canonical.UbuntuAppLaunch", /* interface */
		"UnityResumeResponse", /* signal */
		params, /* params, the same */
		&error);

	if (error != NULL) {
		g_warning("Unable to emit response signal: %s", error->message);
		g_error_free(error);
	}

	ual_tracepoint(observer_finish, "resume");
}

gboolean
ubuntu_app_launch_observer_add_app_resume (UbuntuAppLaunchAppObserver observer, gpointer user_data)
{
	return add_session_generic(observer, user_data, "UnityResumeRequest", &resume_array, resume_signal_cb);
}

/* Handle the starting signal when it occurs, call the observer, then send a signal back when we're done */
static void
starting_signal_cb (GDBusConnection * conn, const gchar * sender, const gchar * object, const gchar * interface, const gchar * signal, GVariant * params, gpointer user_data)
{
	ual_tracepoint(observer_start, "starting");

	generic_signal_cb(conn, sender, object, interface, signal, params, user_data);

	GError * error = NULL;
	g_dbus_connection_emit_signal(conn,
		sender, /* destination */
		"/", /* path */
		"com.canonical.UbuntuAppLaunch", /* interface */
		"UnityStartingSignal", /* signal */
		params, /* params, the same */
		&error);

	if (error != NULL) {
		g_warning("Unable to emit response signal: %s", error->message);
		g_error_free(error);
	}

	ual_tracepoint(observer_finish, "starting");
}

gboolean
ubuntu_app_launch_observer_add_app_starting (UbuntuAppLaunchAppObserver observer, gpointer user_data)
{
	return add_session_generic(observer, user_data, "UnityStartingBroadcast", &starting_array, starting_signal_cb);
}

/* Handle the failed signal when it occurs, call the observer */
static void
failed_signal_cb (GDBusConnection * conn, const gchar * sender, const gchar * object, const gchar * interface, const gchar * signal, GVariant * params, gpointer user_data)
{
	failed_observer_t * observer = (failed_observer_t *)user_data;
	const gchar * appid = NULL;
	const gchar * typestr = NULL;

	ual_tracepoint(observer_start, "failed");

	if (observer->func != NULL) {
		UbuntuAppLaunchAppFailed type = UBUNTU_APP_LAUNCH_APP_FAILED_CRASH;
		g_variant_get(params, "(&s&s)", &appid, &typestr);

		if (g_strcmp0("crash", typestr) == 0) {
			type = UBUNTU_APP_LAUNCH_APP_FAILED_CRASH;
		} else if (g_strcmp0("start-failure", typestr) == 0) {
			type = UBUNTU_APP_LAUNCH_APP_FAILED_START_FAILURE;
		} else {
			g_warning("Application failure type '%s' unknown, reporting as a crash", typestr);
		}

		observer->func(appid, type, observer->user_data);
	}

	ual_tracepoint(observer_finish, "failed");
}

gboolean
ubuntu_app_launch_observer_add_app_failed (UbuntuAppLaunchAppFailedObserver observer, gpointer user_data)
{
	GDBusConnection * conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);

	if (conn == NULL) {
		return FALSE;
	}

	failed_observer_t * observert = g_new0(failed_observer_t, 1);

	observert->conn = conn;
	observert->func = observer;
	observert->user_data = user_data;

	failed_array = g_list_prepend(failed_array, observert);

	observert->sighandle = g_dbus_connection_signal_subscribe(conn,
		NULL, /* sender */
		"com.canonical.UbuntuAppLaunch", /* interface */
		"ApplicationFailed", /* signal */
		"/", /* path */
		NULL, /* arg0 */
		G_DBUS_SIGNAL_FLAGS_NONE,
		failed_signal_cb,
		observert,
		NULL); /* user data destroy */

	return TRUE;
}

/* Handle the paused signal when it occurs, call the observer */
static void
paused_signal_cb (GDBusConnection * conn, const gchar * sender, const gchar * object, const gchar * interface, const gchar * signal, GVariant * params, gpointer user_data)
{
	paused_resumed_observer_t * observer = (paused_resumed_observer_t *)user_data;

	ual_tracepoint(observer_start, observer->lttng_signal);

	if (observer->func != NULL) {
		GArray * pidarray = g_array_new(TRUE, TRUE, sizeof(GPid));
		GVariant * appid = g_variant_get_child_value(params, 0);
		GVariant * pids = g_variant_get_child_value(params, 1);
		guint64 pid;
		GVariantIter thispid;
		g_variant_iter_init(&thispid, pids);

		while (g_variant_iter_loop(&thispid, "t", &pid)) {
			GPid gpid = (GPid)pid; /* Should be a no-op for most architectures, but just in case */
			g_array_append_val(pidarray, gpid);
		}

		observer->func(g_variant_get_string(appid, NULL), (GPid *)pidarray->data, observer->user_data);

		g_array_free(pidarray, TRUE);
		g_variant_unref(appid);
		g_variant_unref(pids);
	}

	ual_tracepoint(observer_finish, observer->lttng_signal);
}

static gboolean
paused_resumed_generic (UbuntuAppLaunchAppPausedResumedObserver observer, gpointer user_data, GList ** queue, const gchar * signal_name, const gchar * lttng_signal)
{
	GDBusConnection * conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);

	if (conn == NULL) {
		return FALSE;
	}

	paused_resumed_observer_t * observert = g_new0(paused_resumed_observer_t, 1);

	observert->conn = conn;
	observert->func = observer;
	observert->user_data = user_data;
	observert->lttng_signal = lttng_signal;

	*queue = g_list_prepend(*queue, observert);

	observert->sighandle = g_dbus_connection_signal_subscribe(conn,
		NULL, /* sender */
		"com.canonical.UbuntuAppLaunch", /* interface */
		signal_name, /* signal */
		"/", /* path */
		NULL, /* arg0 */
		G_DBUS_SIGNAL_FLAGS_NONE,
		paused_signal_cb,
		observert,
		NULL); /* user data destroy */

	return TRUE;
}

gboolean
ubuntu_app_launch_observer_add_app_paused (UbuntuAppLaunchAppPausedResumedObserver observer, gpointer user_data)
{
	return paused_resumed_generic(observer, user_data, &paused_array, "ApplicationPaused", "paused");
}

gboolean
ubuntu_app_launch_observer_add_app_resumed (UbuntuAppLaunchAppPausedResumedObserver observer, gpointer user_data)
{
	return paused_resumed_generic(observer, user_data, &resumed_array, "ApplicationResumed", "resumed");
}

static gboolean
delete_app_generic (UbuntuAppLaunchAppObserver observer, gpointer user_data, GList ** list)
{
	observer_t * observert = NULL;
	GList * look;

	for (look = *list; look != NULL; look = g_list_next(look)) {
		observert = (observer_t *)look->data;

		if (observert->func == observer && observert->user_data == user_data) {
			break;
		}
	}

	if (look == NULL) {
		return FALSE;
	}

	g_dbus_connection_signal_unsubscribe(observert->conn, observert->sighandle);
	g_object_unref(observert->conn);

	g_free(observert);
	*list = g_list_delete_link(*list, look);

	return TRUE;
}

gboolean
ubuntu_app_launch_observer_delete_app_started (UbuntuAppLaunchAppObserver observer, gpointer user_data)
{
	return delete_app_generic(observer, user_data, &started_array);
}

gboolean
ubuntu_app_launch_observer_delete_app_stop (UbuntuAppLaunchAppObserver observer, gpointer user_data)
{
	return delete_app_generic(observer, user_data, &stop_array);
}

gboolean
ubuntu_app_launch_observer_delete_app_resume (UbuntuAppLaunchAppObserver observer, gpointer user_data)
{
	return delete_app_generic(observer, user_data, &resume_array);
}

gboolean
ubuntu_app_launch_observer_delete_app_focus (UbuntuAppLaunchAppObserver observer, gpointer user_data)
{
	return delete_app_generic(observer, user_data, &focus_array);
}

gboolean
ubuntu_app_launch_observer_delete_app_starting (UbuntuAppLaunchAppObserver observer, gpointer user_data)
{
	return delete_app_generic(observer, user_data, &starting_array);
}

gboolean
ubuntu_app_launch_observer_delete_app_failed (UbuntuAppLaunchAppFailedObserver observer, gpointer user_data)
{
	failed_observer_t * observert = NULL;
	GList * look;

	for (look = failed_array; look != NULL; look = g_list_next(look)) {
		observert = (failed_observer_t *)look->data;

		if (observert->func == observer && observert->user_data == user_data) {
			break;
		}
	}

	if (look == NULL) {
		return FALSE;
	}

	g_dbus_connection_signal_unsubscribe(observert->conn, observert->sighandle);
	g_object_unref(observert->conn);

	g_free(observert);
	failed_array = g_list_delete_link(failed_array, look);

	return TRUE;
}

static gboolean
paused_resumed_delete (UbuntuAppLaunchAppPausedResumedObserver observer, gpointer user_data, GList ** list)
{
	paused_resumed_observer_t * observert = NULL;
	GList * look;

	for (look = *list; look != NULL; look = g_list_next(look)) {
		observert = (paused_resumed_observer_t *)look->data;

		if (observert->func == observer && observert->user_data == user_data) {
			break;
		}
	}

	if (look == NULL) {
		return FALSE;
	}

	g_dbus_connection_signal_unsubscribe(observert->conn, observert->sighandle);
	g_object_unref(observert->conn);

	g_free(observert);
	*list = g_list_delete_link(*list, look);

	return TRUE;
}

gboolean
ubuntu_app_launch_observer_delete_app_paused (UbuntuAppLaunchAppPausedResumedObserver observer, gpointer user_data)
{
	return paused_resumed_delete(observer, user_data, &paused_array);
}

gboolean
ubuntu_app_launch_observer_delete_app_resumed (UbuntuAppLaunchAppPausedResumedObserver observer, gpointer user_data)
{
	return paused_resumed_delete(observer, user_data, &resumed_array);
}

typedef void (*per_instance_func_t) (GDBusConnection * con, GVariant * prop_dict, gpointer user_data);

static void
foreach_job_instance (GDBusConnection * con, const gchar * jobname, per_instance_func_t func, gpointer user_data)
{
	const gchar * job_path = get_jobpath(con, jobname);
	if (job_path == NULL)
		return;

	GError * error = NULL;
	GVariant * instance_tuple = g_dbus_connection_call_sync(con,
		DBUS_SERVICE_UPSTART,
		job_path,
		DBUS_INTERFACE_UPSTART_JOB,
		"GetAllInstances",
		NULL,
		G_VARIANT_TYPE("(ao)"),
		G_DBUS_CALL_FLAGS_NONE,
		-1, /* timeout: default */
		NULL, /* cancelable */
		&error);

	if (error != NULL) {
		g_warning("Unable to get instances of job '%s': %s", jobname, error->message);
		g_error_free(error);
		return;
	}

	GVariant * instance_list = g_variant_get_child_value(instance_tuple, 0);
	g_variant_unref(instance_tuple);

	GVariantIter instance_iter;
	g_variant_iter_init(&instance_iter, instance_list);
	const gchar * instance_path = NULL;

	while (g_variant_iter_loop(&instance_iter, "&o", &instance_path)) {
		GVariant * props_tuple = g_dbus_connection_call_sync(con,
			DBUS_SERVICE_UPSTART,
			instance_path,
			"org.freedesktop.DBus.Properties",
			"GetAll",
			g_variant_new("(s)", DBUS_INTERFACE_UPSTART_INSTANCE),
			G_VARIANT_TYPE("(a{sv})"),
			G_DBUS_CALL_FLAGS_NONE,
			-1, /* timeout: default */
			NULL, /* cancelable */
			&error);

		if (error != NULL) {
			g_warning("Unable to name of instance '%s': %s", instance_path, error->message);
			g_error_free(error);
			error = NULL;
			continue;
		}

		GVariant * props_dict = g_variant_get_child_value(props_tuple, 0);

		func(con, props_dict, user_data);

		g_variant_unref(props_dict);
		g_variant_unref(props_tuple);

	}

	g_variant_unref(instance_list);
}

typedef struct {
	GArray * apps;
	gboolean truncate_legacy;
	const gchar * jobname;
} apps_for_job_t;

static void
apps_for_job_instance (GDBusConnection * con, GVariant * props_dict, gpointer user_data)
{
	GVariant * namev = g_variant_lookup_value(props_dict, "name", G_VARIANT_TYPE_STRING);
	if (namev == NULL) {
		return;
	}

	apps_for_job_t * data = (apps_for_job_t *)user_data;
	gchar * instance_name = g_variant_dup_string(namev, NULL);
	g_variant_unref(namev);

	if (data->truncate_legacy && g_strcmp0(data->jobname, "application-legacy") == 0) {
		gchar * last_dash = g_strrstr(instance_name, "-");
		if (last_dash != NULL) {
			last_dash[0] = '\0';
		}
	}

	g_array_append_val(data->apps, instance_name);
}

/* Get all the instances for a given job name */
static void
apps_for_job (GDBusConnection * con, const gchar * jobname, GArray * apps, gboolean truncate_legacy)
{
	apps_for_job_t data = {
		.jobname = jobname,
		.apps = apps,
		.truncate_legacy = truncate_legacy
	};

	foreach_job_instance(con, jobname, apps_for_job_instance, &data);
}

gchar **
ubuntu_app_launch_list_running_apps (void)
{
	GDBusConnection * con = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	g_return_val_if_fail(con != NULL, g_new0(gchar *, 1));

	GArray * apps = g_array_new(TRUE, TRUE, sizeof(gchar *));

	apps_for_job(con, "application-legacy", apps, TRUE);
	apps_for_job(con, "application-click", apps, FALSE);

	g_object_unref(con);

	return (gchar **)g_array_free(apps, FALSE);
}

typedef struct {
	GPid pid;
	const gchar * appid;
	const gchar * jobname;
} pid_for_job_t;

static void
pid_for_job_instance (GDBusConnection * con, GVariant * props_dict, gpointer user_data)
{
	GVariant * namev = g_variant_lookup_value(props_dict, "name", G_VARIANT_TYPE_STRING);
	if (namev == NULL) {
		return;
	}

	pid_for_job_t * data = (pid_for_job_t *)user_data;
	gchar * instance_name = g_variant_dup_string(namev, NULL);
	g_variant_unref(namev);

	if (g_strcmp0(data->jobname, "application-legacy") == 0) {
		gchar * last_dash = g_strrstr(instance_name, "-");
		if (last_dash != NULL) {
			last_dash[0] = '\0';
		}
	}

	if (g_strcmp0(instance_name, data->appid) == 0) {
		GVariant * processv = g_variant_lookup_value(props_dict, "processes", G_VARIANT_TYPE("a(si)"));

		if (processv != NULL) {
			if (g_variant_n_children(processv) > 0) {
				GVariant * first_entry = g_variant_get_child_value(processv, 0);
				GVariant * pidv = g_variant_get_child_value(first_entry, 1);

				data->pid = g_variant_get_int32(pidv);

				g_variant_unref(pidv);
				g_variant_unref(first_entry);
			}

			g_variant_unref(processv);
		}
	}

	g_free(instance_name);
}

/* Look for the app for a job */
static GPid
pid_for_job (GDBusConnection * con, const gchar * jobname, const gchar * appid)
{
	pid_for_job_t data = {
		.jobname = jobname,
		.appid = appid,
		.pid = 0
	};

	foreach_job_instance(con, jobname, pid_for_job_instance, &data);

	return data.pid;
}

GPid
ubuntu_app_launch_get_primary_pid (const gchar * appid)
{
	g_return_val_if_fail(appid != NULL, 0);

	GDBusConnection * con = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	g_return_val_if_fail(con != NULL, 0);

	GPid pid = 0;

	if (pid == 0) {
		pid = pid_for_job(con, "application-legacy", appid);
	}

	if (pid == 0) {
		pid = pid_for_job(con, "application-click", appid);
	}

	g_object_unref(con);

	return pid;
}

/* Get the PIDs for an AppID. If it's click or legacy single instance that's
   a simple call to the helper. But if it's not, we have to make a call for
   each instance of the app that we have running. */
static GList *
pids_for_appid (const gchar * appid)
{
	ual_tracepoint(pids_list_start, appid);

	GDBusConnection * cgmanager = cgroup_manager_connection();
	g_return_val_if_fail(cgmanager != NULL, NULL);

	ual_tracepoint(pids_list_connected, appid);

	if (is_click(appid)) {
		GList * pids = pids_from_cgroup(cgmanager, "application-click", appid);
		cgroup_manager_unref(cgmanager);

		ual_tracepoint(pids_list_finished, appid, g_list_length(pids));
		return pids;
	} else if (!is_libertine(appid) && legacy_single_instance(appid)) {
		gchar * jobname = g_strdup_printf("%s-", appid);
		GList * pids = pids_from_cgroup(cgmanager, "application-legacy", jobname);
		g_free(jobname);
		cgroup_manager_unref(cgmanager);

		ual_tracepoint(pids_list_finished, appid, g_list_length(pids));
		return pids;
	}

	/* If we're not single instance, we need to find all the pids for all
	   the instances of the app */
	unsigned int i;
	GDBusConnection * con = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	g_return_val_if_fail(con != NULL, NULL);

	GList * pids = NULL;

	GArray * apps = g_array_new(TRUE, TRUE, sizeof(gchar *));
	g_array_set_clear_func(apps, free_helper);

	apps_for_job(con, "application-legacy", apps, FALSE);
	gchar * appiddash = g_strdup_printf("%s-", appid); /* Probably could go RegEx here, but let's start with just a prefix lookup */
	for (i = 0; i < apps->len; i++) {
		const gchar * array_id = g_array_index(apps, const gchar *, i);
		if (g_str_has_prefix(array_id, appiddash)) {
			GList * morepids = pids_from_cgroup(cgmanager, "application-legacy", array_id);
			pids = g_list_concat(pids, morepids);
		}
	}
	g_free(appiddash);

	g_array_free(apps, TRUE);
	g_object_unref(con);

	cgroup_manager_unref(cgmanager);

	ual_tracepoint(pids_list_finished, appid, g_list_length(pids));
	return pids;
}

gboolean
ubuntu_app_launch_pid_in_app_id (GPid pid, const gchar * appid)
{
	g_return_val_if_fail(appid != NULL, FALSE);

	if (pid == 0) {
		return FALSE;
	}

	GList * pidlist = pids_for_appid(appid);
	GList * head;

	for (head = pidlist; head != NULL; head = g_list_next(head)) {
		GPid checkpid = GPOINTER_TO_INT(head->data);
		if (checkpid == pid) {
			g_list_free(pidlist);
			return TRUE;
		}
	}

	g_list_free(pidlist);
	return FALSE;
}

gboolean
ubuntu_app_launch_app_id_parse (const gchar * appid, gchar ** package, gchar ** application, gchar ** version)
{
	g_return_val_if_fail(appid != NULL, FALSE);

	/* 'Parse' the App ID */
	gchar ** app_id_segments = g_strsplit(appid, "_", 4);
	if (g_strv_length(app_id_segments) != 3) {
		g_debug("Unable to parse Application ID: %s", appid);
		g_strfreev(app_id_segments);
		return FALSE;
	}

	if (package != NULL) {
		*package = app_id_segments[0];
	} else {
		g_free(app_id_segments[0]);
	}

	if (application != NULL) {
		*application = app_id_segments[1];
	} else {
		g_free(app_id_segments[1]);
	}

	if (version != NULL) {
		*version = app_id_segments[2];
	} else {
		g_free(app_id_segments[2]);
	}

	g_free(app_id_segments);
	return TRUE;
}

/* Figure out whether we're a libertine container app or a click and then
   choose which function to use */
gchar *
ubuntu_app_launch_triplet_to_app_id (const gchar * pkg, const gchar * app, const gchar * ver)
{
	g_return_val_if_fail(pkg != NULL, NULL);

	/* Check if is a libertine container */
	gchar * libertinepath = g_build_filename(g_get_user_cache_dir(), "libertine-container", pkg, NULL);
	gboolean libcontainer = g_file_test(libertinepath, G_FILE_TEST_EXISTS);
	g_free(libertinepath);

	if (libcontainer) {
		return libertine_triplet_to_app_id(pkg, app, ver);
	} else {
		return click_triplet_to_app_id(pkg, app, ver);
	}
}

/* Print an error if we couldn't start it */
static void
start_helper_callback (GObject * obj, GAsyncResult * res, gpointer user_data)
{
	GError * error = NULL;
	GVariant * result = NULL;

	result = g_dbus_connection_call_finish(G_DBUS_CONNECTION(obj), res, &error);
	if (result != NULL)
		g_variant_unref(result);

	if (error != NULL) {
		g_warning("Unable to start helper: %s", error->message);
		g_error_free(error);
	}
}

/* Implements sending the "start" command to Upstart for the
   untrusted helper job with the various configuration options
   to define the instance.  In the end there's only one job with
   an array of instances. */
static gboolean
start_helper_core (const gchar * type, const gchar * appid, const gchar * const * uris, const gchar * instance, const gchar * mirsocketpath)
{
	GDBusConnection * con = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	g_return_val_if_fail(con != NULL, FALSE);

	const gchar * jobpath = get_jobpath(con, "untrusted-helper");

	/* Build up our environment */
	GVariantBuilder builder;
	g_variant_builder_init(&builder, G_VARIANT_TYPE_TUPLE);
	g_variant_builder_open(&builder, G_VARIANT_TYPE_ARRAY);
	g_variant_builder_add_value(&builder, g_variant_new_take_string(g_strdup_printf("APP_ID=%s", appid)));
	g_variant_builder_add_value(&builder, g_variant_new_take_string(g_strdup_printf("HELPER_TYPE=%s", type)));

	if (uris != NULL) {
		gchar * urisjoin = app_uris_string(uris);
		g_variant_builder_add_value(&builder, g_variant_new_take_string(g_strdup_printf("APP_URIS=%s", urisjoin)));
		g_free(urisjoin);
	}

	if (instance != NULL) {
		g_variant_builder_add_value(&builder, g_variant_new_take_string(g_strdup_printf("INSTANCE_ID=%s", instance)));
	}

	if (mirsocketpath != NULL) {
		g_variant_builder_add_value(&builder, g_variant_new_take_string(g_strdup_printf("UBUNTU_APP_LAUNCH_DEMANGLE_PATH=%s", mirsocketpath)));
		g_variant_builder_add_value(&builder, g_variant_new_take_string(g_strdup_printf("UBUNTU_APP_LAUNCH_DEMANGLE_NAME=%s", g_dbus_connection_get_unique_name(con))));
	}

	g_variant_builder_close(&builder);
	g_variant_builder_add_value(&builder, g_variant_new_boolean(TRUE));
	
	/* Call the job start function */
	g_dbus_connection_call(con,
	                       DBUS_SERVICE_UPSTART,
	                       jobpath,
	                       DBUS_INTERFACE_UPSTART_JOB,
	                       "Start",
	                       g_variant_builder_end(&builder),
	                       NULL,
	                       G_DBUS_CALL_FLAGS_NONE,
	                       -1,
	                       NULL, /* cancelable */
	                       start_helper_callback,
	                       NULL);

	g_object_unref(con);

	return TRUE;
}

gboolean
ubuntu_app_launch_start_helper (const gchar * type, const gchar * appid, const gchar * const * uris)
{
	g_return_val_if_fail(type != NULL, FALSE);
	g_return_val_if_fail(appid != NULL, FALSE);
	g_return_val_if_fail(g_strstr_len(type, -1, ":") == NULL, FALSE);

	return start_helper_core(type, appid, uris, NULL, NULL);
}

gchar *
ubuntu_app_launch_start_multiple_helper (const gchar * type, const gchar * appid, const gchar * const * uris)
{
	g_return_val_if_fail(type != NULL, NULL);
	g_return_val_if_fail(appid != NULL, NULL);
	g_return_val_if_fail(g_strstr_len(type, -1, ":") == NULL, NULL);

	gchar * instanceid = g_strdup_printf("%" G_GUINT64_FORMAT, g_get_real_time());

	if (start_helper_core(type, appid, uris, instanceid, NULL)) {
		return instanceid;
	}

	g_free(instanceid);
	return NULL;
}

/* Transfer from Mir's data structure to ours */
static void
get_mir_session_fd_helper (MirPromptSession * session, size_t count, int const * fdin, void * user_data)
{
	if (count != 1) {
		g_warning("Mir trusted session returned %d FDs instead of one", (int)count);
		return;
	}

	int * retfd = (int *)user_data;
	*retfd = fdin[0];
}

/* Setup to get the FD from Mir, blocking */
static int
get_mir_session_fd (MirPromptSession * session)
{
	int retfd = 0;
	MirWaitHandle * wait = mir_prompt_session_new_fds_for_prompt_providers(session,
		1,
		get_mir_session_fd_helper,
		&retfd);

	mir_wait_for(wait);

	return retfd;
}

static GList * open_proxies = NULL;

static gint
remove_socket_path_find (gconstpointer a, gconstpointer b) 
{
	GObject * obj = (GObject *)a;
	const gchar * path = (const gchar *)b;

	gchar * objpath = g_object_get_qdata(obj, proxy_path_quark());
	
	return g_strcmp0(objpath, path);
}

/* Cleans up if we need to early */
static gboolean
remove_socket_path (const gchar * path)
{
	GList * thisproxy = g_list_find_custom(open_proxies, path, remove_socket_path_find);
	if (thisproxy == NULL)
		return FALSE;

	g_debug("Removing Mir Socket Proxy: %s", path);

	GObject * obj = G_OBJECT(thisproxy->data);
	open_proxies = g_list_delete_link(open_proxies, thisproxy);

	/* Remove ourselves from DBus if we weren't already */
	g_dbus_interface_skeleton_unexport(G_DBUS_INTERFACE_SKELETON(obj));

	/* If we still have FD, close it */
	int mirfd = GPOINTER_TO_INT(g_object_get_qdata(obj, mir_fd_quark()));
	if (mirfd != 0) {
		close(mirfd);

		/* This is actually an error, we should expect not to find
		   this here to do anything with it. */
		const gchar * props[3] = {
			"UbuntuAppLaunchProxyDbusPath",
			NULL,
			NULL
		};
		props[1] = path;
		report_recoverable_problem("ubuntu-app-launch-mir-fd-proxy", 0, TRUE, props);
	}

	g_object_unref(obj);

	return TRUE;
}

/* Small timeout function that shouldn't, in most cases, ever do anything.
   But we need it here to ensure we don't leave things on the bus */
static gboolean
proxy_timeout (gpointer user_data)
{
	const gchar * path = (const gchar *)user_data;
	remove_socket_path(path);
	return G_SOURCE_REMOVE;
}

/* Removes the whole list of proxies if they are there */
static void
proxy_cleanup_list (void)
{
	while (open_proxies) {
		GObject * obj = G_OBJECT(open_proxies->data);
		gchar * path = g_object_get_qdata(obj, proxy_path_quark());
		remove_socket_path(path);
	}
}

static gboolean
proxy_mir_socket (GObject * obj, GDBusMethodInvocation * invocation, gpointer user_data)
{
	g_debug("Called to give Mir socket");
	int fd = GPOINTER_TO_INT(user_data);

	if (fd == 0) {
		g_critical("No FDs to give!");
		return FALSE;
	}

	/* Index into fds */
	GVariant* handle = g_variant_new_handle(0);
	GVariant* tuple = g_variant_new_tuple(&handle, 1);

	GError* error = NULL;
	GUnixFDList* list = g_unix_fd_list_new();
	g_unix_fd_list_append(list, fd, &error);

	if (error == NULL) {   
		g_dbus_method_invocation_return_value_with_unix_fd_list(invocation, tuple, list);
	} else {
		g_variant_ref_sink(tuple);
		g_variant_unref(tuple);
	}

	g_object_unref(list);

	if (error != NULL) {   
		g_critical("Unable to pass FD %d: %s", fd, error->message);
		g_error_free(error);
		return FALSE;
	}   

	g_object_set_qdata(obj, mir_fd_quark(), GINT_TO_POINTER(0));

	return TRUE;
}

/* Sets up the DBus proxy to send to the demangler */
static gchar *
build_proxy_socket_path (const gchar * appid, int mirfd)
{
	static gboolean final_cleanup = FALSE;
	if (!final_cleanup) {
		g_atexit(proxy_cleanup_list);
		final_cleanup = TRUE;
	}

	GError * error = NULL;
	GDBusConnection * session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
	if (error != NULL) {
		g_warning("Unable to get session bus: %s", error->message);
		g_error_free(error);
		return NULL;
	}

	/* Export an Object on DBus */
	proxySocketDemangler * skel = proxy_socket_demangler_skeleton_new();
	g_signal_connect(G_OBJECT(skel), "handle-get-mir-socket", G_CALLBACK(proxy_mir_socket), GINT_TO_POINTER(mirfd));

	gchar * encoded_appid = escape_dbus_string(appid);
	gchar * socket_name = NULL;
	/* Loop until we fine an object path that isn't taken (probably only once) */
	while (socket_name == NULL) {
		gchar* tryname = g_strdup_printf("/com/canonical/UbuntuAppLaunch/%s/%X", encoded_appid, g_random_int());
		g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(skel),
			session,
			tryname,
			&error);

		if (error == NULL) {
			socket_name = tryname;
			g_debug("Exporting Mir socket on path: %s", socket_name);
		} else {
			/* Always print the error, but if the object path is in use let's
			   not exit the loop. Let's just try again. */
			bool exitnow = (error->domain != G_DBUS_ERROR || error->code != G_DBUS_ERROR_OBJECT_PATH_IN_USE);
			g_critical("Unable to export trusted session object: %s", error->message);

			g_clear_error(&error);
			g_free(tryname);

			if (exitnow) {
				break;
			}
		}
	}
	g_free(encoded_appid);

	/* If we didn't get a socket name, we should just exit. And
	   make sure to clean up the socket. */
	if (socket_name == NULL) {   
		g_object_unref(skel);
		g_object_unref(session);
		g_critical("Unable to export object to any name");
		return NULL;
	}

	g_object_set_qdata_full(G_OBJECT(skel), proxy_path_quark(), g_strdup(socket_name), g_free);
	g_object_set_qdata(G_OBJECT(skel), mir_fd_quark(), GINT_TO_POINTER(mirfd));
	open_proxies = g_list_prepend(open_proxies, skel);

	g_timeout_add_seconds_full(G_PRIORITY_DEFAULT,
	                           2,
	                           proxy_timeout,
	                           g_strdup(socket_name),
	                           g_free);

	g_object_unref(session);

	return socket_name;
}

gchar *
ubuntu_app_launch_start_session_helper (const gchar * type, MirPromptSession * session, const gchar * appid, const gchar * const * uris)
{
	g_return_val_if_fail(type != NULL, NULL);
	g_return_val_if_fail(session != NULL, NULL);
	g_return_val_if_fail(appid != NULL, NULL);
	g_return_val_if_fail(g_strstr_len(type, -1, ":") == NULL, NULL);

	int mirfd = get_mir_session_fd(session);
	if (mirfd == 0)
		return NULL;

	gchar * socket_path = build_proxy_socket_path(appid, mirfd);
	if (socket_path == NULL) {
		close(mirfd);
		return NULL;
	}

	gchar * instanceid = g_strdup_printf("%" G_GUINT64_FORMAT, g_get_real_time());

	if (start_helper_core(type, appid, uris, instanceid, socket_path)) {
		return instanceid;
	}

	remove_socket_path(socket_path);
	g_free(socket_path);
	close(mirfd);
	g_free(instanceid);
	return NULL;
}

/* Print an error if we couldn't stop it */
static void
stop_helper_callback (GObject * obj, GAsyncResult * res, gpointer user_data)
{
	GError * error = NULL;
	GVariant * result = NULL;

	result = g_dbus_connection_call_finish(G_DBUS_CONNECTION(obj), res, &error);
	if (result != NULL)
		g_variant_unref(result);

	if (error != NULL) {
		g_warning("Unable to stop helper: %s", error->message);
		g_error_free(error);
	}
}

/* Implements the basis of sending the stop message to Upstart for
   an instance of the untrusted-helper job.  That also can have an
   instance in that we allow for random instance ids to be used for
   helpers that are not unique */
static gboolean
stop_helper_core (const gchar * type, const gchar * appid, const gchar * instanceid)
{
	GDBusConnection * con = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	g_return_val_if_fail(con != NULL, FALSE);

	const gchar * jobpath = get_jobpath(con, "untrusted-helper");

	/* Build up our environment */
	GVariantBuilder builder;
	g_variant_builder_init(&builder, G_VARIANT_TYPE_TUPLE);
	g_variant_builder_open(&builder, G_VARIANT_TYPE_ARRAY);
	g_variant_builder_add_value(&builder, g_variant_new_take_string(g_strdup_printf("APP_ID=%s", appid)));
	g_variant_builder_add_value(&builder, g_variant_new_take_string(g_strdup_printf("HELPER_TYPE=%s", type)));

	if (instanceid != NULL) {
		g_variant_builder_add_value(&builder, g_variant_new_take_string(g_strdup_printf("INSTANCE_ID=%s", instanceid)));
	}

	g_variant_builder_close(&builder);
	g_variant_builder_add_value(&builder, g_variant_new_boolean(TRUE));
	
	/* Call the job start function */
	g_dbus_connection_call(con,
	                       DBUS_SERVICE_UPSTART,
	                       jobpath,
	                       DBUS_INTERFACE_UPSTART_JOB,
	                       "Stop",
	                       g_variant_builder_end(&builder),
	                       NULL,
	                       G_DBUS_CALL_FLAGS_NONE,
	                       -1,
	                       NULL, /* cancelable */
	                       stop_helper_callback,
	                       NULL);

	g_object_unref(con);

	return TRUE;
}

gboolean
ubuntu_app_launch_stop_helper (const gchar * type, const gchar * appid)
{
	g_return_val_if_fail(type != NULL, FALSE);
	g_return_val_if_fail(appid != NULL, FALSE);
	g_return_val_if_fail(g_strstr_len(type, -1, ":") == NULL, FALSE);

	return stop_helper_core(type, appid, NULL);
}

gboolean
ubuntu_app_launch_stop_multiple_helper (const gchar * type, const gchar * appid, const gchar * instanceid)
{
	g_return_val_if_fail(type != NULL, FALSE);
	g_return_val_if_fail(appid != NULL, FALSE);
	g_return_val_if_fail(instanceid != NULL, FALSE);
	g_return_val_if_fail(g_strstr_len(type, -1, ":") == NULL, FALSE);

	return stop_helper_core(type, appid, instanceid);
}


typedef struct {
	gchar * type_prefix; /* Type with the colon sperator */
	size_t type_len;     /* Length in characters of the prefix */
	GArray * retappids;  /* Array of appids to return */
} helpers_helper_t;

/* Look at each instance and see if it matches this type, if so
   add the appid portion to the array of appids */
static void
list_helpers_helper (GDBusConnection * con, GVariant * props_dict, gpointer user_data)
{
	helpers_helper_t * data = (helpers_helper_t *)user_data;

	GVariant * namev = g_variant_lookup_value(props_dict, "name", G_VARIANT_TYPE_STRING);
	if (namev == NULL) {
		return;
	}

	const gchar * name = g_variant_get_string(namev, NULL);
	if (g_str_has_prefix(name, data->type_prefix)) {
		/* Skip the type name */
		name += data->type_len;

		/* Skip a possible instance ID */
		name = g_strstr_len(name, -1, ":");
		name++;

		/* Now copy the app id */
		gchar * appid = g_strdup(name);
		g_array_append_val(data->retappids, appid);
	}

	g_variant_unref(namev);

	return;
}

gchar **
ubuntu_app_launch_list_helpers (const gchar * type)
{
	g_return_val_if_fail(type != NULL, FALSE);
	g_return_val_if_fail(g_strstr_len(type, -1, ":") == NULL, FALSE);

	GDBusConnection * con = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	g_return_val_if_fail(con != NULL, FALSE);

	helpers_helper_t helpers_helper_data = {
		.type_prefix = g_strdup_printf("%s:", type),
		.type_len = strlen(type) + 1, /* 1 for the colon */
		.retappids = g_array_new(TRUE, TRUE, sizeof(gchar *))
	};

	foreach_job_instance(con, "untrusted-helper", list_helpers_helper, &helpers_helper_data);

	g_object_unref(con);
	g_free(helpers_helper_data.type_prefix);

	return (gchar **)g_array_free(helpers_helper_data.retappids, FALSE);
}

typedef struct {
	gchar * type_prefix; /* Type with the colon sperator */
	size_t type_len;     /* Length in characters of the prefix */
	GArray * retappids;  /* Array of appids to return */
	gchar * appid_suffix; /* The appid for the end */
} helper_instances_t;

/* Look at each instance and see if it matches this type and appid.
   If so, add the instance ID to the array of instance IDs */
static void
list_helper_instances (GDBusConnection * con, GVariant * props_dict, gpointer user_data)
{
	helper_instances_t * data = (helper_instances_t *)user_data;

	GVariant * namev = g_variant_lookup_value(props_dict, "name", G_VARIANT_TYPE_STRING);
	if (namev == NULL) {
		return;
	}

	const gchar * name = g_variant_get_string(namev, NULL);
	gchar * suffix_loc = NULL;
	if (g_str_has_prefix(name, data->type_prefix) &&
			(suffix_loc = g_strrstr(name, data->appid_suffix)) != NULL) {
		/* Skip the type name */
		name += data->type_len;

		/* Now copy the instance id */
		gchar * instanceid = g_strndup(name, suffix_loc - name);
		g_array_append_val(data->retappids, instanceid);
	}

	g_variant_unref(namev);

	return;
}

gchar **
ubuntu_app_launch_list_helper_instances (const gchar * type, const gchar * appid)
{
	g_return_val_if_fail(type != NULL, FALSE);
	g_return_val_if_fail(g_strstr_len(type, -1, ":") == NULL, FALSE);

	GDBusConnection * con = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	g_return_val_if_fail(con != NULL, FALSE);

	helper_instances_t helper_instances_data = {
		.type_prefix = g_strdup_printf("%s:", type),
		.type_len = strlen(type) + 1, /* 1 for the colon */
		.retappids = g_array_new(TRUE, TRUE, sizeof(gchar *)),
		.appid_suffix = g_strdup_printf(":%s", appid)
	};

	foreach_job_instance(con, "untrusted-helper", list_helper_instances, &helper_instances_data);

	g_object_unref(con);
	g_free(helper_instances_data.type_prefix);
	g_free(helper_instances_data.appid_suffix);

	return (gchar **)g_array_free(helper_instances_data.retappids, FALSE);
}

/* The data we keep for each observer */
typedef struct _helper_observer_t helper_observer_t;
struct _helper_observer_t {
	GDBusConnection * conn;
	guint sighandle;
	gchar * type;
	UbuntuAppLaunchHelperObserver func;
	gpointer user_data;
};

/* The lists of helper observers */
static GList * helper_started_obs = NULL;
static GList * helper_stopped_obs = NULL;

static void
helper_observer_cb (GDBusConnection * conn, const gchar * sender, const gchar * object, const gchar * interface, const gchar * signal, GVariant * params, gpointer user_data)
{
	helper_observer_t * observer = (helper_observer_t *)user_data;

	gchar * env = NULL;
	GVariant * envs = g_variant_get_child_value(params, 1);
	GVariantIter iter;
	g_variant_iter_init(&iter, envs);

	gboolean job_found = FALSE;
	gchar * instance = NULL;

	while (g_variant_iter_loop(&iter, "s", &env)) {
		if (g_strcmp0(env, "JOB=untrusted-helper") == 0) {
			job_found = TRUE;
		} else if (g_str_has_prefix(env, "INSTANCE=")) {
			instance = g_strdup(env + strlen("INSTANCE="));
		}
	}

	g_variant_unref(envs);

	if (instance != NULL && !g_str_has_prefix(instance, observer->type)) {
		g_free(instance);
		instance = NULL;
	}

	gchar * appid = NULL;
	gchar * instanceid = NULL;
	gchar * type = NULL;

	if (instance != NULL) {
		gchar ** split = g_strsplit(instance, ":", 3);
		type = split[0];
		instanceid = split[1];
		appid = split[2];
		g_free(split);
	}
	g_free(instance);

	if (instanceid != NULL && instanceid[0] == '\0') {
		g_free(instanceid);
		instanceid = NULL;
	}

	if (job_found && appid != NULL) {
		observer->func(appid, instanceid, type, observer->user_data);
	}

	g_free(appid);
	g_free(instanceid);
	g_free(type);
}

/* Creates the observer structure and registers for the signal with
   GDBus so that we can get a callback */
static gboolean
add_helper_generic (UbuntuAppLaunchHelperObserver observer, const gchar * helper_type, gpointer user_data, const gchar * signal, GList ** list)
{
	GDBusConnection * conn = gdbus_upstart_ref();

	if (conn == NULL) {
		return FALSE;
	}

	helper_observer_t * observert = g_new0(helper_observer_t, 1);

	observert->conn = conn;
	observert->func = observer;
	observert->user_data = user_data;
	observert->type = g_strdup_printf("%s:", helper_type);

	*list = g_list_prepend(*list, observert);

	observert->sighandle = g_dbus_connection_signal_subscribe(conn,
		NULL, /* sender */
		DBUS_INTERFACE_UPSTART, /* interface */
		"EventEmitted", /* signal */
		DBUS_PATH_UPSTART, /* path */
		signal, /* arg0 */
		G_DBUS_SIGNAL_FLAGS_NONE,
		helper_observer_cb,
		observert,
		NULL); /* user data destroy */

	return TRUE;
}

static gboolean
delete_helper_generic (UbuntuAppLaunchHelperObserver observer, const gchar * type, gpointer user_data, GList ** list)
{
	helper_observer_t * observert = NULL;
	GList * look;

	for (look = *list; look != NULL; look = g_list_next(look)) {
		observert = (helper_observer_t *)look->data;

		if (observert->func == observer && observert->user_data == user_data && g_str_has_prefix(observert->type, type)) {
			break;
		}
	}

	if (look == NULL) {
		return FALSE;
	}

	g_dbus_connection_signal_unsubscribe(observert->conn, observert->sighandle);
	g_object_unref(observert->conn);

	g_free(observert->type);
	g_free(observert);
	*list = g_list_delete_link(*list, look);

	return TRUE;
}

gboolean
ubuntu_app_launch_observer_add_helper_started (UbuntuAppLaunchHelperObserver observer, const gchar * helper_type, gpointer user_data)
{
	g_return_val_if_fail(observer != NULL, FALSE);
	g_return_val_if_fail(helper_type != NULL, FALSE);
	g_return_val_if_fail(g_strstr_len(helper_type, -1, ":") == NULL, FALSE);

	return add_helper_generic(observer, helper_type, user_data, "started", &helper_started_obs);
}

gboolean
ubuntu_app_launch_observer_add_helper_stop (UbuntuAppLaunchHelperObserver observer, const gchar * helper_type, gpointer user_data)
{
	g_return_val_if_fail(observer != NULL, FALSE);
	g_return_val_if_fail(helper_type != NULL, FALSE);
	g_return_val_if_fail(g_strstr_len(helper_type, -1, ":") == NULL, FALSE);

	return add_helper_generic(observer, helper_type, user_data, "stopped", &helper_stopped_obs);
}

gboolean
ubuntu_app_launch_observer_delete_helper_started (UbuntuAppLaunchHelperObserver observer, const gchar * helper_type, gpointer user_data)
{
	g_return_val_if_fail(observer != NULL, FALSE);
	g_return_val_if_fail(helper_type != NULL, FALSE);
	g_return_val_if_fail(g_strstr_len(helper_type, -1, ":") == NULL, FALSE);

	return delete_helper_generic(observer, helper_type, user_data, &helper_started_obs);
}

gboolean
ubuntu_app_launch_observer_delete_helper_stop (UbuntuAppLaunchHelperObserver observer, const gchar * helper_type, gpointer user_data)
{
	g_return_val_if_fail(observer != NULL, FALSE);
	g_return_val_if_fail(helper_type != NULL, FALSE);
	g_return_val_if_fail(g_strstr_len(helper_type, -1, ":") == NULL, FALSE);

	return delete_helper_generic(observer, helper_type, user_data, &helper_stopped_obs);
}

/* Sets an environment variable in Upstart */
static void
set_var (GDBusConnection * bus, const gchar * job_name, const gchar * instance_name, const gchar * envvar)
{
	GVariantBuilder builder; /* Target: (assb) */
	g_variant_builder_init(&builder, G_VARIANT_TYPE_TUPLE);

	/* Setup the job properties */
	g_variant_builder_open(&builder, G_VARIANT_TYPE_ARRAY);
	g_variant_builder_add_value(&builder, g_variant_new_string(job_name));
	if (instance_name != NULL)
		g_variant_builder_add_value(&builder, g_variant_new_string(instance_name));
	g_variant_builder_close(&builder);

	g_variant_builder_add_value(&builder, g_variant_new_string(envvar));

	/* Do we want to replace?  Yes, we do! */
	g_variant_builder_add_value(&builder, g_variant_new_boolean(TRUE));

	g_dbus_connection_call(bus,
		"com.ubuntu.Upstart",
		"/com/ubuntu/Upstart",
		"com.ubuntu.Upstart0_6",
		"SetEnv",
		g_variant_builder_end(&builder),
		NULL, /* reply */
		G_DBUS_CALL_FLAGS_NONE,
		-1, /* timeout */
		NULL, /* cancelable */
		NULL, NULL); /* callback */
}

gboolean
ubuntu_app_launch_helper_set_exec (const gchar * execline, const gchar * directory)
{
	g_return_val_if_fail(execline != NULL, FALSE);
	g_return_val_if_fail(execline[0] != '\0', FALSE);

	/* Check to see if we can get the job environment */
	const gchar * job_name = g_getenv("UPSTART_JOB");
	const gchar * instance_name = g_getenv("UPSTART_INSTANCE");
	const gchar * demangler = g_getenv("UBUNTU_APP_LAUNCH_DEMANGLE_NAME");
	g_return_val_if_fail(job_name != NULL, FALSE);

	GError * error = NULL;
	GDBusConnection * bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);

	if (error != NULL) {
		g_warning("Unable to get session bus: %s", error->message);
		g_error_free(error);
		return FALSE;
	}

	/* The exec value */
	gchar * envstr = NULL;
	if (demangler) {
		envstr = g_strdup_printf("APP_EXEC=%s %s", DEMANGLER_PATH, execline);
	} else {
		envstr = g_strdup_printf("APP_EXEC=%s", execline);
	}

	set_var(bus, job_name, instance_name, envstr);
	g_free(envstr);

	/* The directory value */
	if (directory != NULL) {
		gchar * direnv = g_strdup_printf("APP_DIR=%s", directory);
		set_var(bus, job_name, instance_name, direnv);
		g_free(direnv);
	}

	g_object_unref(bus);

	return TRUE;
}


/* ensure that all characters are valid in the dbus output string */
static gchar *
escape_dbus_string (const gchar * input)
{
	static const gchar *xdigits = "0123456789abcdef";
	GString *escaped;
	gchar c;

	g_return_val_if_fail (input != NULL, NULL);

	escaped = g_string_new (NULL);
	while ((c = *input++)) {
		if (g_ascii_isalnum (c)) {
			g_string_append_c (escaped, c);
		} else {
			g_string_append_c (escaped, '_');
			g_string_append_c (escaped, xdigits[c >> 4]);
			g_string_append_c (escaped, xdigits[c & 0xf]);
		}
	}

	return g_string_free (escaped, FALSE);
}

