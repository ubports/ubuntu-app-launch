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
 *   Ted Gould <ted.gould@canonical.com>
 */

#include "glib-thread.h"

#include <unity/util/GlibMemory.h>

using namespace unity::util;

namespace GLib
{

ContextThread::ContextThread(const std::function<void()>& beforeLoop, const std::function<void()>& afterLoop)
{
    _cancel = std::shared_ptr<GCancellable>(g_cancellable_new(), [](GCancellable* cancel) {
        if (cancel != nullptr)
        {
            g_cancellable_cancel(cancel);
            g_object_unref(cancel);
        }
    });
    std::promise<std::pair<std::shared_ptr<GMainContext>, std::shared_ptr<GMainLoop>>> context_promise;

    /* NOTE: We copy afterLoop but reference beforeLoop. We're blocking so we
       know that beforeLoop will stay valid long enough, but we can't say the
       same for afterLoop */
    afterLoop_ = afterLoop;
    afterFlag_ = std::make_shared<std::once_flag>();

    _thread = std::thread([&context_promise, &beforeLoop, this]() {
        /* Build up the context and loop for the async events and a place
           for GDBus to send its events back to */
        auto context = share_glib(g_main_context_new());
        auto loop = share_glib(g_main_loop_new(context.get(), FALSE));

        g_main_context_push_thread_default(context.get());

        beforeLoop();

        /* Free's the constructor to continue */
        context_promise.set_value(std::make_pair(context, loop));

        if (!g_cancellable_is_cancelled(_cancel.get()))
        {
            g_main_loop_run(loop.get());
        }

        std::call_once(*afterFlag_, afterLoop_);
    });

    /* We need to have the context and the mainloop ready before
       other functions on this object can work properly. So we wait
       for them and set them on this thread. */
    auto context_future = context_promise.get_future();
    context_future.wait();
    auto context_value = context_future.get();

    _context = context_value.first;
    _loop = context_value.second;

    if (!_context || !_loop)
    {
        throw std::runtime_error("Unable to create GLib Thread");
    }
}

ContextThread::~ContextThread()
{
    quit();
}

void ContextThread::quit()
{
    g_cancellable_cancel(_cancel.get()); /* Force the cancellation on ongoing tasks */
    if (_loop)
    {
        g_main_loop_quit(_loop.get()); /* Quit the loop */
    }

    /* Joining here because we want to ensure that the final afterLoop()
       function is run before returning */
    if (_thread.joinable())
    {
        if (std::this_thread::get_id() != _thread.get_id())
        {
            _thread.join();
        }
        else
        {
            std::call_once(*afterFlag_, afterLoop_);
            _thread.detach();
        }
    }
}

bool ContextThread::isCancelled()
{
    return g_cancellable_is_cancelled(_cancel.get()) == TRUE;
}

std::shared_ptr<GCancellable> ContextThread::getCancellable()
{
    return _cancel;
}

guint ContextThread::simpleSource(std::function<GSource*()> srcBuilder, std::function<void()> work)
{
    if (isCancelled())
    {
        throw std::runtime_error("Trying to execute work on a GLib thread that is shutting down.");
    }

    /* Copy the work so that we can reuse it */
    /* Lifecycle is handled with the source pointer when we attach
       it to the context. */
    auto heapWork = new std::function<void()>(work);

    auto source = unique_glib(srcBuilder());
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

    return g_source_attach(source.get(), _context.get());
}

guint ContextThread::executeOnThread(std::function<void()> work)
{
    return simpleSource(g_idle_source_new, work);
}

guint ContextThread::timeout(const std::chrono::milliseconds& length, std::function<void()> work)
{
    return simpleSource([length]() { return g_timeout_source_new(length.count()); }, work);
}

guint ContextThread::timeoutSeconds(const std::chrono::seconds& length, std::function<void()> work)
{
    return simpleSource([length]() { return g_timeout_source_new_seconds(length.count()); }, work);
}

void ContextThread::removeSource(guint sourceid)
{
    auto source = g_main_context_find_source_by_id(_context.get(), sourceid);
    if (source == nullptr)
    {
        return;
    }

    g_source_destroy(source);
}

}  // ns GLib
