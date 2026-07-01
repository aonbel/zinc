#pragma once

#include <atomic>
#include <coroutine>
#include <iostream>
#include <memory>
#include <utility>

#include "thread_pool.hpp"
#include "thread_pool_context.hpp"

namespace zinc::core::async {
template <class T>
class PromiseFor;

template <class T>
class Task : public ThreadPoolSubmittable {
public:
    using promise_type = PromiseFor<T>;

    explicit Task(std::coroutine_handle<PromiseFor<T>> handle) {
        handle_ = handle;

        work_item_ptr_ = handle.promise().GetWorkItemPtr();
    }

    ~Task() {
        work_item_ptr_->TryDestroy(work_item_ptr_);
    };

    void Resume() {
        handle_.promise().Resume();
    }

    bool Completed() {
        return handle_.promise().Completed();
    }

    const T &Result() const {
        return handle_.promise().Result();
    }

    T &&Result() {
        return std::move(handle_.promise().Result());
    }

    constexpr bool await_ready() const noexcept {
        return false;
    }

    template <class U>
    void await_suspend(std::coroutine_handle<PromiseFor<U>> handle) const noexcept;

    constexpr const T &await_resume() const noexcept {
        return Result();
    }

private:
    WorkItemPtrT GetWorkItemPtr() override {
        return work_item_ptr_;
    }

    std::coroutine_handle<PromiseFor<T>> handle_;

    WorkItemPtrT work_item_ptr_;
};

template <>
class Task<void> : public ThreadPoolSubmittable {
public:
    using promise_type = PromiseFor<void>;

    explicit Task(std::coroutine_handle<PromiseFor<void>> handle);

    ~Task() {
        work_item_ptr_->TryDestroy(work_item_ptr_);
    };

    void Resume();

    bool Completed();

    constexpr bool await_ready() const noexcept {
        return false;
    }

    template <class U>
    void await_suspend(std::coroutine_handle<PromiseFor<U>> handle) const noexcept;

    constexpr void await_resume() const noexcept {
    }

private:
    WorkItemPtrT GetWorkItemPtr() override;

    std::coroutine_handle<PromiseFor<void>> handle_;

    WorkItemPtrT work_item_ptr_;
};

template <class T>
class PromiseFor : public WorkItem {
public:
    PromiseFor<T>() : handler_(std::coroutine_handle<PromiseFor<T>>::from_promise(*this)) {
    }

    ~PromiseFor() {
    }

    Task<T> get_return_object() {
        return Task<T>{handler_};
    }

    std::suspend_always initial_suspend() noexcept {
        return {};
    }
    std::suspend_always final_suspend() noexcept {
        return {};
    }

    void unhandled_exception() {
    }

    void Resume() override {
        handler_.resume();
    }

    bool Completed() const noexcept override {
        return completed_;
    }

    void Destroy() override {
        handler_.destroy();
    }

    const T &Result() const {
        return result_;
    }

    T &&Result() {
        return std::move(result_);
    }

    void return_value(T value) {
        result_ = std::move(value);

        completed_ = true;

        OnComplete();
    }

private:
    std::atomic_size_t task_counter_{};
    std::coroutine_handle<PromiseFor<T>> handler_;
    bool completed_{};
    T result_;
};

template <>
class PromiseFor<void> : public WorkItem {
public:
    PromiseFor<void>() : handler_(std::coroutine_handle<PromiseFor<void>>::from_promise(*this)) {
    }

    Task<void> get_return_object() {
        return Task<void>{handler_};
    }

    std::suspend_always initial_suspend() noexcept {
        return {};
    }
    std::suspend_always final_suspend() noexcept {
        return {};
    }

    void unhandled_exception() {
    }

    void Resume() override {
        handler_.resume();
    }

    bool Completed() const noexcept override {
        return completed_;
    }

    void Destroy() override {
        handler_.destroy();
    }

    void return_void() {
        completed_ = true;

        OnComplete();
    }

private:
    std::atomic_size_t task_counter_{};
    std::coroutine_handle<PromiseFor<void>> handler_;
    bool completed_{};
};

template <class T>
template <class U>
void zinc::core::async::Task<T>::await_suspend(
    std::coroutine_handle<PromiseFor<U>> handle) const noexcept {
    PromiseFor<U> &dependent_promise = handle.promise();
    PromiseFor<T> &dependency_promise = handle_.promise();

    auto dependency_work_item_ptr = dependency_promise.GetWorkItemPtr();
    auto dependent_work_item_ptr = dependent_promise.GetWorkItemPtr();

    dependent_promise.AddDependency(dependency_work_item_ptr);

    dependency_promise.AddCompletionCallback([dependent_work_item_ptr, dependency_work_item_ptr] {
        dependent_work_item_ptr->RemoveDependency(dependency_work_item_ptr);

        if (dependent_work_item_ptr->GetDependenciesCount() != 0) {
            return;
        }

        // TODO get right pool
        auto global_thread_pool = GetGlobalThreadPool();
        if (global_thread_pool) {
            global_thread_pool->Submit(dependent_work_item_ptr);
        } else {
            // TODO logger
            std::cerr << "No global thread pool allocated for task " << dependent_work_item_ptr
                      << "\n";
        }
    });

    auto global_thread_pool = GetGlobalThreadPool();

    if (global_thread_pool) {
        global_thread_pool->Submit(dependency_work_item_ptr);
    } else {
        // TODO logger
        std::cerr << "No global thread pool allocated for task " << dependency_work_item_ptr
                  << "\n";
    }
}

template <class U>
void zinc::core::async::Task<void>::await_suspend(
    std::coroutine_handle<PromiseFor<U>> handle) const noexcept {
    PromiseFor<U> &dependent_promise = handle.promise();
    PromiseFor<void> &dependency_promise = handle_.promise();

    auto dependency_work_item_ptr = dependency_promise.GetWorkItemPtr();
    auto dependent_work_item_ptr = dependent_promise.GetWorkItemPtr();

    dependent_promise.AddDependency(dependency_work_item_ptr);

    dependency_promise.AddCompletionCallback([dependent_work_item_ptr, dependency_work_item_ptr] {
        dependent_work_item_ptr->RemoveDependency(dependency_work_item_ptr);

        if (dependent_work_item_ptr->GetDependenciesCount() != 0) {
            return;
        }

        // TODO get right pool
        auto global_thread_pool = GetGlobalThreadPool();
        if (global_thread_pool) {
            global_thread_pool->Submit(dependent_work_item_ptr);
        } else {
            // TODO logger
            std::cerr << "No global thread pool allocated for task " << dependent_work_item_ptr
                      << "\n";
        }
    });

    auto global_thread_pool = GetGlobalThreadPool();

    if (global_thread_pool) {
        global_thread_pool->Submit(dependency_work_item_ptr);
    } else {
        // TODO logger
        std::cerr << "No global thread pool allocated for task " << dependency_work_item_ptr
                  << "\n";
    }
}

}  // namespace zinc::core::async