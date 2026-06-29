#pragma once

#include <chrono>
#include <coroutine>

#include "core/async/task.hpp"

namespace zinc::core::async::awaiters {
template <class Rep, class Period, class ClockT>
struct DurationAwaitable {

    bool await_ready() {
        return false;
    }

    template <class U>
    void await_suspend(std::coroutine_handle<PromiseFor<U>> handle) {
        PromiseFor<U> &dependent_promise = handle.promise();

        auto dependent_work_item_ptr = dependent_promise.GetWorkItemPtr();

        auto global_thread_pool = GetGlobalThreadPool();

        if (global_thread_pool) {
            global_thread_pool->SubmitAfter(ClockT::now() + duration_, dependent_work_item_ptr);
        } else {
            // TODO loggers

            std::cerr << "No global thread pool allocated for task " << dependent_work_item_ptr
                      << "\n";
        }
    }

    void await_resume() {
    }

    std::chrono::duration<Rep, Period> duration_;
};
}  // namespace zinc::core::async::awaiters

template <class Rep, class Period, class ClockT = ::core::defaults::ClockT>
zinc::core::async::awaiters::DurationAwaitable<Rep, Period, ClockT> operator co_await(
    const std::chrono::duration<Rep, Period> &duration) {
    return {duration};
}