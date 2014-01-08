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

#include <glib.h>

#ifndef __UPSTART_APP_LAUNCH_H__
#define __UPSTART_APP_LAUNCH_H__ 1

#pragma GCC visibility push(default)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * upstart_app_launch_app_failed_t:
 *
 * Types of failure that we report.
 */
typedef enum { /*< prefix=UPSTART_APP_LAUNCH_APP_FAILED */
	UPSTART_APP_LAUNCH_APP_FAILED_CRASH,          /*< nick=crash */
	UPSTART_APP_LAUNCH_APP_FAILED_START_FAILURE,  /*< nick=start-failure */
} upstart_app_launch_app_failed_t;

/**
 * upstart_app_launch_app_observer_t:
 *
 * Function prototype for application observers.
 */
typedef void (*upstart_app_launch_app_observer_t) (const gchar * appid, gpointer user_data);

/**
 * upstart_app_launch_app_failed_observer_t:
 *
 * Function prototype for application failed observers.
 */
typedef void (*upstart_app_launch_app_failed_observer_t) (const gchar * appid, upstart_app_launch_app_failed_t failure_type, gpointer user_data);


/**
 * upstart_app_launch_start_application:
 * @appid: ID of the application to launch
 * @uris: (allow-none): A NULL terminated list of URIs to send to the application
 *
 * Asks upstart to launch an application.
 *
 * Return value: Whether the launch succeeded (may fail later, but upstart
 *    will report the error in that case.
 */
gboolean   upstart_app_launch_start_application         (const gchar *                     appid,
                                                         const gchar * const *             uris);

/**
 * upstart_app_launch_stop_application:
 * @appid: ID of the application to launch
 *
 * Asks upstart to stop an application.
 *
 * Return value: Whether we were able to ask Upstart to stop the process,
 *    used upstart_app_launch_observer_add_app_stop() to know when it is
 *    finally stopped.
 */
gboolean   upstart_app_launch_stop_application         (const gchar *                     appid);

/**
 * upstart_app_launch_observer_add_app_starting:
 * @observer: (scope notified): Callback when an application is about to start
 * @user_data: (closure) (allow-none): Data to pass to the observer
 *
 * Sets up a callback to get called each time an application
 * is about to start.  The application will not start until the
 * function returns.
 *
 * Return value: Whether adding the observer was successful.
 */
gboolean   upstart_app_launch_observer_add_app_starting (void                            (*observer) (const gchar * appid, gpointer user_data),
                                                         gpointer                          user_data);
/**
 * upstart_app_launch_observer_add_app_started:
 * @observer: (scope notified): Callback when an application started
 * @user_data: (closure) (allow-none): Data to pass to the observer
 *
 * Sets up a callback to get called each time an application
 * has been started.
 *
 * Return value: Whether adding the observer was successful.
 */
gboolean   upstart_app_launch_observer_add_app_started  (void                            (*observer) (const gchar * appid, gpointer user_data),
                                                         gpointer                          user_data);
/**
 * upstart_app_launch_observer_add_app_stop:
 * @observer: (scope notified): Callback when an application stops
 * @user_data: (closure) (allow-none): Data to pass to the observer
 *
 * Sets up a callback to get called each time an application
 * stops.
 *
 * Return value: Whether adding the observer was successful.
 */
gboolean   upstart_app_launch_observer_add_app_stop     (void                            (*observer) (const gchar * appid, gpointer user_data),
                                                         gpointer                          user_data);

/**
 * upstart_app_launch_observer_add_app_focus:
 * @observer: (scope notified): Callback when an application is started for the second time
 * @user_data: (closure) (allow-none): Data to pass to the observer
 *
 * Sets up a callback to get called each time an app gets called
 * that is already running, so we request it to be focused again.
 *
 * Return value: Whether adding the observer was successful.
 */
gboolean   upstart_app_launch_observer_add_app_focus    (void                            (*observer) (const gchar * appid, gpointer user_data),
                                                         gpointer                          user_data);

/**
 * upstart_app_launch_observer_add_app_resume:
 * @observer: (scope notified): Callback when an application is started and possibly asleep
 * @user_data: (closure) (allow-none): Data to pass to the observer
 *
 * Sets up a callback to get called each time an app gets called
 * that is already running, so we request it to be given CPU time.
 * At the end of the observer running the app as assumed to be active.
 *
 * Return value: Whether adding the observer was successful.
 */
gboolean   upstart_app_launch_observer_add_app_resume   (void                            (*observer) (const gchar * appid, gpointer user_data),
                                                         gpointer                          user_data);

/**
 * upstart_app_launch_observer_add_app_failed:
 * @observer: (scope notified): Callback when an application fails
 * @user_data: (allow-none) (closure): Data to pass to the observer
 *
 * Sets up a callback to get called each time an application
 * stops via failure.
 *
 * Return value: Whether adding the observer was successful.
 */
gboolean   upstart_app_launch_observer_add_app_failed   (void                            (*observer) (const gchar * appid, upstart_app_launch_app_failed_t failed, gpointer user_data),
                                                         gpointer                          user_data);

/**
 * upstart_app_launch_observer_delete_app_starting:
 * @observer: (scope notified): Callback to remove
 * @user_data: (closure) (allow-none): Data that was passed to the observer
 *
 * Removes a previously registered callback to ensure it no longer
 * gets signaled.
 *
 * Return value: Whether deleting the observer was successful.
 */
gboolean   upstart_app_launch_observer_delete_app_starting (void                            (*observer) (const gchar * appid, gpointer user_data),
                                                            gpointer                          user_data);
/**
 * upstart_app_launch_observer_delete_app_started:
 * @observer: (scope notified): Callback to remove
 * @user_data: (closure) (allow-none): Data that was passed to the observer
 *
 * Removes a previously registered callback to ensure it no longer
 * gets signaled.
 *
 * Return value: Whether deleting the observer was successful.
 */
gboolean   upstart_app_launch_observer_delete_app_started (void                          (*observer) (const gchar * appid, gpointer user_data),
                                                         gpointer                          user_data);
/**
 * upstart_app_launch_observer_delete_app_stop:
 * @observer: (scope notified): Callback to remove
 * @user_data: (closure) (allow-none): Data that was passed to the observer
 *
 * Removes a previously registered callback to ensure it no longer
 * gets signaled.
 *
 * Return value: Whether deleting the observer was successful.
 */
gboolean   upstart_app_launch_observer_delete_app_stop  (void                            (*observer) (const gchar * appid, gpointer user_data),
                                                         gpointer                          user_data);

/**
 * upstart_app_launch_observer_delete_app_focus:
 * @observer: (scope notified): Callback to remove
 * @user_data: (closure) (allow-none): Data that was passed to the observer
 *
 * Removes a previously registered callback to ensure it no longer
 * gets signaled.
 *
 * Return value: Whether deleting the observer was successful.
 */
gboolean   upstart_app_launch_observer_delete_app_focus    (void                            (*observer) (const gchar * appid, gpointer user_data),
                                                            gpointer                          user_data);

/**
 * upstart_app_launch_observer_delete_app_resume:
 * @observer: (scope notified): Callback to remove
 * @user_data: (closure) (allow-none): Data that was passed to the observer
 *
 * Removes a previously registered callback to ensure it no longer
 * gets signaled.
 *
 * Return value: Whether deleting the observer was successful.
 */
gboolean   upstart_app_launch_observer_delete_app_resume   (void                            (*observer) (const gchar * appid, gpointer user_data),
                                                            gpointer                          user_data);

/**
 * upstart_app_launch_observer_delete_app_failed:
 * @observer: (scope notified): Callback to remove
 * @user_data: (closure) (allow-none): Data to pass to the observer
 *
 * Removes a previously registered callback to ensure it no longer
 * gets signaled.
 *
 * Return value: Whether deleting the observer was successful.
 */
gboolean   upstart_app_launch_observer_delete_app_failed (void                                   (*observer) (const gchar * appid, upstart_app_launch_app_failed_t failed, gpointer user_data),
                                                          gpointer                                 user_data);
/**
 * upstart_app_launch_list_running_apps:
 *
 * Gets the Application IDs of all the running applications
 * in the system.
 *
 * Return value: (transfer full): A NULL terminated list of
 *     application IDs.  Should be free'd with g_strfreev().
 */
gchar **   upstart_app_launch_list_running_apps         (void);

/**
 * upstart_app_launch_get_primary_pid:
 * @appid: ID of the application to look for
 *
 * Checks to see if an application is running and returns its
 * main PID if so.
 *
 * Return Value: Either the PID of the application or 0 if it
 *     is not running.
 */
GPid       upstart_app_launch_get_primary_pid           (const gchar *                     appid);

/**
 * upstart_app_launch_pid_in_app_id:
 * @pid: Process ID to check on
 * @appid: ID of the application to look in
 *
 * Checks to see if a PID is associated with the current application ID.
 *
 * Currently the implementation just calls upstart_app_launch_get_primary_pid()
 * and checks to see if they're the same.  But in the future this will check
 * any PID created in the cgroup to see if it is associated.
 *
 * Return Value: Whether @pid is associated with the @appid
 */
gboolean   upstart_app_launch_pid_in_app_id             (GPid                              pid,
                                                         const gchar *                     appid);

#ifdef __cplusplus
}
#endif

#pragma GCC visibility pop

#endif /* __UPSTART_APP_LAUNCH_H__ */
