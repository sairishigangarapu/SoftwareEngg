# Quality Report — Days 18-22 (TC-PAR-02, coverage)

**Branch**: `team-day-18-22`

## ThreadSanitizer (TC-PAR-02)

| Test | Build | Result |
|------|-------|--------|
| tests/core/thread_pool_test.cpp | `-fsanitize=thread` (ASLR disabled via `setarch -R`) | **PASS, no reports** |
| tests/core/parallel_test.cpp | `-fsanitize=thread` (ASLR disabled) | **PASS, no reports** |

The pool's bounded queue, the backpressure, the wait_idle barrier, the
exception capture and the parallel block pipeline all run clean under TSan.
One earlier hang (the queue slot freed after the task ran, not at dequeue)
was found and fixed on Day 14; TSan confirms the fix.

## Line coverage (gcov, the core layer only)

The 12 core tests (block slicing, transforms, codecs, probe, selector,
CRC32, SHA-256, compression, path safety, planner, Huffman, parallel) linked
against instrumented objects; the coverage merges across runs.

| Module | Lines | Coverage |
|--------|-------|----------|
| crc32.cpp | 17 | 100.00% |
| block_stats.cpp | 17 | 100.00% |
| compressibility_probe.cpp | 19 | 100.00% |
| cost_estimator.cpp | 23 | 100.00% |
| path_safety.cpp | 17 | 100.00% |
| registry.hpp | 28 | 96.43% |
| huffman_coder.cpp | 131 | 96.18% |
| sha256.cpp | 104 | 95.19% |
| byte_shuffle_transform.cpp | 38 | 94.74% |
| delta_transform.cpp / xor_transform.cpp | 35 each | 94.29% |
| block_slicer.cpp | 28 | 92.86% |
| planner.cpp | 71 | 91.55% |
| baseline_entropy_coder.cpp | 55 | 90.91% |
| thread_pool.cpp | 71 | 88.73% |
| compressor.cpp | 93 | 87.10% |
| rle_transform.cpp | 38 | 86.84% |
| transform_selector.cpp | 30 | 80.00% |
| error.cpp | 48 | 54.17% (the error-name/exit-code table is exercised via the mapping tests) |
| **CORE TOTAL** | **1 803** | **91.7%** |

The STP target is 80% lines on core modules - met.

## Full-corpus round trip (TC-NFR-01)

Every benchmark record carries a round-trip check (`uatc benchmark`), and
the parallel determinism test asserts byte-identical archives at 1, 2 and 8
workers. The demo script verifies SHA-256 equality for every extracted file.
