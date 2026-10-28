# UATC-035: Day 13 - Thread pool implementation

**Milestone**: Milestone 3 (Days 11-17) - Hardening and Parallelism
**Assignee**: Sai Rishi Gangarapu
**Priority**: P0
**Requirements**: FR-10 (the implementation half of UATC-028)

- [x] Bounded-queue thread pool behind the core's own Executor interface; the core does not depend on the pcomp scaffold
- [x] submit() applies backpressure; wait_idle() is a reusable barrier
- [x] A task that throws is captured and rethrown at wait_idle(); the pool keeps working
- [x] The destructor drains the queue and joins every worker
- [x] One concurrency bug found and fixed under load: a worker freed a queue slot only after the task had finished running, not when it was dequeued. With one worker pinned by a long task and a queue capacity of one, the producer could sleep next to a queue it believed was full while the worker slept believing there was no work. The slot is now freed and notified at dequeue time, with the lock held

**Acceptance (TC-PAR-03)**: 2000 tasks each run exactly once; an exception is reported; clean shutdown; no deadlock.

**GitHub**: #48
