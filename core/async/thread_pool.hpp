#pragma once

#include <atomic>
#include <set>
#include <thread>
#include <vector>

namespace zinc::core::async {

class WorkItem {
public:
  virtual ~WorkItem() = default;
  virtual void Resume() = 0;
  virtual bool Completed() const noexcept = 0;
  virtual void Destroy() = 0;

  void AddDependency(WorkItem *dependency) {
    dependencies_.insert(dependency);

    dependency->dependets_.insert(this);
  }

  void RemoveDependency(WorkItem *dependency) {
    dependencies_.erase(dependency);

    dependency->dependets_.erase(this);
  }

  size_t GetDependenciesCount() const { return dependencies_.size(); }

  size_t GetDependentsCount() const { return dependets_.size(); }

protected:
  std::set<WorkItem *> dependencies_;
  std::set<WorkItem *> dependets_;
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
      while (true) {
        size_t tail_reserve_buf = pool_->items_reserve_tail_ptr_.load();

        while (true) {
          if (!enabled_.load()) {
            return;
          }

          if (tail_reserve_buf == pool_->items_release_head_ptr_.load()) {
            tail_reserve_buf = pool_->items_reserve_tail_ptr_.load();

            continue;
          }

          if (pool_->items_reserve_tail_ptr_.compare_exchange_weak(
                  tail_reserve_buf,
                  (tail_reserve_buf + 1) % pool_->buffer_size_)) {
            break;
          }
        }

        WorkItem *work_item_ptr = pool_->work_items_[tail_reserve_buf];
        pool_->work_items_[tail_reserve_buf] = nullptr;

        size_t expected_tail_release = tail_reserve_buf;

        while (!pool_->items_release_tail_ptr_.compare_exchange_weak(
            expected_tail_release,
            (expected_tail_release + 1) % pool_->buffer_size_)) {
          expected_tail_release = tail_reserve_buf;
        }

        if (work_item_ptr->GetDependenciesCount() == 0) {
          work_item_ptr->Resume();
        }

        if (!work_item_ptr->Completed()) {
          pool_->Submit(work_item_ptr);
        }
      }
    }

    std::thread thread_{};

    ThreadPool *pool_;
    std::atomic_bool enabled_{true};
  };

  ThreadPool(size_t workers_count) : workers_count_(workers_count) {
    work_items_ = std::vector<WorkItem *>(buffer_size_);

    workers_.reserve(workers_count_);
    for (size_t i = 0; i < workers_count; i++) {
      workers_.push_back(std::make_unique<Worker>(this));
    }
  }

  ~ThreadPool() {
    for (auto &worker : workers_) {
      worker->Disable();
    }

    for (auto &work_item : work_items_) {
      if (work_item) {
        work_item->Destroy();
      }
    }
  }

  void Submit(WorkItem *item) {
    size_t head_reserve_buf = items_reserve_head_ptr_.load();

    while (true) {
      if ((head_reserve_buf + 1) % buffer_size_ ==
          items_release_tail_ptr_.load()) {
        head_reserve_buf = items_reserve_head_ptr_.load();

        continue;
      }

      if (items_reserve_head_ptr_.compare_exchange_weak(
              head_reserve_buf, (head_reserve_buf + 1) % buffer_size_)) {
        break;
      }
    }

    work_items_[head_reserve_buf] = item;

    size_t expected_head_release = head_reserve_buf;

    while (!items_release_head_ptr_.compare_exchange_weak(
        expected_head_release, (expected_head_release + 1) % buffer_size_)) {
      expected_head_release = head_reserve_buf;
    }
  }

private:
  std::vector<std::unique_ptr<Worker>> workers_{};

  std::vector<WorkItem *> work_items_;
  std::atomic_uint64_t items_reserve_head_ptr_{};
  std::atomic_uint64_t items_release_head_ptr_{};
  std::atomic_uint64_t items_reserve_tail_ptr_{};
  std::atomic_uint64_t items_release_tail_ptr_{};

  const size_t workers_count_ = 4;
  const size_t buffer_size_ = 1024;
};
} // namespace zinc::core::async