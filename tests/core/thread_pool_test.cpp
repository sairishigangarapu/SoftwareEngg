// DAY 13 - Thread pool tests (FR-10, TC-PAR-03).
//
// Covers: every submitted task runs exactly once, wait_idle() is a reliable
// completion barrier, an exception in a task is reported without taking the
// pool down, backpressure bounds the queue, and shutdown is clean.
#include <atomic>
#include <cassert>
#include <cstdio>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>

#include "core/thread_pool.hpp"

int main() {
    using namespace uatc::core;

    // 1. Every task runs exactly once.
    {
        std::mutex lock;
        std::set<std::size_t> seen;
        constexpr std::size_t kTasks = 2000;

        auto pool = make_thread_pool(8, 64);
        assert(pool->worker_count() == 8);
        for (std::size_t i = 0; i < kTasks; ++i) {
            pool->submit([&lock, &seen, i] {
                std::lock_guard<std::mutex> l(lock);
                seen.insert(i);
            });
        }
        pool->wait_idle();
        assert(seen.size() == kTasks);
    }

    // 2. wait_idle() is a barrier: everything submitted before it is done.
    {
        std::atomic<int> counter{0};
        constexpr int kTasks = 500;

        auto pool = make_thread_pool(4, 32);
        for (int i = 0; i < kTasks; ++i) {
            pool->submit([&counter] {
                ++counter;
                std::this_thread::yield();
            });
        }
        pool->wait_idle();
        assert(counter == kTasks);

        // The pool is reusable after wait_idle().
        for (int i = 0; i < kTasks; ++i) {
            pool->submit([&counter] { ++counter; });
        }
        pool->wait_idle();
        assert(counter == 2 * kTasks);
    }

    // 3. An exception in a task is reported by wait_idle(), and the pool
    //    survives to run more work afterwards.
    {
        auto pool = make_thread_pool(4, 8);
        std::atomic<int> good{0};
        pool->submit([] { throw std::runtime_error("task failed"); });
        for (int i = 0; i < 10; ++i) {
            pool->submit([&good] { ++good; });
        }

        bool caught = false;
        try {
            pool->wait_idle();
        } catch (const std::exception& error) {
            caught = std::string(error.what()) == "task failed";
        }
        assert(caught);
        assert(good == 10);
    }

    // 4. Backpressure: submit() blocks when the bounded queue is full, and
    //    everything still completes once the workers catch up.
    {
        // A slow first task holds the workers; the queue capacity is one,
        // so the producer must wait.
        std::atomic<bool> release{false};
        std::atomic<int> done{0};

        auto pool = make_thread_pool(2, 1);
        pool->submit([&release] {
            while (!release) std::this_thread::yield();
        });
        for (int i = 0; i < 20; ++i) {
            pool->submit([&done] { ++done; });
        }
        release = true;
        pool->wait_idle();
        assert(done == 20);
    }

    // 5. Clean shutdown: the destructor joins every worker.
    {
        std::atomic<int> done{0};
        {
            auto pool = make_thread_pool(4, 16);
            for (int i = 0; i < 100; ++i) {
                pool->submit([&done] { ++done; });
            }
            // Destroyed without wait_idle(): the destructor drains the
            // queue before joining.
        }
        assert(done == 100);
    }

    // 6. An unreasonable worker count is rejected.
    {
        bool caught = false;
        try {
            ThreadPool huge(5000, 10);
        } catch (const UatcError&) {
            caught = true;
        }
        assert(caught);
    }

    std::printf("thread_pool_test: PASS\n");
    return 0;
}
