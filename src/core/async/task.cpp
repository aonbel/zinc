#include "core/async/task.hpp"

#include <memory>

zinc::core::async::Task<void>::Task(std::coroutine_handle<PromiseFor<void>> handle) {
    handle_ = handle;

    work_item_ptr_ = handle.promise().GetWorkItemPtr();
}

void zinc::core::async::Task<void>::Resume() {
    handle_.promise().Resume();
}

bool zinc::core::async::Task<void>::Completed() {
    return handle_.promise().Completed();
}

std::shared_ptr<zinc::core::async::WorkItem> zinc::core::async::Task<void>::GetWorkItemPtr() {
    return work_item_ptr_;
}
