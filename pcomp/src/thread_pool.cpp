// DAY 21 - The pool implementation, mirroring the core's thread pool.
// Same shape, same guarantees: bounded queue with backpressure, a reusable
// wait_idle barrier, exception capture, clean shutdown.
#include "pc/thread_pool.hpp"

namespace pc {

ThreadPool::ThreadPool(size_t workers, size_t queue_capacity)
    : workers_(workers == 0 ? 1 : workers),
      queue_capacity_(queue_capacity == 0 ? 1 : queue_capacity) {
    threads_.reserve(workers_);
    for (size_t i = 0; i < workers_; ++i) {
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

            // The slot is free the moment the task is dequeued.
            queue_not_full_.notify_one();
        }

        try {
            task();
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!first_error_) first_error_ = std::current_exception();
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (pending_ > 0) --pending_;
            if (pending_ == 0) work_available_.notify_all();
        }
    }
}

void ThreadPool::submit(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_not_full_.wait(lock, [this] {
            return shutting_down_ || queue_.size() < queue_capacity_;
        });
        if (shutting_down_) return;
        ++pending_;
        queue_.push(std::move(task));
    }
    work_available_.notify_one();
}

void ThreadPool::wait_idle() {
    std::unique_lock<std::mutex> lock(mutex_);
    work_available_.wait(lock, [this] { return shutting_down_ || pending_ == 0; });
    const std::exception_ptr error = first_error_;
    first_error_ = nullptr;
    if (error) std::rethrow_exception(error);
}

std::unique_ptr<Executor> make_thread_pool(size_t workers, size_t queue_capacity) {
    return std::make_unique<ThreadPool>(workers, queue_capacity);
}

}  // namespace pc
