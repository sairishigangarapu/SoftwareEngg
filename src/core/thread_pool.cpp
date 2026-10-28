#include "core/thread_pool.hpp"

namespace uatc::core {

ThreadPool::ThreadPool(std::size_t workers, std::size_t queue_capacity)
    : workers_(workers == 0 ? 1 : workers),
      queue_capacity_(queue_capacity == 0 ? 1 : queue_capacity) {
    if (workers_ > 1024) {
        throw UatcError(ErrorCode::InvalidArgument,
                        "Worker count is unreasonably large.");
    }
    threads_.reserve(workers_);
    for (std::size_t i = 0; i < workers_; ++i) {
        threads_.emplace_back([this] { worker_loop(); });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutting_down_ = true;
    }
    work_available_.notify_all();
    for (auto& thread : threads_) {
        if (thread.joinable()) thread.join();
    }
}

void ThreadPool::worker_loop() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            work_available_.wait(lock, [this] {
                return shutting_down_ || !queue_.empty();
            });

            if (shutting_down_ && queue_.empty()) return;

            task = std::move(queue_.front());
            queue_.pop();

            // The queue slot is free the moment the task is dequeued, not
            // when it finishes. Notifying here, with the lock held, means a
            // blocked producer can never sleep next to a queue it believes
            // is full - that is the lost-wakeup case that otherwise felines
            // up when one worker is pinned and the queue capacity is one.
            queue_not_full_.notify_one();
        }

        // Run the task without holding the lock. An exception is captured
        // instead of escaping, so one bad task cannot take the pool down.
        try {
            task();
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!first_error_) first_error_ = std::current_exception();
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            // pending_ counts queued and running tasks, so it only drops
            // when the run completes. When it reaches zero the queue has
            // fully drained and wait_idle() must wake.
            if (pending_ > 0) --pending_;
            if (pending_ == 0) work_available_.notify_all();
        }
    }
}

void ThreadPool::submit(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (shutting_down_) {
            throw UatcError(ErrorCode::Internal,
                            "The thread pool is shutting down.");
        }
        queue_not_full_.wait(lock, [this] {
            return shutting_down_ || queue_.size() < queue_capacity_;
        });
        if (shutting_down_) {
            throw UatcError(ErrorCode::Internal,
                            "The thread pool is shutting down.");
        }
        ++pending_;
        queue_.push(std::move(task));
    }
    work_available_.notify_one();
}

void ThreadPool::wait_idle() {
    std::unique_lock<std::mutex> lock(mutex_);
    work_available_.wait(lock, [this] {
        return shutting_down_ || pending_ == 0;
    });

    const std::exception_ptr error = first_error_;
    first_error_ = nullptr;
    if (error) {
        std::rethrow_exception(error);
    }
}

std::unique_ptr<Executor> make_thread_pool(std::size_t workers,
                                           std::size_t queue_capacity) {
    return std::make_unique<ThreadPool>(workers, queue_capacity);
}

}  // namespace uatc::core
