#pragma once
// DAY 1 - Execution model and thread-pool API. INTERFACE ONLY.
//
// Execution model (single producer -> N workers -> single writer):
//
//   [Reader/Producer]  reads blocks in deterministic order, assigns seq = 0,1,2,...
//         |  (bounded queue, later)
//         v
//   [Workers x N]      run process_block(job) -> result. Pure function of the job.
//         |
//         v
//   [ReorderBuffer + Writer]  emits results strictly in seq order.
//
// Rules: workers never touch files, never see other jobs, never allocate seq.
//        Only the producer assigns seq; only the writer does output.
#include <cstddef>
#include <functional>
#include <memory>

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

// Declared only. Implemented after the single-thread path is measured (not before Day 6).
std::unique_ptr<Executor> make_thread_pool(size_t workers, size_t queue_capacity);

// Flip to true only when the pool exists AND output is verified byte-identical to serial.
constexpr bool kParallelPathSafe = false;

}  // namespace pc
