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

extern "C" {
#include "ubuntu-app-launch.h"
#include <upstart.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <zeitgeist.h>

#include "ubuntu-app-launch-trace.h"
#include "helpers.h"
#include "ual-tracepoint.h"
#include "recoverable-problem.h"
#include "proxy-socket-demangler.h"
}

/* C++ Interface */
#include "application.h"
#include "appid.h"
#include "registry.h"
#include "registry-impl.h"

static void free_helper (gpointer value);
int kill (pid_t pid, int signal) noexcept;
static gchar * escape_dbus_string (const gchar * input);

G_DEFINE_QUARK(UBUNTU_APP_LAUNCH_PROXY_PATH, proxy_path)
G_DEFINE_QUARK(UBUNTU_APP_LAUNCH_MIR_FD, mir_fd)

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

/* Get the path of the job from Upstart, if we've got it already, we'll just
   use the cache of the value */
static const gchar *
get_jobpath (GDBusConnection * con, const gchar * jobname)
{
	gchar * cachepath = g_strdup_printf("ubuntu-app-lauch-job-path-cache-%s", jobname);
	gpointer cachedata = g_object_get_data(G_OBJECT(con), cachepath);

	if (cachedata != NULL) {
		g_free(cachepath);
		return (const gchar*)cachedata;
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
		NULL, /* cancellable */
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

gboolean
ubuntu_app_launch_start_application (const gchar * appid, const gchar * const * uris)
{
	try {
		auto registry = ubuntu::app_launch::Registry::getDefault();
		auto appId = ubuntu::app_launch::AppID::find(appid);
		auto app = ubuntu::app_launch::Application::create(appId, registry);

		std::vector<ubuntu::app_launch::Application::URL> urivect;
		for (auto i = 0; uris != nullptr && uris[i] != nullptr; i++)
			urivect.emplace_back(ubuntu::app_launch::Application::URL::from_raw(uris[i]));

		auto instance = app->launch(urivect);

		if (instance) {
			return TRUE;
		} else {
			return FALSE;
		}
	} catch (std::runtime_error &e) {
		g_warning("Unable to start app '%s': %s", appid, e.what());
		return FALSE;
	}
}

gboolean
ubuntu_app_launch_start_application_test (const gchar * appid, const gchar * const * uris)
{
	try {
		auto registry = ubuntu::app_launch::Registry::getDefault();
		auto appId = ubuntu::app_launch::AppID::find(appid);
		auto app = ubuntu::app_launch::Application::create(appId, registry);

		std::vector<ubuntu::app_launch::Application::URL> urivect;
		for (auto i = 0; uris != nullptr && uris[i] != nullptr; i++)
			urivect.emplace_back(ubuntu::app_launch::Application::URL::from_raw(uris[i]));

		auto instance = app->launchTest(urivect);

		if (instance) {
			return TRUE;
		} else {
			return FALSE;
		}
	} catch (std::runtime_error &e) {
		g_warning("Unable to start app '%s': %s", appid, e.what());
		return FALSE;
	}
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

	try {
		auto registry = ubuntu::app_launch::Registry::getDefault();
		auto appId = ubuntu::app_launch::AppID::find(appid);
		auto app = ubuntu::app_launch::Application::create(appId, registry);

		auto instances = app->instances();
		for (auto instance : instances) {
			instance->stop();
		}

		return TRUE;
	} catch (std::runtime_error &e) {
		g_warning("Unable to stop app '%s': %s", appid, e.what());
		return FALSE;
	}
}

gboolean
ubuntu_app_launch_pause_application (const gchar * appid)
{
	try {
		auto registry = ubuntu::app_launch::Registry::getDefault();
		auto appId = ubuntu::app_launch::AppID::find(appid);
		auto app = ubuntu::app_launch::Application::create(appId, registry);
		app->instances().at(0)->pause();
		return TRUE;
	} catch (...) {
		return FALSE;
	}
}

gboolean
ubuntu_app_launch_resume_application (const gchar * appid)
{
	try {
		auto registry = ubuntu::app_launch::Registry::getDefault();
		auto appId = ubuntu::app_launch::AppID::find(appid);
		auto app = ubuntu::app_launch::Application::create(appId, registry);
		app->instances().at(0)->resume();
		return TRUE;
	} catch (...) {
		return FALSE;
	}
}

gchar *
ubuntu_app_launch_application_log_path (const gchar * appid)
{
	try {
		auto registry = ubuntu::app_launch::Registry::getDefault();
		auto appId = ubuntu::app_launch::AppID::find(appid);
		auto app = ubuntu::app_launch::Application::create(appId, registry);
		auto log = app->instances()[0]->logPath();
		return g_strdup(log.c_str());
	} catch (...) {
		return nullptr;
	}
}

static GDBusConnection *
gdbus_upstart_ref (void) {
	static GDBusConnection * gdbus_upstart = NULL;

	if (gdbus_upstart != NULL) {
		return G_DBUS_CONNECTION(g_object_ref(gdbus_upstart));
	}

	GError * error = NULL;
	gdbus_upstart = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);

	if (error != NULL) {
		g_warning("Unable to connect to Upstart bus: %s", error->message);
		g_error_free(error);
		return NULL;
	}

	g_object_add_weak_pointer(G_OBJECT(gdbus_upstart), (void **)(&gdbus_upstart));

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
		} else if (g_strcmp0(env, "JOB=application-snap") == 0) {
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
	ubuntu::app_launch::Registry::Impl::watchingAppStarting(true);
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
	ubuntu::app_launch::Registry::Impl::watchingAppStarting(false);
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
		NULL, /* cancellable */
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
			NULL, /* cancellable */
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

gchar **
ubuntu_app_launch_list_running_apps (void)
{
	try {
		GArray * apps = g_array_new(TRUE, TRUE, sizeof(gchar *));
		for (auto app : ubuntu::app_launch::Registry::runningApps()) {
			std::string appid = app->appId();
			g_debug("Adding AppID to list: %s", appid.c_str());
			gchar * gappid = g_strdup(appid.c_str());
			g_array_append_val(apps, gappid);
		}

		return (gchar **)g_array_free(apps, FALSE);
	} catch (...) {
		return nullptr;
	}
}

GPid
ubuntu_app_launch_get_primary_pid (const gchar * appid)
{
	g_return_val_if_fail(appid != NULL, 0);

	try {
		auto registry = ubuntu::app_launch::Registry::getDefault();
		auto appId = ubuntu::app_launch::AppID::find(appid);
		auto app = ubuntu::app_launch::Application::create(appId, registry);
		return app->instances().at(0)->primaryPid();
	} catch (...) {
		return 0;
	}
}

/* Get the PIDs for an AppID. If it's click or legacy single instance that's
   a simple call to the helper. But if it's not, we have to make a call for
   each instance of the app that we have running. */
GList *
ubuntu_app_launch_get_pids (const gchar * appid)
{
	try {
		auto registry = ubuntu::app_launch::Registry::getDefault();
		auto appId = ubuntu::app_launch::AppID::find(appid);
		auto app = ubuntu::app_launch::Application::create(appId, registry);
		auto pids = app->instances().at(0)->pids();

		GList * retval = nullptr;
		for (auto pid : pids) {
			retval = g_list_prepend(retval, GINT_TO_POINTER(pid));
		}

		return retval;
	} catch (...) {
		return nullptr;
	}
}

gboolean
ubuntu_app_launch_pid_in_app_id (GPid pid, const gchar * appid)
{
	g_return_val_if_fail(appid != NULL, FALSE);
	try {
		auto registry = ubuntu::app_launch::Registry::getDefault();
		auto appId = ubuntu::app_launch::AppID::find(appid);
		auto app = ubuntu::app_launch::Application::create(appId, registry);

		if (app->instances().at(0)->hasPid(pid)) {
			return TRUE;
		} else {
			return FALSE;
		}
	} catch (...) {
		return FALSE;
	}
}

gboolean
ubuntu_app_launch_app_id_parse (const gchar * appid, gchar ** package, gchar ** application, gchar ** version)
{
	g_return_val_if_fail(appid != NULL, FALSE);

	try {
		auto appId = ubuntu::app_launch::AppID::parse(appid);

		if (appId.empty()) {
			return FALSE;
		}

		if (package != nullptr) {
			*package = g_strdup(appId.package.value().c_str());
		}

		if (application != nullptr) {
			*application = g_strdup(appId.appname.value().c_str());
		}

		if (version != nullptr) {
			*version = g_strdup(appId.version.value().c_str());
		}
	} catch (...) {
		return FALSE;
	}

	return TRUE;
}

/* Figure out whether we're a libertine container app or a click and then
   choose which function to use */
gchar *
ubuntu_app_launch_triplet_to_app_id (const gchar * pkg, const gchar * app, const gchar * ver)
{
	g_return_val_if_fail(pkg != NULL, NULL);

	std::string package{pkg};
	std::string appname;
	std::string version;

	if (app != nullptr) {
		appname = app;
	}

	if (ver != nullptr) {
		version = ver;
	}

	auto appid = ubuntu::app_launch::AppID::discover(package, appname, version);
	if (appid.empty()) {
		return nullptr;
	}

	return g_strdup(std::string(appid).c_str());
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
	                       NULL, /* cancellable */
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

	gchar * objpath = (gchar *)g_object_get_qdata(obj, proxy_path_quark());
	
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
		gchar * path = (gchar *)g_object_get_qdata(obj, proxy_path_quark());
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
		std::atexit(proxy_cleanup_list);
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
	                       NULL, /* cancellable */
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
		g_strdup_printf("%s:", type),
		strlen(type) + 1, /* 1 for the colon */
		g_array_new(TRUE, TRUE, sizeof(gchar *))
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
		g_strdup_printf("%s:", type),
		strlen(type) + 1, /* 1 for the colon */
		g_array_new(TRUE, TRUE, sizeof(gchar *)),
		g_strdup_printf(":%s", appid)
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
		NULL, /* cancellable */
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

