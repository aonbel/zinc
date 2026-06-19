#include "thread_pool.hpp"

#include <mutex>

void zinc::core::async::WorkItem::RemoveDependency(WorkItem *dependency) {
  std::unique_lock lg(work_item_mtx_);

  dependencies_.erase(dependency);

  dependency->dependets_.erase(this);
}

void zinc::core::async::WorkItem::AddDependency(WorkItem *dependency) {
  std::unique_lock lg(work_item_mtx_);

  dependencies_.insert(dependency);

  dependency->dependets_.insert(this);
}

void zinc::core::async::WorkItem::AddCompletionCallback(
    std::function<void()> callback) {
  std::unique_lock lg(work_item_mtx_);

  if (Completed()) {
    callback();

    while (!complete_callbacks_.empty()) {
      complete_callbacks_.back()();
      complete_callbacks_.pop_back();
    }

    return;
  }

  complete_callbacks_.push_back(std::move(callback));
}
size_t zinc::core::async::WorkItem::GetDependenciesCount() {
  std::unique_lock lg(work_item_mtx_);

  return dependencies_.size();
}
size_t zinc::core::async::WorkItem::GetDependentsCount() {
  std::unique_lock lg(work_item_mtx_);

  return dependets_.size();
}
