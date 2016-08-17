/*
 * Copyright © 2016 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Ted Gould <ted@canonical.com>
 */

#include <chrono>
#include <future>

#include <gio/gio.h>
#include <gtest/gtest.h>

class EventuallyFixture : public ::testing::Test
{
protected:
    std::chrono::milliseconds _eventuallyTime = std::chrono::seconds{10};

    static gboolean timeout_cb(gpointer user_data)
    {
        auto loop = static_cast<GMainLoop *>(user_data);
        g_main_loop_quit(loop);
        return G_SOURCE_REMOVE;
    }

    void pause(unsigned int ms = 0)
    {
        GMainLoop *loop = g_main_loop_new(NULL, FALSE);
        g_timeout_add(ms, timeout_cb, loop);
        g_main_loop_run(loop);
        g_main_loop_unref(loop);
    }

    testing::AssertionResult eventuallyLoop(std::function<testing::AssertionResult(void)> &testfunc)
    {
        auto loop = std::shared_ptr<GMainLoop>(g_main_loop_new(nullptr, FALSE),
                                               [](GMainLoop *loop) { g_clear_pointer(&loop, g_main_loop_unref); });

        std::promise<testing::AssertionResult> retpromise;
        auto retfuture = retpromise.get_future();
        auto start = std::chrono::steady_clock::now();

        /* The core of the idle function as an object so we can use the C++-isms
           of attaching the variables and make this code reasonably readable */
        std::function<gboolean(void)> idlefunc = [&loop, &retpromise, &testfunc, &start, this]() -> gboolean {
            auto result = testfunc();
            auto elapsed = std::chrono::steady_clock::now() - start;

            if (result == false)
            {
                if (_eventuallyTime > elapsed)
                {
                    return G_SOURCE_CONTINUE;
                }

                g_warning("Eventually time out after: %d ms",
                          int(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()));
            }

            retpromise.set_value(result);
            g_main_loop_quit(loop.get());
            return G_SOURCE_REMOVE;
        };

        g_idle_add(
            [](gpointer data) -> gboolean {
                auto func = reinterpret_cast<std::function<gboolean(void)> *>(data);
                return (*func)();
            },
            &idlefunc);

        g_main_loop_run(loop.get());

        return retfuture.get();
    }

/* Eventually Helpers */
#define _EVENTUALLY_HELPER(oper)                                                    \
    template <typename... Args>                                                     \
    testing::AssertionResult eventuallyHelper##oper(Args &&... args)                \
    {                                                                               \
        std::function<testing::AssertionResult(void)> func = [&]() {                \
            return testing::internal::CmpHelper##oper(std::forward<Args>(args)...); \
        };                                                                          \
        return eventuallyLoop(func);                                                \
    }

    _EVENTUALLY_HELPER(EQ);
    _EVENTUALLY_HELPER(NE);
    _EVENTUALLY_HELPER(LT);
    _EVENTUALLY_HELPER(GT);
    _EVENTUALLY_HELPER(STREQ);
    _EVENTUALLY_HELPER(STRNE);

#undef _EVENTUALLY_HELPER
};

/* Helpers */
#define EXPECT_EVENTUALLY_EQ(expected, actual) \
    EXPECT_PRED_FORMAT2(EventuallyFixture::eventuallyHelperEQ, expected, actual)

#define EXPECT_EVENTUALLY_NE(expected, actual) \
    EXPECT_PRED_FORMAT2(EventuallyFixture::eventuallyHelperNE, expected, actual)

#define EXPECT_EVENTUALLY_LT(expected, actual) \
    EXPECT_PRED_FORMAT2(EventuallyFixture::eventuallyHelperLT, expected, actual)

#define EXPECT_EVENTUALLY_GT(expected, actual) \
    EXPECT_PRED_FORMAT2(EventuallyFixture::eventuallyHelperGT, expected, actual)

#define EXPECT_EVENTUALLY_STREQ(expected, actual) \
    EXPECT_PRED_FORMAT2(EventuallyFixture::eventuallyHelperSTREQ, expected, actual)

#define EXPECT_EVENTUALLY_STRNE(expected, actual) \
    EXPECT_PRED_FORMAT2(EventuallyFixture::eventuallyHelperSTRNE, expected, actual)

#define ASSERT_EVENTUALLY_EQ(expected, actual) \
    ASSERT_PRED_FORMAT2(EventuallyFixture::eventuallyHelperEQ, expected, actual)

#define ASSERT_EVENTUALLY_NE(expected, actual) \
    ASSERT_PRED_FORMAT2(EventuallyFixture::eventuallyHelperNE, expected, actual)

#define ASSERT_EVENTUALLY_LT(expected, actual) \
    ASSERT_PRED_FORMAT2(EventuallyFixture::eventuallyHelperLT, expected, actual)

#define ASSERT_EVENTUALLY_GT(expected, actual) \
    ASSERT_PRED_FORMAT2(EventuallyFixture::eventuallyHelperGT, expected, actual)

#define ASSERT_EVENTUALLY_STREQ(expected, actual) \
    ASSERT_PRED_FORMAT2(EventuallyFixture::eventuallyHelperSTREQ, expected, actual)

#define ASSERT_EVENTUALLY_STRNE(expected, actual) \
    ASSERT_PRED_FORMAT2(EventuallyFixture::eventuallyHelperSTRNE, expected, actual)
