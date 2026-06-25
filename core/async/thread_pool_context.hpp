#pragma once

#include "../defaults.hpp"
#include "thread_pool.hpp"
#include <iostream>
#include <type_traits>

namespace zinc::core::async {
namespace internal {
static ThreadPool<::core::defaults::ClockT> *global_thread_pool = nullptr;
}

template <class ClockT = ::core::defaults::ClockT>
static constexpr ThreadPool<ClockT> *GetGlobalThreadPool() {
    if constexpr (std::is_same_v<ClockT, ::core::defaults::ClockT>) {
        return internal::global_thread_pool;
    }

    // TODO smth with global pools

    std::cerr << "Currently unsupported global thread pool type "
              << typeid(ThreadPool<ClockT>).name() << "\n";

    return nullptr;
}

template <class ClockT = ::core::defaults::ClockT>
static constexpr void SubmitToGlobalThreadPool(WorkItem *work_item_ptr) {
    if constexpr (std::is_same_v<ClockT, ::core::defaults::ClockT>) {
        internal::global_thread_pool->Submit(work_item_ptr);

        return;
    }

    // TODO smth with global pools

    std::cerr << "Currently unsupported global thread pool type "
              << typeid(ThreadPool<ClockT>).name() << "\n";

    return;
}

template <class Clock = ::core::defaults::ClockT>
class ThreadPoolContext {
public:
    ThreadPoolContext(ThreadPool<Clock> *thread_pool) {
        current_thread_pool_ = thread_pool;
        previous_thread_pool_ = internal::global_thread_pool;
        internal::global_thread_pool = current_thread_pool_;
    }

    template <class... Types>
    ThreadPoolContext(Types... args) : ThreadPoolContext(new ThreadPool<Clock>(args...)) {
        allocated_ = true;
    }

    ~ThreadPoolContext() {
        internal::global_thread_pool = previous_thread_pool_;
        if (allocated_) {
            delete current_thread_pool_;
        }
    }

private:
    ThreadPool<Clock> *current_thread_pool_;
    ThreadPool<Clock> *previous_thread_pool_;
    bool allocated_{};
};
}  // namespace zinc::core::async