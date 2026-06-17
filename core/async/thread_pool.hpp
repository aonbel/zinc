#pragma once

#include <atomic>
#include <memory>
#include <set>
#include <thread>
#include <vector>

namespace zinc::core::async {

class WorkItem : public std::enable_shared_from_this<WorkItem> {
public:
  virtual ~WorkItem() {};
  virtual void Resume() = 0;
  virtual bool Completed() const noexcept = 0;

  std::shared_ptr<WorkItem> GetPtrFromThis() { return shared_from_this(); }

  void AddDependency(const std::shared_ptr<WorkItem> &dependency) {
    dependencies_.insert(dependency);

    dependency->dependets_.insert(GetPtrFromThis());
  }

  void AddDependency(std::shared_ptr<WorkItem> &&dependency) {
    dependencies_.insert(std::move(dependency));

    dependency->dependets_.insert(GetPtrFromThis());
  }

  void RemoveDependency(const std::shared_ptr<WorkItem> &dependency) {
    dependencies_.erase(dependency);

    dependency->dependets_.erase(GetPtrFromThis());
  }

  void RemoveDependency(std::shared_ptr<WorkItem> &&dependency) {
    dependencies_.erase(dependency);

    dependency->dependets_.erase(GetPtrFromThis());
  }

  size_t GetDependenciesCount() const { return dependencies_.size(); }

  size_t GetDependentsCount() const { return dependets_.size(); }

protected:
  std::set<std::shared_ptr<WorkItem>> dependencies_;
  std::set<std::shared_ptr<WorkItem>> dependets_;
};

class ThreadPool {
public:
  friend class Worker;

  class Worker {
  public:
    Worker(ThreadPool *pool) : pool_(pool) {
      thread_ = std::thread{[&] { Cycle(); }};
    }

    void Disable() {
      enabled_.store(false);
      thread_.join();
    }

  private:
    void Cycle() {
      while (enabled_.load()) {
        size_t head_buf = pool_->items_head_ptr_.load();
        std::shared_ptr<WorkItem> item_ptr = pool_->items_[head_buf];

        while (head_buf == pool_->items_tail_ptr_.load() ||
               !pool_->items_head_ptr_.compare_exchange_weak(head_buf,
                                                             head_buf + 1)) {
          head_buf = pool_->items_head_ptr_.load();
          item_ptr = pool_->items_[head_buf];
        }

        pool_->items_[head_buf].reset();

        if (item_ptr->GetDependenciesCount() == 0) {
          item_ptr->Resume();
        }
      }
    }

    std::thread thread_{};

    ThreadPool *pool_;
    std::atomic_bool enabled_{true};
  };

  ThreadPool(size_t workers_count) : workers_count_(workers_count) {
    items_ = std::vector<std::shared_ptr<WorkItem>>(buffer_size_);

    workers_.reserve(workers_count_);
    for (size_t i = 0; i < workers_count; i++) {
      workers_.push_back(std::make_unique<Worker>(this));
    }
  }

  ~ThreadPool() {
    for (auto &worker : workers_) {
      worker->Disable();
    }
  }

  void Submit(std::shared_ptr<WorkItem> item) {
    size_t tail_buf = items_tail_ptr_.load();

    while ((tail_buf + 1) % buffer_size_ == items_head_ptr_.load() ||
           items_[tail_buf] != nullptr ||
           !items_tail_ptr_.compare_exchange_weak(tail_buf, (tail_buf + 1) %
                                                                buffer_size_)) {
      tail_buf = items_tail_ptr_.load();
    }

    size_t insert_pos = (tail_buf + buffer_size_ - 1) % buffer_size_;
    items_[insert_pos] = item;
  }

private:
  std::vector<std::unique_ptr<Worker>> workers_{};

  std::vector<std::shared_ptr<WorkItem>> items_;
  std::atomic_uint64_t items_head_ptr_{};
  std::atomic_uint64_t items_tail_ptr_{};

  const size_t workers_count_ = 4;
  const size_t buffer_size_ = 1024;
};
} // namespace zinc::core::async