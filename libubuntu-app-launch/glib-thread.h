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

#include <thread>
#include <future>

#include <gio/gio.h>

namespace GLib
{

class ContextThread
{
    std::thread _thread;
    std::shared_ptr<GMainContext> _context;
    std::shared_ptr<GMainLoop> _loop;
    std::shared_ptr<GCancellable> _cancel;

public:
    ContextThread (std::function<void()> beforeLoop = [] {}, std::function<void()> afterLoop = [] {});
    ~ContextThread ();

    void quit ();
    bool isCancelled ();
    std::shared_ptr<GCancellable> getCancellable ();

    void executeOnThread (std::function<void()> work);
    template<typename T> auto executeOnThread (std::function<T()> work) -> T
    {
        if (std::this_thread::get_id() == _thread.get_id())
        {
            /* Don't block if we're on the same thread */
            return work();
        }

        std::promise<T> promise;
        std::function<void()> magicFunc = [&promise, &work] () {
            promise.set_value(work());
        };

        executeOnThread(magicFunc);

        auto future = promise.get_future();
        future.wait();
        return future.get();
    }

    void timeout (const std::chrono::milliseconds& length, std::function<void()> work);
    template<class Rep, class Period> void timeout (const std::chrono::duration<Rep, Period>& length,
                                                    std::function<void()> work)
    {
        return timeout(std::chrono::duration_cast<std::chrono::milliseconds>(length), work);
    }

    void timeoutSeconds (const std::chrono::seconds& length, std::function<void()> work);
    template<class Rep, class Period> void timeoutSeconds (const std::chrono::duration<Rep, Period>& length,
                                                           std::function<void()> work)
    {
        return timeoutSeconds(std::chrono::duration_cast<std::chrono::seconds>(length), work);
    }

private:
    void simpleSource (std::function<GSource * ()> srcBuilder, std::function<void()> work);
};
}
