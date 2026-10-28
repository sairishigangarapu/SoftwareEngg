#pragma once

// DAY 13 - Thread pool (FR-10, part 1).
//
// A fixed pool of workers behind the Executor interface from the execution
// model: a single producer submits tasks, the workers run them, and
// wait_idle() is the completion barrier.
//
// The rules from pcomp/docs/ORDERING.md apply to the caller, not to the
// pool: the pool only guarantees that every submitted task runs exactly
// once and that wait_idle() returns once they all have.

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

#include "core/error.hpp"

namespace uatc::core {

// The execution-model interface (SAD 3.4, the same shape as the pcomp
// scaffold). A single producer submits tasks, the workers run them, and
// wait_idle() is the completion barrier.
class Executor {
 public:
    virtual ~Executor() = default;

    // Enqueues a task. May block if the bounded queue is full
    // (backpressure).
    virtual void submit(std::function<void()> task) = 0;

    // Blocks until every submitted task has finished.
    virtual void wait_idle() = 0;

    virtual std::size_t worker_count() const = 0;
};

class ThreadPool : public Executor {
 public:
    // Creates `workers` threads and a task queue bounded to
    // `queue_capacity` pending tasks. submit() blocks when the queue is
    // full, so the producer cannot run ahead without limit.
    ThreadPool(std::size_t workers, std::size_t queue_capacity);

    ~ThreadPool() override;

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Enqueues a task. Blocks while the bounded queue is full.
    // A task that throws has its exception captured and rethrown to the
    // caller of wait_idle(); the pool itself stays alive.
    void submit(std::function<void()> task) override;

    // Blocks until every submitted task has finished.
    // Rethrows the first exception a task raised, if any.
    void wait_idle() override;

    std::size_t worker_count() const override { return workers_; }

 private:
    void worker_loop();

    const std::size_t workers_;
    const std::size_t queue_capacity_;

    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> queue_;

    std::mutex mutex_;
    std::condition_variable work_available_;
    std::condition_variable queue_not_full_;

    bool shutting_down_ = false;
    std::size_t pending_ = 0;        // queued or running
    std::exception_ptr first_error_; // the first task exception, if any
};

// Factory matching the declared interface.
std::unique_ptr<Executor> make_thread_pool(std::size_t workers,
                                           std::size_t queue_capacity);

}  // namespace uatc::core
