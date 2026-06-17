#pragma once

#include <coroutine>
#include <memory>
#include <utility>

#include "thread_pool.hpp"
#include "thread_pool_context.hpp"

namespace zinc::core::async {
template <class T> class PromiseFor;

template <class T> class Task {
public:
  using promise_type = PromiseFor<T>;

  explicit Task(std::coroutine_handle<PromiseFor<T>> handler)
      : handler_(handler) {}

  void Resume();

  bool Completed();

  const T &Result() const { return handler_.promise().Result(); }

  T &&Result() { return std::move(handler_.promise().Result()); }

  constexpr bool await_ready() const noexcept { return false; }

  template <class U>
  constexpr void
  await_suspend(std::coroutine_handle<PromiseFor<U>> handle) const noexcept {
    PromiseFor<U> &promise = handle.promise();

    promise.AddDependency(handler_.promise().GetPtrFromThis());
  }

  constexpr const T &await_resume() const noexcept { return Result(); }

private:
  std::coroutine_handle<PromiseFor<T>> handler_;
};

template <> class Task<void> {
public:
  using promise_type = PromiseFor<void>;

  explicit Task(std::coroutine_handle<PromiseFor<void>> handler)
      : handler_(handler) {}

  void Resume();

  bool Completed();

  constexpr bool await_ready() const noexcept { return false; }

  template <class U>
  constexpr void
  await_suspend(std::coroutine_handle<PromiseFor<U>> handle) const noexcept {
    PromiseFor<U> &promise = handle.promise();

    promise.AddDependency(handler_.promise().GetPtrFromThis());
  }

  constexpr void await_resume() const noexcept {}

private:
  std::coroutine_handle<PromiseFor<void>> handler_;
};

template <class T> class PromiseFor : public WorkItem {
public:
  PromiseFor<T>()
      : handler_(std::coroutine_handle<PromiseFor<T>>::from_promise(*this)),
        task_global_thread_pool_(global_thread_pool) {}

  Task<T> get_return_object() { return Task<T>{handler_}; }

  std::suspend_always initial_suspend() noexcept { return {}; }
  std::suspend_never final_suspend() noexcept { return {}; }

  void unhandled_exception() {}

  void Resume() override {
    ThreadPoolContext ctx{task_global_thread_pool_};

    handler_.resume();

    task_global_thread_pool_ = global_thread_pool;
  }

  bool Completed() const noexcept override { return completed_; }

  const T &Result() const { return result_; }

  T &&Result() { return std::move(result_); }

  void return_value(T value) {
    for (auto &dependent : dependets_) {
      dependent->RemoveDependency(GetPtrFromThis());
    }

    completed_ = true;
    result_ = std::move(value);
  }

private:
  std::coroutine_handle<PromiseFor<T>> handler_;
  ThreadPool *task_global_thread_pool_;
  bool completed_{};
  T result_;
};

template <> class PromiseFor<void> : public WorkItem {
public:
  PromiseFor<void>()
      : handler_(std::coroutine_handle<PromiseFor<void>>::from_promise(*this)),
        task_global_thread_pool_(global_thread_pool) {}

  Task<void> get_return_object() { return Task<void>{handler_}; }

  std::suspend_always initial_suspend() noexcept { return {}; }
  std::suspend_never final_suspend() noexcept { return {}; }

  void unhandled_exception() {}

  void Resume() override {
    ThreadPoolContext ctx{task_global_thread_pool_};

    handler_.resume();

    task_global_thread_pool_ = global_thread_pool;
  }

  bool Completed() const noexcept override { return completed_; }

  void return_void() {
    for (auto &dependent : dependets_) {
      dependent->RemoveDependency(GetPtrFromThis());
    }

    completed_ = true;
  }

private:
  std::coroutine_handle<PromiseFor<void>> handler_;
  ThreadPool *task_global_thread_pool_;
  bool completed_{};
};
} // namespace zinc::core::async