#pragma once
// DAY 1 (implemented Day 21) - Execution model and thread-pool API.
//
// Execution model (single producer -> N workers -> single writer):
//
//   [Reader/Producer]  reads blocks in deterministic order, assigns seq = 0,1,2,...
//         |  (bounded queue)
//         v
//   [Workers x N]      run process_block(job) -> result. Pure function of the job.
//         |
//         v
//   [ReorderBuffer + Writer]  emits results strictly in seq order.
//
// Rules: workers never touch files, never see other jobs, never allocate seq.
//        Only the producer assigns seq; only the writer does output.
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

namespace pc {

class Executor {
 public:
    virtual ~Executor() = default;
    // Enqueue a task. May block if the bounded queue is full (backpressure).
    virtual void submit(std::function<void()> task) = 0;
    // Block until every submitted task has finished.
    virtual void wait_idle() = 0;
    virtual size_t worker_count() const = 0;
};

// Bounded-queue pool. submit() applies backpressure; wait_idle() is a
// reusable barrier; a task that throws is captured and rethrown at
// wait_idle(); the destructor drains the queue and joins every worker.
class ThreadPool : public Executor {
 public:
    ThreadPool(size_t workers, size_t queue_capacity);
    ~ThreadPool() override;

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void submit(std::function<void()> task) override;
    void wait_idle() override;
    size_t worker_count() const override { return workers_; }

 private:
    void worker_loop();

    const size_t workers_;
    const size_t queue_capacity_;

    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> queue_;

    std::mutex mutex_;
    std::condition_variable work_available_;
    std::condition_variable queue_not_full_;

    bool shutting_down_ = false;
    size_t pending_ = 0;
    std::exception_ptr first_error_;
};

// Flip to true only when the pool exists AND output is verified byte-identical to serial.
constexpr bool kParallelPathSafe = true;

std::unique_ptr<Executor> make_thread_pool(size_t workers, size_t queue_capacity);

}  // namespace pc
