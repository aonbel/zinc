
#include "core/async/thread_pool_context.hpp"
#include "gtest/gtest.h"
#include "core/async/task.hpp"
#include <thread>

using zinc::core::async::SubmitToGlobalThreadPool;
using zinc::core::async::Task;
using zinc::core::async::ThreadPoolContext;
using namespace std::chrono_literals;

Task<void> Pass() {
    co_return;
};

Task<int> Return42() {
    co_return 42;
}

Task<int> Recursive(int depth) {
    if (depth == 0) {
        co_return 0;
    } else {
        depth--;
        co_return 1 + co_await Recursive(depth);
    }
}

TEST(TaskTests, Constructs) {
    auto pass_task = Pass();
}

TEST(TaskTests, Resumes) {
    auto pass_task = Pass();

    pass_task.Resume();
}

TEST(TaskTests, Returns) {
    auto return_42_task = Return42();

    return_42_task.Resume();

    ASSERT_TRUE(return_42_task.Completed());
    ASSERT_EQ(return_42_task.Result(), 42);
}

TEST(TaskTests, Recursive) {
    ThreadPoolContext ctx{4};

    int depth = 100;

    auto recursive_task = Recursive(depth);

    SubmitToGlobalThreadPool(recursive_task);

    std::this_thread::sleep_for(1s);

    ASSERT_TRUE(recursive_task.Completed());
    ASSERT_EQ(recursive_task.Result(), depth);
}