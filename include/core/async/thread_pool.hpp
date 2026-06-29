#pragma once

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <thread>
#include <vector>

namespace zinc::core::async {

template <class ClockT>
class ThreadPool;

class WorkItem;

using WorkItemPtrT = std::shared_ptr<WorkItem>;

class ThreadPoolSubmittable {
public:
    virtual WorkItemPtrT GetWorkItemPtr() = 0;
};

class WorkItem : public ThreadPoolSubmittable {
public:
    virtual ~WorkItem() = default;
    virtual void Resume() = 0;
    virtual bool Completed() const noexcept = 0;
    virtual void Destroy() = 0;

    bool TryDestroy(WorkItemPtrT &external_ptr);

    void AddDependency(WorkItemPtrT dependency);

    void RemoveDependency(WorkItemPtrT dependency);

    size_t GetDependenciesCount();

    size_t GetDependentsCount();

    void AddCompletionCallback(std::function<void()> callback);

    WorkItemPtrT GetWorkItemPtr() override;

protected:
    void OnComplete();

    std::mutex work_item_mtx_{};

    std::set<WorkItemPtrT> dependencies_{};
    std::set<WorkItemPtrT> dependets_{};

private:
    WorkItemPtrT work_item_ptr_{this, [](auto) {}};

    std::vector<std::function<void()>> complete_callbacks_{};
};

template <class ClockT>
class ThreadPool {

    using TimepointT = std::chrono::time_point<ClockT>;
    using SubmitQueueItemT = std::pair<TimepointT, WorkItemPtrT>;

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
                if (!enabled_.load()) {
                    break;
                }

                {
                    std::unique_lock lg(pool_->submit_queue_mtx_);

                    if (!pool_->submit_queue_.empty() &&
                        pool_->submit_queue_.top().first <= ClockT::now()) {
                        WorkItemPtrT work_item_ptr = pool_->submit_queue_.top().second;

                        pool_->submit_queue_.pop();

                        pool_->Submit(work_item_ptr);
                    }
                }

                size_t tail_reserve_buf = pool_->items_reserve_tail_ptr_.load();

                if (tail_reserve_buf == pool_->items_release_head_ptr_.load()) {
                    continue;
                }

                if (!pool_->items_reserve_tail_ptr_.compare_exchange_weak(
                        tail_reserve_buf, (tail_reserve_buf + 1) % pool_->buffer_size_)) {
                    continue;
                }

                WorkItemPtrT work_item_ptr = pool_->work_items_[tail_reserve_buf];
                pool_->work_items_[tail_reserve_buf] = nullptr;

                size_t expected_tail_release = tail_reserve_buf;

                while (!pool_->items_release_tail_ptr_.compare_exchange_weak(
                    expected_tail_release, (expected_tail_release + 1) % pool_->buffer_size_)) {
                    expected_tail_release = tail_reserve_buf;
                }

                work_item_ptr->Resume();

                work_item_ptr->TryDestroy(work_item_ptr);
            }
        }

        std::thread thread_{};

        ThreadPool *pool_;
        std::atomic_bool enabled_{true};
    };

    ThreadPool(size_t workers_count) : workers_count_(workers_count) {
        work_items_ = std::vector<WorkItemPtrT>(buffer_size_);

        workers_.reserve(workers_count_);
        for (size_t i = 0; i < workers_count; i++) {
            workers_.push_back(std::make_unique<Worker>(this));
        }
    }

    ~ThreadPool() {
        for (auto &worker : workers_) {
            worker->Disable();
        }

        for (auto work_item_ptr : work_items_) {
            if (work_item_ptr) {
                work_item_ptr->Destroy();
            }
        }
    }

    void Submit(ThreadPoolSubmittable &&submittable) {
        Submit(submittable.GetWorkItemPtr());
    };

    void Submit(ThreadPoolSubmittable &submittable) {
        Submit(submittable.GetWorkItemPtr());
    }

    void Submit(std::shared_ptr<ThreadPoolSubmittable> submittable) {
        WorkItemPtrT work_item_ptr = submittable->GetWorkItemPtr();
        size_t head_reserve_buf = items_reserve_head_ptr_.load();

        while (true) {
            if ((head_reserve_buf + 1) % buffer_size_ == items_release_tail_ptr_.load()) {
                head_reserve_buf = items_reserve_head_ptr_.load();

                continue;
            }

            if (items_reserve_head_ptr_.compare_exchange_weak(
                    head_reserve_buf, (head_reserve_buf + 1) % buffer_size_)) {
                break;
            }
        }

        work_items_[head_reserve_buf] = work_item_ptr;

        size_t expected_head_release = head_reserve_buf;

        while (!items_release_head_ptr_.compare_exchange_weak(
            expected_head_release, (expected_head_release + 1) % buffer_size_)) {
            expected_head_release = head_reserve_buf;
        }
    }

    void SubmitAfter(TimepointT time_point, std::shared_ptr<ThreadPoolSubmittable> submittable) {
        WorkItemPtrT work_item_ptr = submittable->GetWorkItemPtr();

        std::unique_lock lg(submit_queue_mtx_);

        submit_queue_.emplace(time_point, work_item_ptr);
    }

private:
    std::vector<std::unique_ptr<Worker>> workers_{};

    std::mutex submit_queue_mtx_{};
    std::priority_queue<SubmitQueueItemT, std::vector<SubmitQueueItemT>,
                        std::greater<SubmitQueueItemT>>
        submit_queue_{};

    std::vector<WorkItemPtrT> work_items_{};

    std::atomic_size_t items_reserve_head_ptr_{};
    std::atomic_size_t items_release_head_ptr_{};
    std::atomic_size_t items_reserve_tail_ptr_{};
    std::atomic_size_t items_release_tail_ptr_{};

    const size_t workers_count_ = 4;
    const size_t buffer_size_ = 1024;
};

}  // namespace zinc::core::async