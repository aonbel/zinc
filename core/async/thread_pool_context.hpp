#pragma once

#include "thread_pool.hpp"

namespace zinc::core::async {
static ThreadPool *global_thread_pool = nullptr;

class ThreadPoolContext {
public:
  ThreadPoolContext(ThreadPool *thread_pool) {
    current_thread_pool_ = thread_pool;
    previous_thread_pool_ = global_thread_pool;
    global_thread_pool = current_thread_pool_;
  }

  template <class... Types>
  ThreadPoolContext(Types... args)
      : ThreadPoolContext(new ThreadPool(args...)) {
    allocated = true;
  }

  ~ThreadPoolContext() {
    global_thread_pool = previous_thread_pool_;
    if (allocated) {
      delete current_thread_pool_;
    }
  }

private:
  ThreadPool *current_thread_pool_;
  ThreadPool *previous_thread_pool_;
  bool allocated{};
};
} // namespace zinc::core::async