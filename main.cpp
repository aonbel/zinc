#include "core/async/task.hpp"
#include "core/async/thread_pool_context.hpp"
#include <iostream>
#include <memory>

using namespace zinc::core::async;

Task<int> compute_int() {
  int result = 0;

  for (int i = 0; i < 100000000; i++) {
    result++;
  }

  co_return result;
}

Task<void> f() {
  auto result = co_await compute_int();

  std::cout << result << '\n';
}

int main() {
  ThreadPoolContext ctx{4};

  auto coro = f();
  coro.Resume();
  coro.Resume();
}