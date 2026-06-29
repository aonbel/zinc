
#include "gtest/gtest.h"
#include "core/async/task.hpp"

using zinc::core::async::Task;

Task<void> Pass() {
    co_return;
};

Task<int> Return42() {
    co_return 42;
}

TEST(TaskTests, Constructs) {
    auto pass_task = Pass();
}

TEST(TaskTests, Resumes) {
    auto pass_task = Pass();

    pass_task.Resume();
}

TEST(TaskTests, Returns) {
    auto pass_task = Return42();

    pass_task.Resume();

    ASSERT_EQ(pass_task.Result(), 42);
}