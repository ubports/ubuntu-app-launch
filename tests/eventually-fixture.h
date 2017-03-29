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
    std::chrono::milliseconds _eventuallyTime = std::chrono::minutes{1};
    std::once_flag checkEventuallyEnv_;

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
        std::call_once(checkEventuallyEnv_, [this]() {
            auto eventuallyenv = getenv("EVENTUALLY_TIMEOUT");
            if (eventuallyenv != nullptr)
            {
                _eventuallyTime = std::chrono::seconds{std::atoi(eventuallyenv)};
            }
        });

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

#define _EVENTUALLY_FUNC_HELPER(oper)                                                                            \
    template <typename comptype>                                                                                 \
    testing::AssertionResult eventuallyFuncHelper##oper(const char *desca, const char *descb, comptype expected, \
                                                        std::function<comptype()> checker)                       \
    {                                                                                                            \
        std::function<testing::AssertionResult(void)> func = [&]() {                                             \
            auto newval = checker();                                                                             \
            return testing::internal::CmpHelper##oper(desca, descb, expected, newval);                           \
        };                                                                                                       \
        return eventuallyLoop(func);                                                                             \
    }

#define _EVENTUALLY_FUTURE_HELPER(oper)                                                                            \
    template <typename comptype>                                                                                   \
    testing::AssertionResult eventuallyFutureHelper##oper(const char *desca, const char *descb, comptype expected, \
                                                          std::future<comptype> future)                            \
    {                                                                                                              \
        std::function<testing::AssertionResult(void)> func = [&]() {                                               \
            auto status = future.wait_for(std::chrono::seconds{0});                                                \
            if (status != std::future_status::ready)                                                               \
            {                                                                                                      \
                return testing::AssertionFailure();                                                                \
            }                                                                                                      \
            return testing::internal::CmpHelper##oper(desca, descb, expected, future.get());                       \
        };                                                                                                         \
        return eventuallyLoop(func);                                                                               \
    }

    _EVENTUALLY_HELPER(EQ);
    _EVENTUALLY_HELPER(NE);
    _EVENTUALLY_HELPER(LT);
    _EVENTUALLY_HELPER(GT);
    _EVENTUALLY_HELPER(STREQ);
    _EVENTUALLY_HELPER(STRNE);

    _EVENTUALLY_FUNC_HELPER(EQ);
    _EVENTUALLY_FUNC_HELPER(NE);
    _EVENTUALLY_FUNC_HELPER(LT);
    _EVENTUALLY_FUNC_HELPER(GT);
    _EVENTUALLY_FUNC_HELPER(STREQ);
    _EVENTUALLY_FUNC_HELPER(STRNE);

    _EVENTUALLY_FUTURE_HELPER(EQ);
    _EVENTUALLY_FUTURE_HELPER(NE);
    _EVENTUALLY_FUTURE_HELPER(LT);
    _EVENTUALLY_FUTURE_HELPER(GT);
    _EVENTUALLY_FUTURE_HELPER(STREQ);
    _EVENTUALLY_FUTURE_HELPER(STRNE);

#undef _EVENTUALLY_HELPER
#undef _EVENTUALLY_FUNC_HELPER
#undef _EVENTUALLY_FUTURE_HELPER
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

/* Func Helpers */
#define EXPECT_EVENTUALLY_FUNC_EQ(expected, actual) \
    EXPECT_PRED_FORMAT2(EventuallyFixture::eventuallyFuncHelperEQ, expected, actual)

#define EXPECT_EVENTUALLY_FUNC_NE(expected, actual) \
    EXPECT_PRED_FORMAT2(EventuallyFixture::eventuallyFuncHelperNE, expected, actual)

#define EXPECT_EVENTUALLY_FUNC_LT(expected, actual) \
    EXPECT_PRED_FORMAT2(EventuallyFixture::eventuallyFuncHelperLT, expected, actual)

#define EXPECT_EVENTUALLY_FUNC_GT(expected, actual) \
    EXPECT_PRED_FORMAT2(EventuallyFixture::eventuallyFuncHelperGT, expected, actual)

#define EXPECT_EVENTUALLY_FUNC_STREQ(expected, actual) \
    EXPECT_PRED_FORMAT2(EventuallyFixture::eventuallyFuncHelperSTREQ, expected, actual)

#define EXPECT_EVENTUALLY_FUNC_STRNE(expected, actual) \
    EXPECT_PRED_FORMAT2(EventuallyFixture::eventuallyFuncHelperSTRNE, expected, actual)

#define ASSERT_EVENTUALLY_FUNC_EQ(expected, actual) \
    ASSERT_PRED_FORMAT2(EventuallyFixture::eventuallyFuncHelperEQ, expected, actual)

#define ASSERT_EVENTUALLY_FUNC_NE(expected, actual) \
    ASSERT_PRED_FORMAT2(EventuallyFixture::eventuallyFuncHelperNE, expected, actual)

#define ASSERT_EVENTUALLY_FUNC_LT(expected, actual) \
    ASSERT_PRED_FORMAT2(EventuallyFixture::eventuallyFuncHelperLT, expected, actual)

#define ASSERT_EVENTUALLY_FUNC_GT(expected, actual) \
    ASSERT_PRED_FORMAT2(EventuallyFixture::eventuallyFuncHelperGT, expected, actual)

#define ASSERT_EVENTUALLY_FUNC_STREQ(expected, actual) \
    ASSERT_PRED_FORMAT2(EventuallyFixture::eventuallyFuncHelperSTREQ, expected, actual)

#define ASSERT_EVENTUALLY_FUNC_STRNE(expected, actual) \
    ASSERT_PRED_FORMAT2(EventuallyFixture::eventuallyFuncHelperSTRNE, expected, actual)

/* Future Helpers */
#define EXPECT_EVENTUALLY_FUTURE_EQ(expected, actual) \
    EXPECT_PRED_FORMAT2(EventuallyFixture::eventuallyFutureHelperEQ, expected, actual)

#define EXPECT_EVENTUALLY_FUTURE_NE(expected, actual) \
    EXPECT_PRED_FORMAT2(EventuallyFixture::eventuallyFutureHelperNE, expected, actual)

#define EXPECT_EVENTUALLY_FUTURE_LT(expected, actual) \
    EXPECT_PRED_FORMAT2(EventuallyFixture::eventuallyFutureHelperLT, expected, actual)

#define EXPECT_EVENTUALLY_FUTURE_GT(expected, actual) \
    EXPECT_PRED_FORMAT2(EventuallyFixture::eventuallyFutureHelperGT, expected, actual)

#define EXPECT_EVENTUALLY_FUTURE_STREQ(expected, actual) \
    EXPECT_PRED_FORMAT2(EventuallyFixture::eventuallyFutureHelperSTREQ, expected, actual)

#define EXPECT_EVENTUALLY_FUTURE_STRNE(expected, actual) \
    EXPECT_PRED_FORMAT2(EventuallyFixture::eventuallyFutureHelperSTRNE, expected, actual)

#define ASSERT_EVENTUALLY_FUTURE_EQ(expected, actual) \
    ASSERT_PRED_FORMAT2(EventuallyFixture::eventuallyFutureHelperEQ, expected, actual)

#define ASSERT_EVENTUALLY_FUTURE_NE(expected, actual) \
    ASSERT_PRED_FORMAT2(EventuallyFixture::eventuallyFutureHelperNE, expected, actual)

#define ASSERT_EVENTUALLY_FUTURE_LT(expected, actual) \
    ASSERT_PRED_FORMAT2(EventuallyFixture::eventuallyFutureHelperLT, expected, actual)

#define ASSERT_EVENTUALLY_FUTURE_GT(expected, actual) \
    ASSERT_PRED_FORMAT2(EventuallyFixture::eventuallyFutureHelperGT, expected, actual)

#define ASSERT_EVENTUALLY_FUTURE_STREQ(expected, actual) \
    ASSERT_PRED_FORMAT2(EventuallyFixture::eventuallyFutureHelperSTREQ, expected, actual)

#define ASSERT_EVENTUALLY_FUTURE_STRNE(expected, actual) \
    ASSERT_PRED_FORMAT2(EventuallyFixture::eventuallyFutureHelperSTRNE, expected, actual)
