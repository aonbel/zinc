#include "task.hpp"

template <class T> void zinc::core::async::Task<T>::Resume() {
  handler_.promise().Resume();
}

template <class T> bool zinc::core::async::Task<T>::Completed() {
  handler_.promise().Completed();
}

void zinc::core::async::Task<void>::Resume() { handler_.promise().Resume(); }

bool zinc::core::async::Task<void>::Completed() {
  return handler_.promise().Completed();
}

template <class T>
constexpr void zinc::core::async::Task<T>::await_suspend(
    std::coroutine_handle<PromiseFor<U>> handle) const noexcept {
  PromiseFor<U> &promise = handle.promise();

  promise.AddDependency(handler_.promise().GetPtrFromThis());
}
