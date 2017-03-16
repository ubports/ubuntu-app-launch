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
#include <libwhoopsie/recoverable-problem.h>

#include "ubuntu-app-launch-trace.h"
#include "helpers.h"
#include "ual-tracepoint.h"
#include "proxy-socket-demangler.h"
}

/* C++ Interface */
#include "application.h"
#include "appid.h"
#include "helper-impl.h"
#include "registry.h"
#include "registry-impl.h"
#include <algorithm>

/* Make a typed string vector out of a GStrv */
template <typename T>
static std::vector<T> uriVector (const gchar * const * uris)
{
	std::vector<T> urivect;

	for (auto i = 0; uris != nullptr && uris[i] != nullptr; i++) {
		urivect.emplace_back(T::from_raw(uris[i]));
	}

	return urivect;
}

gboolean
ubuntu_app_launch_start_application (const gchar * appid, const gchar * const * uris)
{
	try {
		auto registry = ubuntu::app_launch::Registry::getDefault();
		auto appId = ubuntu::app_launch::AppID::find(appid);
		auto app = ubuntu::app_launch::Application::create(appId, registry);

		auto uriv = uriVector<ubuntu::app_launch::Application::URL>(uris);

		auto instance = app->launch(uriv);

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

		auto uriv = uriVector<ubuntu::app_launch::Application::URL>(uris);

		auto instance = app->launchTest(uriv);

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

/* Function to take a work function and have it execute on a given
   GMainContext */
static void executeOnContext (const std::shared_ptr<GMainContext>& context, std::function<void()> work)
{
	if (!context) {
		work();
		return;
	}

    auto heapWork = new std::function<void()>(work);

    auto source = std::shared_ptr<GSource>(g_idle_source_new(), [](GSource* src) { g_clear_pointer(&src, g_source_unref); });
    g_source_set_callback(source.get(),
                          [](gpointer data) {
                              auto heapWork = static_cast<std::function<void()>*>(data);
                              (*heapWork)();
                              return G_SOURCE_REMOVE;
                          },
                          heapWork,
                          [](gpointer data) {
                              auto heapWork = static_cast<std::function<void()>*>(data);
                              delete heapWork;
                          });

    g_source_attach(source.get(), context.get());
}

/** A handy helper function that is based of a function to get
    a signal and put it into a map. */
template <core::Signal<const std::shared_ptr<ubuntu::app_launch::Application>&, const std::shared_ptr<ubuntu::app_launch::Application::Instance>&>& (*getSignal)(const std::shared_ptr<ubuntu::app_launch::Registry>&)>
static gboolean
observer_add (UbuntuAppLaunchAppObserver observer, gpointer user_data, std::map<std::pair<UbuntuAppLaunchAppObserver, gpointer>, core::ScopedConnection> &observers)
{
	auto context = std::shared_ptr<GMainContext>(g_main_context_ref_thread_default(), [](GMainContext * context) { g_clear_pointer(&context, g_main_context_unref); });

	observers.emplace(std::make_pair(
		std::make_pair(observer, user_data),
		core::ScopedConnection(
			getSignal(ubuntu::app_launch::Registry::getDefault())
				.connect([context, observer, user_data](std::shared_ptr<ubuntu::app_launch::Application> app, std::shared_ptr<ubuntu::app_launch::Application::Instance> instance) {
					std::string appid = app->appId();
					executeOnContext(context, [appid, observer, user_data]() {
						observer(appid.c_str(), user_data);
					});
			})
		)
		));

	return TRUE;
}

/** A handy helper to delete items from an observer map */
template<typename observertype>
static gboolean
observer_delete (observertype observer, gpointer user_data, std::map<std::pair<observertype, gpointer>, core::ScopedConnection> &observers)
{
	auto iter = observers.find(std::make_pair(observer, user_data));

	if (iter == observers.end()) {
		return FALSE;
	}

	observers.erase(iter);
	return TRUE;
}

/** Map of all the observers listening for app started */
static std::map<std::pair<UbuntuAppLaunchAppObserver, gpointer>, core::ScopedConnection> appStartedObservers;

gboolean
ubuntu_app_launch_observer_add_app_started (UbuntuAppLaunchAppObserver observer, gpointer user_data)
{
	return observer_add<&ubuntu::app_launch::Registry::appStarted>(observer, user_data, appStartedObservers);
}

gboolean
ubuntu_app_launch_observer_delete_app_started (UbuntuAppLaunchAppObserver observer, gpointer user_data)
{
	return observer_delete<UbuntuAppLaunchAppObserver>(observer, user_data, appStartedObservers);
}

/* Map of all the observers listening for app stopped */
static std::map<std::pair<UbuntuAppLaunchAppObserver, gpointer>, core::ScopedConnection> appStoppedObservers;

gboolean
ubuntu_app_launch_observer_add_app_stop (UbuntuAppLaunchAppObserver observer, gpointer user_data)
{
	return observer_add<&ubuntu::app_launch::Registry::appStopped>(observer, user_data, appStoppedObservers);
}

gboolean
ubuntu_app_launch_observer_delete_app_stop (UbuntuAppLaunchAppObserver observer, gpointer user_data)
{
	return observer_delete<UbuntuAppLaunchAppObserver>(observer, user_data, appStoppedObservers);
}

/** Class to implement the Registry::Manager interface for the C code
    using a GLib mainloop. */
class CManager : public ubuntu::app_launch::Registry::Manager
{
public:
	CManager () {
		g_debug("Creating the CManager object");
	}
	virtual ~CManager() {
		g_debug("Removing the shared the CManager object");
	}

	void startingRequest(const std::shared_ptr<ubuntu::app_launch::Application>& app,
                                     const std::shared_ptr<ubuntu::app_launch::Application::Instance>& instance,
                                     std::function<void(bool)> reply) override {
		requestImpl(app, instance, reply, "starting", startingList);
	}

	void focusRequest(const std::shared_ptr<ubuntu::app_launch::Application>& app,
                                  const std::shared_ptr<ubuntu::app_launch::Application::Instance>& instance,
                                  std::function<void(bool)> reply) override {
		requestImpl(app, instance, reply, "focus", focusList);
	}

	void resumeRequest(const std::shared_ptr<ubuntu::app_launch::Application> &app,
                                   const std::shared_ptr<ubuntu::app_launch::Application::Instance> &instance,
                                   std::function<void(bool)> reply) override {
		requestImpl(app, instance, reply, "resume", resumeList);
	}

private:
	/** The Data that we track on an observer. It is the functions to
	    call, the user data and the context to call it on. */
	struct ObserverData {
		UbuntuAppLaunchAppObserver observer;
		gpointer user_data;
		std::shared_ptr<GMainContext> context;

		/** Handy constructor to get the context in one place */
		ObserverData(UbuntuAppLaunchAppObserver obs, gpointer ud)
			: observer(obs)
			, user_data(ud) {
			context = std::shared_ptr<GMainContext>(g_main_context_ref_thread_default(), [](GMainContext * context) { g_clear_pointer(&context, g_main_context_unref); });
		}
	};

	std::list<ObserverData> focusList;    /**< List of observers on the focus signal */
	std::list<ObserverData> resumeList;   /**< List of observers on the resume signal */
	std::list<ObserverData> startingList; /**< List of observers on the starting signal */

	/** Removes an observer from a specified list */
	bool removeObserver (std::list<ObserverData> &list, UbuntuAppLaunchAppObserver observer, gpointer user_data) {
		auto iter = std::find_if(list.begin(), list.end(), [observer, user_data](const ObserverData &data) {
			return data.observer == observer && data.user_data == user_data;
		});

		if (iter == list.end()) {
			return false;
		}

		list.erase(iter);
		return true;
	}

	/** Implements a request for a specified list by calling each observer and then the reply */
	inline void requestImpl ( const std::shared_ptr<ubuntu::app_launch::Application> &app,
                                   const std::shared_ptr<ubuntu::app_launch::Application::Instance> &instance,
                                   std::function<void(bool)> reply,
				   const std::string& name,
				   std::list<ObserverData>& list) {
		std::string sappid = app->appId();
		g_debug("CManager %s: %s", name.c_str(), sappid.c_str());

		for (const auto &data : list) {
			executeOnContext(data.context, [data, sappid]() {
				data.observer(sappid.c_str(), data.user_data);
			});
		}

		reply(true);
	}

public:
	void addFocus (UbuntuAppLaunchAppObserver observer, gpointer user_data) {
		focusList.emplace_back(ObserverData(observer, user_data));
	}
	void addResume (UbuntuAppLaunchAppObserver observer, gpointer user_data) {
		resumeList.emplace_back(ObserverData(observer, user_data));
	}
	void addStarting (UbuntuAppLaunchAppObserver observer, gpointer user_data) {
		startingList.emplace_back(ObserverData(observer, user_data));
	}
	bool deleteFocus (UbuntuAppLaunchAppObserver observer, gpointer user_data) {
		return removeObserver(focusList, observer, user_data);
	}
	bool deleteResume (UbuntuAppLaunchAppObserver observer, gpointer user_data) {
		return removeObserver(resumeList, observer, user_data);
	}
	bool deleteStarting (UbuntuAppLaunchAppObserver observer, gpointer user_data) {
		return removeObserver(startingList, observer, user_data);
	}
};

/** Weak pointer to the CManager if it is still in use. If it gets free'd by
    the registry we're okay with that. */
static std::weak_ptr<CManager> cmanager;

/** Function to create the CManager if it doesn't currently exist. Otherwise
    just return a lock to it */
static std::shared_ptr<CManager>
ensure_cmanager ()
{
	auto retval = cmanager.lock();

	if (!retval) {
		retval = std::make_shared<CManager>();
		ubuntu::app_launch::Registry::setManager(retval, ubuntu::app_launch::Registry::getDefault());
		cmanager = retval;
	}

	return retval;
}

gboolean
ubuntu_app_launch_observer_add_app_focus (UbuntuAppLaunchAppObserver observer, gpointer user_data)
{
	auto manager = ensure_cmanager();
	manager->addFocus(observer, user_data);
	return TRUE;
}

gboolean
ubuntu_app_launch_observer_delete_app_focus (UbuntuAppLaunchAppObserver observer, gpointer user_data)
{
	auto manager = ensure_cmanager();
	return manager->deleteFocus(observer, user_data) ? TRUE : FALSE;
}

gboolean
ubuntu_app_launch_observer_add_app_resume (UbuntuAppLaunchAppObserver observer, gpointer user_data)
{
	auto manager = ensure_cmanager();
	manager->addResume(observer, user_data);
	return TRUE;
}

gboolean
ubuntu_app_launch_observer_delete_app_resume (UbuntuAppLaunchAppObserver observer, gpointer user_data)
{
	auto manager = ensure_cmanager();
	return manager->deleteResume(observer, user_data) ? TRUE : FALSE;
}

gboolean
ubuntu_app_launch_observer_add_app_starting (UbuntuAppLaunchAppObserver observer, gpointer user_data)
{
	auto manager = ensure_cmanager();
	ubuntu::app_launch::Registry::Impl::watchingAppStarting(true);
	manager->addStarting(observer, user_data);
	return TRUE;
}

gboolean
ubuntu_app_launch_observer_delete_app_starting (UbuntuAppLaunchAppObserver observer, gpointer user_data)
{
	auto manager = ensure_cmanager();
	ubuntu::app_launch::Registry::Impl::watchingAppStarting(false);
	return manager->deleteStarting(observer, user_data) ? TRUE : FALSE;
}

/* Map of all the observers listening for app stopped */
static std::map<std::pair<UbuntuAppLaunchAppFailedObserver, gpointer>, core::ScopedConnection> appFailedObservers;

gboolean
ubuntu_app_launch_observer_add_app_failed (UbuntuAppLaunchAppFailedObserver observer, gpointer user_data)
{
	auto context = std::shared_ptr<GMainContext>(g_main_context_ref_thread_default(), [](GMainContext * context) { g_clear_pointer(&context, g_main_context_unref); });

	appFailedObservers.emplace(std::make_pair(
		std::make_pair(observer, user_data),
			core::ScopedConnection(
				ubuntu::app_launch::Registry::appFailed().connect([context, observer, user_data](std::shared_ptr<ubuntu::app_launch::Application> app, std::shared_ptr<ubuntu::app_launch::Application::Instance> instance, ubuntu::app_launch::Registry::FailureType type) {
					std::string appid = app->appId();
					executeOnContext(context, [appid, type, observer, user_data]() {
						UbuntuAppLaunchAppFailed ctype{UBUNTU_APP_LAUNCH_APP_FAILED_CRASH};

						switch (type) {
						case ubuntu::app_launch::Registry::FailureType::CRASH:
							ctype = UBUNTU_APP_LAUNCH_APP_FAILED_CRASH;
							break;
						case ubuntu::app_launch::Registry::FailureType::START_FAILURE:
							ctype = UBUNTU_APP_LAUNCH_APP_FAILED_START_FAILURE;
							break;
						}

						observer(appid.c_str(), ctype, user_data);
					});
				})
			)
		));

	return TRUE;
}

gboolean
ubuntu_app_launch_observer_delete_app_failed (UbuntuAppLaunchAppFailedObserver observer, gpointer user_data)
{
	return observer_delete<UbuntuAppLaunchAppFailedObserver>(observer, user_data, appFailedObservers);
}

/** Handy helper for pause and resume here */
template <core::Signal<const std::shared_ptr<ubuntu::app_launch::Application>&, const std::shared_ptr<ubuntu::app_launch::Application::Instance>&, const std::vector<pid_t>&>& (*getSignal)(const std::shared_ptr<ubuntu::app_launch::Registry>&)>
static gboolean
observer_add_pause (UbuntuAppLaunchAppPausedResumedObserver observer, gpointer user_data, std::map<std::pair<UbuntuAppLaunchAppPausedResumedObserver, gpointer>, core::ScopedConnection> &observers)
{
	auto context = std::shared_ptr<GMainContext>(g_main_context_ref_thread_default(), [](GMainContext * context) { g_clear_pointer(&context, g_main_context_unref); });

	observers.emplace(std::make_pair(
		std::make_pair(observer, user_data),
		core::ScopedConnection(
			getSignal(ubuntu::app_launch::Registry::getDefault())
				.connect([context, observer, user_data](std::shared_ptr<ubuntu::app_launch::Application> app, std::shared_ptr<ubuntu::app_launch::Application::Instance> instance, const std::vector<pid_t> &pids) {
					std::vector<pid_t> lpids = pids;
					lpids.emplace_back(0);

					std::string appid = app->appId();

					executeOnContext(context, [appid, observer, user_data, lpids]() {
						observer(appid.c_str(), (int *)(lpids.data()), user_data);
					});
			})
		)
		));

	return TRUE;
}

static std::map<std::pair<UbuntuAppLaunchAppPausedResumedObserver, gpointer>, core::ScopedConnection> appPausedObservers;

gboolean
ubuntu_app_launch_observer_add_app_paused (UbuntuAppLaunchAppPausedResumedObserver observer, gpointer user_data)
{
	return observer_add_pause<&ubuntu::app_launch::Registry::appPaused>(observer, user_data, appPausedObservers);
}

gboolean
ubuntu_app_launch_observer_delete_app_paused (UbuntuAppLaunchAppPausedResumedObserver observer, gpointer user_data)
{
	return observer_delete<UbuntuAppLaunchAppPausedResumedObserver>(observer, user_data, appPausedObservers);
}

static std::map<std::pair<UbuntuAppLaunchAppPausedResumedObserver, gpointer>, core::ScopedConnection> appResumedObservers;

gboolean
ubuntu_app_launch_observer_add_app_resumed (UbuntuAppLaunchAppPausedResumedObserver observer, gpointer user_data)
{
	return observer_add_pause<&ubuntu::app_launch::Registry::appResumed>(observer, user_data, appResumedObservers);
}

gboolean
ubuntu_app_launch_observer_delete_app_resumed (UbuntuAppLaunchAppPausedResumedObserver observer, gpointer user_data)
{
	return observer_delete<UbuntuAppLaunchAppPausedResumedObserver>(observer, user_data, appResumedObservers);
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

gboolean
ubuntu_app_launch_start_helper (const gchar * type, const gchar * appid, const gchar * const * uris)
{
	g_return_val_if_fail(type != NULL, FALSE);
	g_return_val_if_fail(appid != NULL, FALSE);
	g_return_val_if_fail(g_strstr_len(type, -1, ":") == NULL, FALSE);

	try {
		auto registry = ubuntu::app_launch::Registry::getDefault();
		auto appId = ubuntu::app_launch::AppID::find(appid);
		auto helper = ubuntu::app_launch::Helper::create(ubuntu::app_launch::Helper::Type::from_raw(type), appId, registry);

		if (!helper->instances().empty()) {
			throw std::runtime_error{"Instance already exits"};
		}

		auto uriv = uriVector<ubuntu::app_launch::Helper::URL>(uris);
		auto inst = helper->launch(uriv);

		if (!inst) {
			throw std::runtime_error{"Empty instance"};
		}

		return true;
	} catch (std::runtime_error &e) {
		g_warning("Unable to launch helper of type '%s' id '%s': %s", type, appid, e.what());
		return false;
	}
}

gchar *
ubuntu_app_launch_start_multiple_helper (const gchar * type, const gchar * appid, const gchar * const * uris)
{
	g_return_val_if_fail(type != NULL, NULL);
	g_return_val_if_fail(appid != NULL, NULL);
	g_return_val_if_fail(g_strstr_len(type, -1, ":") == NULL, NULL);

	try {
		auto registry = ubuntu::app_launch::Registry::getDefault();
		auto appId = ubuntu::app_launch::AppID::find(appid);
		auto helper = ubuntu::app_launch::Helper::create(ubuntu::app_launch::Helper::Type::from_raw(type), appId, registry);

		auto uriv = uriVector<ubuntu::app_launch::Helper::URL>(uris);
		auto inst = helper->launch(uriv);

		if (!inst) {
			throw std::runtime_error{"Empty instance"};
		}

		return g_strdup(std::dynamic_pointer_cast<ubuntu::app_launch::helper_impls::BaseInstance>(inst)->getInstanceId().c_str());
	} catch (std::runtime_error &e) {
		g_warning("Unable to launch helper of type '%s' id '%s': %s", type, appid, e.what());
		return nullptr;
	}
}

gchar *
ubuntu_app_launch_start_session_helper (const gchar * type, MirPromptSession * session, const gchar * appid, const gchar * const * uris)
{
	g_return_val_if_fail(type != NULL, NULL);
	g_return_val_if_fail(session != NULL, NULL);
	g_return_val_if_fail(appid != NULL, NULL);
	g_return_val_if_fail(g_strstr_len(type, -1, ":") == NULL, NULL);

	try {
		auto registry = ubuntu::app_launch::Registry::getDefault();
		auto appId = ubuntu::app_launch::AppID::find(appid);
		auto helper = ubuntu::app_launch::Helper::create(ubuntu::app_launch::Helper::Type::from_raw(type), appId, registry);

		auto uriv = uriVector<ubuntu::app_launch::Helper::URL>(uris);
		auto inst = helper->launch(session, uriv);

		if (!inst) {
			throw std::runtime_error{"Empty instance"};
		}

		return g_strdup(std::dynamic_pointer_cast<ubuntu::app_launch::helper_impls::BaseInstance>(inst)->getInstanceId().c_str());
	} catch (std::runtime_error &e) {
		g_warning("Unable to launch helper of type '%s' id '%s': %s", type, appid, e.what());
		return nullptr;
	}
}

gboolean
ubuntu_app_launch_stop_helper (const gchar * type, const gchar * appid)
{
	g_return_val_if_fail(type != NULL, FALSE);
	g_return_val_if_fail(appid != NULL, FALSE);
	g_return_val_if_fail(g_strstr_len(type, -1, ":") == NULL, FALSE);

	try {
		auto registry = ubuntu::app_launch::Registry::getDefault();
		auto appId = ubuntu::app_launch::AppID::find(appid);
		auto helper = ubuntu::app_launch::Helper::create(ubuntu::app_launch::Helper::Type::from_raw(type), appId, registry);

		auto insts = helper->instances();
		if (insts.empty()) {
			throw std::runtime_error{"No instances"};
		}
		if (insts.size() > 1u) {
			throw std::runtime_error{"Expecting single instance but has multiple instances"};
		}

		(*insts.begin())->stop();

		return TRUE;
	} catch (std::runtime_error &e) {
		g_warning("Unable to stop helper of type '%s' id '%s': %s", type, appid, e.what());
		return FALSE;
	}
}

gboolean
ubuntu_app_launch_stop_multiple_helper (const gchar * type, const gchar * appid, const gchar * instanceid)
{
	g_return_val_if_fail(type != NULL, FALSE);
	g_return_val_if_fail(appid != NULL, FALSE);
	g_return_val_if_fail(instanceid != NULL, FALSE);
	g_return_val_if_fail(g_strstr_len(type, -1, ":") == NULL, FALSE);

	try {
		auto registry = ubuntu::app_launch::Registry::getDefault();
		auto appId = ubuntu::app_launch::AppID::find(appid);
		auto helper = ubuntu::app_launch::Helper::create(ubuntu::app_launch::Helper::Type::from_raw(type), appId, registry);

		auto inst = std::dynamic_pointer_cast<ubuntu::app_launch::helper_impls::Base>(helper)->existingInstance(instanceid);
		if (!inst) {
			throw std::runtime_error{"No instances"};
		}

		inst->stop();

		return TRUE;
	} catch (std::runtime_error &e) {
		g_warning("Unable to stop helper of type '%s' id '%s' instance '%s': %s", type, appid, instanceid, e.what());
		return FALSE;
	}
}

gchar **
ubuntu_app_launch_list_helpers (const gchar * type)
{
	g_return_val_if_fail(type != NULL, FALSE);
	g_return_val_if_fail(g_strstr_len(type, -1, ":") == NULL, FALSE);

	try {
		auto registry = ubuntu::app_launch::Registry::getDefault();
		auto helpers = registry->runningHelpers(ubuntu::app_launch::Helper::Type::from_raw(type));

		auto array = g_array_new(TRUE, TRUE, sizeof(gchar *));

		for (const auto &helper : helpers) {
			gchar * appid = g_strdup(std::string(helper->appId()).c_str());
			g_array_append_val(array, appid);
		}

		return (gchar **)g_array_free(array, FALSE);
	} catch (std::runtime_error &e) {
		g_warning("Unable to get helpers for type '%s': %s", type, e.what());
		return NULL;
	}
}

gchar **
ubuntu_app_launch_list_helper_instances (const gchar * type, const gchar * appid)
{
	g_return_val_if_fail(type != NULL, NULL);
	g_return_val_if_fail(g_strstr_len(type, -1, ":") == NULL, NULL);
	g_return_val_if_fail(appid != NULL, NULL);

	try {
		auto registry = ubuntu::app_launch::Registry::getDefault();
		auto appId = ubuntu::app_launch::AppID::find(appid);
		auto helper = ubuntu::app_launch::Helper::create(ubuntu::app_launch::Helper::Type::from_raw(type), appId, registry);

		auto insts = helper->instances();

		auto array = g_array_new(TRUE, TRUE, sizeof(gchar *));

		for (const auto &inst : insts) {
			auto internalinst = std::dynamic_pointer_cast<ubuntu::app_launch::helper_impls::BaseInstance>(inst);
			auto cstr = g_strdup(internalinst->getInstanceId().c_str());
			g_array_append_val(array, cstr);
		}

		return (gchar **)g_array_free(array, FALSE);
	} catch (std::runtime_error &e) {
		g_warning("Unable to get helper instances for '%s' of type '%s': %s", appid, type, e.what());
		return NULL;
	}
}

/** A handy helper function that is based of a function to get
    a signal and put it into a map. */
template <core::Signal<const std::shared_ptr<ubuntu::app_launch::Helper>&, const std::shared_ptr<ubuntu::app_launch::Helper::Instance>&>& (*getSignal)(ubuntu::app_launch::Helper::Type type, const std::shared_ptr<ubuntu::app_launch::Registry>&)>
static gboolean
helper_add (UbuntuAppLaunchHelperObserver observer, const gchar * helper_type, gpointer user_data, std::map<std::tuple<UbuntuAppLaunchHelperObserver, std::string, gpointer>, core::ScopedConnection> &observers)
{
	auto context = std::shared_ptr<GMainContext>(g_main_context_ref_thread_default(), [](GMainContext * context) { g_clear_pointer(&context, g_main_context_unref); });
	auto type = ubuntu::app_launch::Helper::Type::from_raw(helper_type);

	observers.emplace(std::make_pair(
		std::make_tuple(observer, type.value(), user_data),
		core::ScopedConnection(
			getSignal(type, ubuntu::app_launch::Registry::getDefault())
				.connect([type, context, observer, user_data](std::shared_ptr<ubuntu::app_launch::Helper> app, std::shared_ptr<ubuntu::app_launch::Helper::Instance> instance) {
					auto internalinst = std::dynamic_pointer_cast<ubuntu::app_launch::helper_impls::BaseInstance>(instance);

					std::string appid = app->appId();
					std::string instanceid = internalinst->getInstanceId();

					executeOnContext(context, [appid, instanceid, type, observer, user_data]() {
						observer(appid.c_str(), instanceid.c_str(), type.value().c_str(), user_data);
					});
			})
		)
		));

	return TRUE;
}

static gboolean
helper_delete (UbuntuAppLaunchHelperObserver observer, const gchar * helper_type, gpointer user_data, std::map<std::tuple<UbuntuAppLaunchHelperObserver, std::string, gpointer>, core::ScopedConnection> &observers)
{
	auto type = ubuntu::app_launch::Helper::Type::from_raw(helper_type);
	auto iter = observers.find(std::make_tuple(observer, type.value(), user_data));

	if (iter == observers.end()) {
		return FALSE;
	}

	observers.erase(iter);
	return TRUE;
}

/** Map of all the observers listening for helper started */
static std::map<std::tuple<UbuntuAppLaunchHelperObserver, std::string, gpointer>, core::ScopedConnection> helperStartedObservers;
static std::map<std::tuple<UbuntuAppLaunchHelperObserver, std::string, gpointer>, core::ScopedConnection> helperStoppedObservers;

gboolean
ubuntu_app_launch_observer_add_helper_started (UbuntuAppLaunchHelperObserver observer, const gchar * helper_type, gpointer user_data)
{
	g_return_val_if_fail(observer != NULL, FALSE);
	g_return_val_if_fail(helper_type != NULL, FALSE);
	g_return_val_if_fail(g_strstr_len(helper_type, -1, ":") == NULL, FALSE);

return helper_add<&ubuntu::app_launch::Registry::helperStarted>(observer, helper_type, user_data, helperStartedObservers);
}

gboolean
ubuntu_app_launch_observer_add_helper_stop (UbuntuAppLaunchHelperObserver observer, const gchar * helper_type, gpointer user_data)
{
	g_return_val_if_fail(observer != NULL, FALSE);
	g_return_val_if_fail(helper_type != NULL, FALSE);
	g_return_val_if_fail(g_strstr_len(helper_type, -1, ":") == NULL, FALSE);

	return helper_add<&ubuntu::app_launch::Registry::helperStopped>(observer, helper_type, user_data, helperStoppedObservers);
}

gboolean
ubuntu_app_launch_observer_delete_helper_started (UbuntuAppLaunchHelperObserver observer, const gchar * helper_type, gpointer user_data)
{
	g_return_val_if_fail(observer != NULL, FALSE);
	g_return_val_if_fail(helper_type != NULL, FALSE);
	g_return_val_if_fail(g_strstr_len(helper_type, -1, ":") == NULL, FALSE);

	return helper_delete(observer, helper_type, user_data, helperStartedObservers);
}

gboolean
ubuntu_app_launch_observer_delete_helper_stop (UbuntuAppLaunchHelperObserver observer, const gchar * helper_type, gpointer user_data)
{
	g_return_val_if_fail(observer != NULL, FALSE);
	g_return_val_if_fail(helper_type != NULL, FALSE);
	g_return_val_if_fail(g_strstr_len(helper_type, -1, ":") == NULL, FALSE);

	return helper_delete(observer, helper_type, user_data, helperStoppedObservers);
}

gboolean
ubuntu_app_launch_helper_set_exec (const gchar * execline, const gchar * directory)
{
	g_return_val_if_fail(execline != NULL, FALSE);
	g_return_val_if_fail(execline[0] != '\0', FALSE);

	GError * error{nullptr};
	gchar ** splitexec{nullptr};
	if (!g_shell_parse_argv(execline, nullptr, &splitexec, &error) || error != nullptr) {
		g_warning("Unable to parse exec line '%s': %s", execline, error->message);
		g_error_free(error);
		return FALSE;
	}

	std::vector<std::string> execvect;
	for (auto i = 0; splitexec[i] != nullptr; i++) {
		execvect.emplace_back(std::string{splitexec[i]});
	}

	g_strfreev(splitexec);

	try {
		ubuntu::app_launch::Helper::setExec(execvect);
		return TRUE;
	} catch (std::runtime_error &e) {
		g_warning("Unable to set exec line '%s': %s", execline, e.what());
		return FALSE;
	}
}

