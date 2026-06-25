#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <queue>
#include <set>
#include <thread>
#include <vector>

namespace zinc::core::async {

template <class ClockT>
class ThreadPool;

class WorkItem {
public:
    virtual ~WorkItem() = default;
    virtual void Resume() = 0;
    virtual bool Completed() const noexcept = 0;
    virtual void Destroy() = 0;

    void AddDependency(WorkItem *dependency);

    void RemoveDependency(WorkItem *dependency);

    size_t GetDependenciesCount();

    size_t GetDependentsCount();

    void AddCompletionCallback(std::function<void()> callback);

protected:
    void OnComplete() {
        while (!complete_callbacks_.empty()) {
            complete_callbacks_.back()();
            complete_callbacks_.pop_back();
        }
    }

    std::mutex work_item_mtx_;

    std::set<WorkItem *> dependencies_{};
    std::set<WorkItem *> dependets_{};

private:
    std::vector<std::function<void()>> complete_callbacks_;
};

template <class ClockT>
class ThreadPool {

    using TimepointT = std::chrono::time_point<ClockT>;
    using SubmitQueueItemT = std::pair<TimepointT, WorkItem *>;

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
        void Cycle();

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

        for (auto work_item_ptr : work_items_) {
            if (work_item_ptr) {
                work_item_ptr->Destroy();
            }
        }
    }

    void Submit(WorkItem *item);

    void SubmitAfter(TimepointT time_point, WorkItem *work_item);

private:
    std::vector<std::unique_ptr<Worker>> workers_{};

    std::mutex submit_queue_mtx_{};
    std::priority_queue<SubmitQueueItemT, std::vector<SubmitQueueItemT>,
                        std::greater<SubmitQueueItemT>>
        submit_queue_{};

    std::vector<WorkItem *> work_items_{};

    std::atomic_size_t items_reserve_head_ptr_{};
    std::atomic_size_t items_release_head_ptr_{};
    std::atomic_size_t items_reserve_tail_ptr_{};
    std::atomic_size_t items_release_tail_ptr_{};

    const size_t workers_count_ = 4;
    const size_t buffer_size_ = 1024;
};

template <class ClockT>
void zinc::core::async::ThreadPool<ClockT>::Worker::Cycle() {
    while (true) {
        if (!enabled_.load()) {
            break;
        }

        {
            std::unique_lock lg(pool_->submit_queue_mtx_);

            if (!pool_->submit_queue_.empty() &&
                pool_->submit_queue_.top().first <= ClockT::now()) {
                WorkItem *work_item_ptr = pool_->submit_queue_.top().second;

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

        WorkItem *work_item_ptr = pool_->work_items_[tail_reserve_buf];
        pool_->work_items_[tail_reserve_buf] = nullptr;

        size_t expected_tail_release = tail_reserve_buf;

        while (!pool_->items_release_tail_ptr_.compare_exchange_weak(
            expected_tail_release, (expected_tail_release + 1) % pool_->buffer_size_)) {
            expected_tail_release = tail_reserve_buf;
        }

        if (work_item_ptr->GetDependenciesCount() == 0) {
            work_item_ptr->Resume();
        }
    }
}

template <class ClockT>
void zinc::core::async::ThreadPool<ClockT>::Submit(WorkItem *item) {
    size_t head_reserve_buf = items_reserve_head_ptr_.load();

    while (true) {
        if ((head_reserve_buf + 1) % buffer_size_ == items_release_tail_ptr_.load()) {
            head_reserve_buf = items_reserve_head_ptr_.load();

            continue;
        }

        if (items_reserve_head_ptr_.compare_exchange_weak(head_reserve_buf,
                                                          (head_reserve_buf + 1) % buffer_size_)) {
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

template <class ClockT>
void zinc::core::async::ThreadPool<ClockT>::SubmitAfter(std::chrono::time_point<ClockT> time_point,
                                                        WorkItem *work_item) {
    std::unique_lock lg(submit_queue_mtx_);

    submit_queue_.emplace(time_point, work_item);
}

}  // namespace zinc::core::async