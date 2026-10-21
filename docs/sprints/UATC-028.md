# UATC-028: Implement the thread pool - FR-10 parallel block execution

**Sprint**: Sprint 8 (Parallelism) - continues from pcomp Day 6  
**Assignee**: Sampada K  
**Priority**: P0  
**Requirements**: FR-10, NFR-02  
**GitHub**: #40

## Description

Implement the thread pool so that independent blocks are processed
concurrently and the resulting archive is identical regardless of the worker
count.

## Background

This is the last major unimplemented requirement.

- `pcomp/include/pc/thread_pool.hpp` declares `make_thread_pool(workers, queue_capacity)` but there is no implementation anywhere in the repo
- `kParallelPathSafe = false`, so the benchmark records every run with more than one worker as `skipped_parallel_not_implemented`
- FR-10 (process independent blocks concurrently and produce an identical archive regardless of worker count) is therefore NOT satisfied

## Tasks

- [ ] Implement `make_thread_pool` on top of the `Executor` interface: a bounded queue, N workers and backpressure when the queue fills
- [ ] Wire the block loop to the pool: the producer assigns `seq`, the workers process blocks, and a single writer emits results in `seq` order through the `ReorderBuffer`
- [ ] Honour the rules in `pcomp/docs/ORDERING.md`: file order by relative path, block order by offset, output strictly by `seq`, and codec purity
- [ ] Only flip `kParallelPathSafe = true` once the parallel output has been verified byte-identical to the serial output
- [ ] Add a test that compresses the same input with 1 worker and with N workers and compares the SHA-256 of both archives (TC-PAR-01)
- [ ] Run the suite under ThreadSanitizer with no data races reported (TC-PAR-02)
- [ ] Add the `--workers N` option to the CLI once the pool is in place

## Acceptance Criteria

- `pc_bench --workers 1,2,4,max` produces real measurements for N > 1 instead of `skipped_parallel_not_implemented`
- Archives produced with 1 worker and with N workers are byte-identical
- No data races under ThreadSanitizer; no deadlock or leak on shutdown
