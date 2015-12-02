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

#ifndef UAL_TRACEPOINT_H__
#define UAL_TRACEPOINT_H__ 1

#include <glib.h>

extern int _ual_tracepoints_env_checked;
extern int _ual_tracepoints_enabled;

/* Little macro that makes it so we can easily turn off all the tracepoints
   if they're not needed. Also cleans up the code a bit by removing some common
   elements */

#define ual_tracepoint(point, ...) \
	if (G_UNLIKELY(!_ual_tracepoints_env_checked)) { \
		_ual_tracepoints_enabled = getenv("UBUNTU_APP_LAUNCH_LTTNG_ENABLED") != NULL; \
		_ual_tracepoints_env_checked = 1; \
	} \
	if (G_UNLIKELY(_ual_tracepoints_enabled)) { \
		tracepoint(ubuntu_app_launch, point, __VA_ARGS__); \
	}


#endif /* UAL_TRACEPOINT_H__ */
