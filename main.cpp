#include "core/async/task.hpp"
#include "core/async/thread_pool_context.hpp"
#include <chrono>
#include <iostream>
#include <thread>

using namespace zinc::core::async;

int global = 0;

Task<void> pass() { co_return; }

Task<int> compute_int() {
  int result = 0;

  for (int i = 0; i < 1000000000; i++) {
    if (i % 2) {
      result++;
    }
  }

  co_return result;
}

Task<void> f() {
  while (true) {
    auto result = co_await compute_int();

    std::cout << result << '\n';

    global++;
  }
}

Task<void> f2() {
  while (true) {
    std::cout << global << '\n';

    co_await pass();
  }
}

int main() {
  ThreadPoolContext ctx{4};

  auto task1 = f();
  auto task2 = f2();

  global_thread_pool->Submit(task1.WorkItem());
  global_thread_pool->Submit(task2.WorkItem());

  std::this_thread::sleep_for(std::chrono::seconds(30));
}