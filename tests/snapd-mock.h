/*
 * Copyright © 2016 Canonical Ltd.
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

#include "glib-thread.h"
#include <future>
#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>
#include <gtest/gtest.h>
#include <list>

class SnapdMock
{
public:
    /** Initialize the mock with a list of files to use as
        input and output. Each will be sent in order. */
    SnapdMock(const std::string &socketPath, std::list<std::pair<std::string, std::string>> interactions)
        : thread([]() {},
                 [this]() {
                     for (auto testcase : testCases)
                     {
                         testcase.connection.reset(); /* ensure these get dropped on teh thread */
                     }
                     socketService.reset();
                 })
    {
        for (auto interaction : interactions)
        {
            TestCase testcase{interaction.first, interaction.first, {}, {}};
            testCases.push_back(testcase);
        }

        /* Build the socket */
        socketService = thread.executeOnThread<std::shared_ptr<GSocketService>>([this, socketPath]() {
            auto service = std::shared_ptr<GSocketService>(g_socket_service_new(), [](GSocketService *service) {
                if (service != nullptr)
                {
                    g_socket_service_stop(service);
                    g_object_unref(service);
                }
            });

            GError *error = nullptr;
            auto socket = g_socket_new(G_SOCKET_FAMILY_UNIX,      /* unix */
                                       G_SOCKET_TYPE_INVALID,     /* type */
                                       G_SOCKET_PROTOCOL_DEFAULT, /* protocol */
                                       &error);

            if (error != nullptr)
            {
                std::string message = "Failed to create socket: " + std::string(error->message);
                g_error_free(error);
                throw std::runtime_error(message);
            }

            auto socketaddr = g_unix_socket_address_new(socketPath.c_str());
            if (socketaddr == nullptr)
            {
                throw std::runtime_error("Unable to create a socket address for: " + socketPath);
            }

            g_socket_connect(socket, socketaddr, nullptr, &error);
            if (error != nullptr)
            {
                std::string message =
                    "Unable to connect socket to address '" + socketPath + "': " + std::string(error->message);
                g_error_free(error);
                throw std::runtime_error(message);
            }

            g_socket_listener_add_socket(G_SOCKET_LISTENER(service.get()), socket, nullptr, &error);
            if (error != nullptr)
            {
                std::string message = "Unable to listen to socket: " + std::string(error->message);
                g_error_free(error);
                throw std::runtime_error(message);
            }

            g_signal_connect(service.get(), "incoming", G_CALLBACK(serviceConnectedStatic), this);

            g_socket_service_start(service.get());

            return service;
        });
    }

    /** Check to see if the mock was used successfully */
    inline void result()
    {
        /* Ensure we get queued events off the mainloop */
        std::promise<void> promise;
        thread.timeout(std::chrono::milliseconds{10}, [&promise]() { promise.set_value(); });
        promise.get_future().wait();

        for (auto testcase : testCases)
        {
            EXPECT_EQ(testcase.input, testcase.result);
        }
    }

private:
    GLib::ContextThread thread;
    std::shared_ptr<GSocketService> socketService;

    struct TestCase
    {
        std::string input;
        std::string output;
        std::string result;
        std::shared_ptr<GSocketConnection> connection;
    };

    std::list<TestCase> testCases;

    static gboolean serviceConnectedStatic(GSocketService *service,
                                           GSocketConnection *connection,
                                           GObject *source_obj,
                                           gpointer userdata) noexcept
    {
        auto obj = reinterpret_cast<SnapdMock *>(userdata);
        auto cppconn = std::shared_ptr<GSocketConnection>(G_SOCKET_CONNECTION(g_object_ref(connection)),
                                                          [](GSocketConnection *con) { g_clear_object(&con); });
        return obj->serviceConnected(cppconn) ? TRUE : FALSE;
    }

    bool serviceConnected(std::shared_ptr<GSocketConnection> connection)
    {
        for (auto testcase : testCases)
        {
            if (testcase.connection)
            {
                /* We don't want ones that already have a connection */
                continue;
            }

            testcase.connection = connection;

            auto input = g_io_stream_get_input_stream(G_IO_STREAM(connection.get()));  // transfer: none
            g_input_stream_read_bytes_async(input,                                     /* stream */
                                            1024,                                      /* 1K at a time */
                                            G_PRIORITY_DEFAULT,                        /* default priority */
                                            thread.getCancellable().get(),             /* cancel */
                                            caseInputStatic,                           /* callback */
                                            &(testcase.result));

            auto output = g_io_stream_get_output_stream(G_IO_STREAM(connection.get()));  // transfer: none
            g_output_stream_write_all_async(
                output,                        /* output stream */
                testcase.output.c_str(),       /* data */
                testcase.output.size(),        /* size */
                G_PRIORITY_DEFAULT,            /* priority */
                thread.getCancellable().get(), /* cancel */
                [](GObject *obj, GAsyncResult *res, gpointer userdata) -> void {
                    gsize bytesout = 0;
                    GError *error = nullptr;

                    g_output_stream_write_all_finish(G_OUTPUT_STREAM(obj), res, &bytesout, &error);

                    if (error != nullptr)
                    {
                        g_warning("Unable to write out snapd connection: %s", error->message);
                        g_error_free(error);
                        return;
                    }

                    if (bytesout != reinterpret_cast<gsize>(userdata))
                    {
                        g_warning("Wrote out %d bytes in snapd socket but expected to write out %d", int(bytesout),
                                  reinterpret_cast<int>(GPOINTER_TO_INT(userdata)));
                    }
                },                                        /* callback */
                GINT_TO_POINTER(testcase.output.size())); /* expected size */

            /* We got this one */
            return true;
        }

        g_warning("Couldn't find a test case to use for the connection");
        return false;
    }

    static void caseInputStatic(GObject *obj, GAsyncResult *res, gpointer userdata) noexcept
    {
        GError *error = nullptr;
        auto bytes = g_input_stream_read_bytes_finish(G_INPUT_STREAM(obj), res, &error);

        if (error != nullptr)
        {
            g_warning("Error reading input socket: %s", error->message);
            g_error_free(error);
            return;
        }

        auto bytessize = g_bytes_get_size(bytes);
        if (bytessize > 0)
        {
            auto data = reinterpret_cast<const char *>(g_bytes_get_data(bytes, nullptr));
            auto input = reinterpret_cast<std::string *>(userdata);

            for (unsigned int i = 0; i < bytessize; i++)
            {
                input->push_back(data[i]);
            }

            g_input_stream_read_bytes_async(G_INPUT_STREAM(obj), /* stream */
                                            1024,                /* 1K at a time */
                                            G_PRIORITY_DEFAULT,  /* default priority */
                                            nullptr,             /* TODO? cancel */
                                            caseInputStatic,     /* callback */
                                            userdata);
        }

        g_bytes_unref(bytes);
    }
};
