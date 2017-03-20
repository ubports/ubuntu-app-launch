/*
 * Copyright © 2017 Canonical Ltd.
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
 *     Pete Woods <pete.woods@canonical.com>
 */

#include <gio/gio.h>
#include <memory>
#include <unity/util/ResourcePtr.h>

#pragma once
#pragma GCC visibility push(default)

namespace ubuntu
{
namespace app_launch
{

struct DBusSignalUnsubscriber
{
    std::shared_ptr<GDBusConnection> bus_;

    void operator()(guint handle)
    {
        if (handle != 0 && G_IS_OBJECT(bus_.get()))
        {
            g_dbus_connection_signal_unsubscribe(bus_.get(), handle);
        }
    }
};

typedef unity::util::ResourcePtr<guint, DBusSignalUnsubscriber> ManagedDBusSignalConnection;

inline ManagedDBusSignalConnection managedDBusSignalConnection(guint id, std::shared_ptr<GDBusConnection> bus)
{
    return ManagedDBusSignalConnection(id, DBusSignalUnsubscriber{bus});
}

template <typename T>
struct SignalUnsubscriber
{
    std::shared_ptr<T> obj_;

    void operator()(gulong handle)
    {
        if (handle != 0 && G_IS_OBJECT(obj_.get()))
        {
            g_signal_handler_disconnect(obj_.get(), handle);
        }
    }
};

template <typename T>
using ManagedSignalConnection = unity::util::ResourcePtr<gulong, SignalUnsubscriber<T>>;

template <typename T>
inline ManagedSignalConnection<T> managedSignalConnection(gulong id, std::shared_ptr<T> obj)
{
    return ManagedSignalConnection<T>(id, SignalUnsubscriber<T>{obj});
}

}  // namespace app_launch
}  // namespace ubuntu

#pragma GCC visibility pop
