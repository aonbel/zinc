#include "core/async/thread_pool.hpp"

#include <iostream>
#include <mutex>

void zinc::core::async::WorkItem::RemoveDependency(WorkItemPtrT dependency) {
    std::unique_lock lg(work_item_mtx_);

    dependencies_.erase(dependency);

    dependency->TryDestroy(dependency);
}

void zinc::core::async::WorkItem::AddDependency(WorkItemPtrT dependency) {
    std::unique_lock lg(work_item_mtx_);

    dependencies_.insert(dependency);
}

void zinc::core::async::WorkItem::AddCompletionCallback(std::function<void()> callback) {
    std::unique_lock lg(work_item_mtx_);

    if (Completed()) {
        callback();

        while (!complete_callbacks_.empty()) {
            complete_callbacks_.back()();
            complete_callbacks_.pop_back();
        }

        return;
    }

    complete_callbacks_.push_back(callback);
}
size_t zinc::core::async::WorkItem::GetDependenciesCount() {
    std::unique_lock lg(work_item_mtx_);

    return dependencies_.size();
}
bool zinc::core::async::WorkItem::TryDestroy(WorkItemPtrT &external_ptr) {
    {
        std::lock_guard lg(work_item_mtx_);

        if (external_ptr.get() != work_item_ptr_.get()) {
            // TODO logger

            std::cerr << "external ptr should be equal to internal ptr\n";
            std::exit(EXIT_FAILURE);
        }

        if (work_item_ptr_.use_count() > 2) {
            return false;
        }

        external_ptr.reset();
        work_item_ptr_.reset();
    }

    Destroy();

    return true;
}
void zinc::core::async::WorkItem::OnComplete() {
    while (!complete_callbacks_.empty()) {
        complete_callbacks_.back()();
        complete_callbacks_.pop_back();
    }
}
zinc::core::async::WorkItemPtrT zinc::core::async::WorkItem::GetWorkItemPtr() {
    return work_item_ptr_;
}
