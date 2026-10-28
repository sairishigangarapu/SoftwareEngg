# UATC Sprint Plan — Milestone Based

**Author**: Sai Rishi Gangarapu (Team Lead)  
**Scope**: Compression core + adaptive transform engine + archive hardening  
**Shape**: three milestone sprints (Days 1-5, 6-10, 11-17)  
**Rebuilt**: Day 17, after the previous plan directory was removed from the repository

## Milestones

| Milestone | Days | SRS phase | Goal | Status |
|-----------|------|-----------|------|--------|
| **1 — MVP Compressor** | 1-5 | Phase 2 | A working lossless compressor: any file in, the same bytes out | Complete |
| **2 — Adaptive Engine** | 6-10 | Phase 3 | The full v1 transform set with automatic per-block selection | Complete |
| **3 — Hardening & Parallelism** | 11-17 | Phases 4-5 | CRC32 + SHA-256 integrity, the thread pool (FR-10), the benchmark, course tooling | Complete |

Each milestone is judged by a **demonstrable outcome**, not a task list.

---

# Sprint 1 — Milestone 1: MVP Compressor (Days 1-5)

**Goal**: a lossless compressor that turns any file into an archive and back,
byte for byte, with the data checked at every step.

**Exit criteria**

- [x] Split into fixed-size blocks, reassemble exactly
- [x] Block statistics available to the compressor
- [x] Transform interface and round-trip; TransformRegistry adds transforms without touching the compressor
- [x] Baseline entropy coder behind a Codec interface, round-trips
- [x] Archive written and read back, byte-identical
- [x] Every block checksummed, corrupted archive rejected
- [x] Transform and codec ids match the SAD

**Issues**: UATC-001 to UATC-017 (17, all done). Cards in history (commit
`e1f4b5d`); the GitHub issues #7–#22, #23 remain the record.

---

# Sprint 2 — Milestone 2: Adaptive Engine (Days 6-10)

**Goal**: complete the v1 transform set, choose per block automatically, and
make the whole thing demonstrable.

**Exit criteria**

- [x] All five v1 transforms round-trip and register through a factory
- [x] The compressibility probe rejects incompressible data before any work (FR-05)
- [x] The selector picks the cheapest transform, stores the block when nothing beats raw (FR-06, FR-08), ties to the lowest id (ADR-05)
- [x] Transform ids aligned with the SAD; the archive layer accepts everything the core registers
- [x] CLI compress/extract with before, during and after reporting; demo script runs green

**Issues**: UATC-018 to UATC-027 (10, all done) plus UATC-031 (the demo
integration, done) — GitHub #25–#34, #44.

---

# Sprint 3 — Milestone 3: Hardening & Parallelism (Days 11-17)

**Goal**: make the archive match the SAD's integrity specification, make the
parallel path real, measure the compressor honestly, and add course tooling.

**Exit criteria**

- [x] Per-block checksums are CRC32 (IEEE 802.3), verified against the standard vectors
- [x] A SHA-256 per file and per archive, with the integrity footer from SAD 4.1; corrupting a payload reports the block index, corrupting the archive checksum reports E_ARCHIVE_CHECKSUM (TC-INT-01/02)
- [x] The thread pool exists, is bounded, survives task exceptions, and shuts down cleanly (TC-PAR-03)
- [x] Parallel block pipeline: byte-identical archives for 1 and 8 workers (TC-PAR-01, FR-10); serial path below 1 MiB (TC-PAR-05); --workers N in the CLI
- [x] Path traversal: one shared rule across the CLI, extraction and the future API; nothing written outside the destination (TC-SEC-01, closes UATC-030)
- [x] uatc benchmark with a gzip comparison and a round-trip check in every record (NFR-02/07)
- [x] The cost estimate is a byte-exact model of the coder (100/100 trials), verified end to end with the earlier heuristic regression (UATC-032, #45) closed
- [x] Dockerfile and Jenkinsfile per the course tooling list
- [x] Updated CLI spec, demo runbook and file guide

**Issues**: UATC-033 to UATC-039 — GitHub #46–#52, all assigned to
sairishigangarapu.

---

# Where things stand after Day 17

- **27 test suites, all passing.** Across the filesystem, archive, transform,
  codec, probe, selector, planner, compression, path-safety, CRC32, SHA-256,
  integrity, thread-pool and parallel-determinism layers.
- **FR-10 (parallel execution with identical output) is implemented and
  verified**: byte-identical archives at 1, 2 and 8 workers.
- **FR-09 (integrity) matches the SAD**: CRC32 per block, SHA-256 per file
  and archive, with the footer.
- The store decision is driven by an **exact** model of the coder, so
  "stored as-is" blocks are a deliberate, verifiable choice.

## The one real concurrency bug, worth remembering

The first thread pool freed a queue slot only after the task finished
running. With one worker pinned by a long task and a queue capacity of one,
the producer could sleep believing the queue was full while the worker slept
believing there was no work — a lost wakeup that only appeared under exactly
that load shape (20 tasks, 2 workers, capacity 1, one spinner; it never
appeared with 5 tasks). The slot is now freed and notified at dequeue time
with the lock held.

## Still open

- The HTTP API layer and the job manager remain unimplemented; the frontend
  runs on mock data against `docs/api-contract.md`.
- Multi-file and directory archives: the reader handles one file per archive.
- Huffman or rANS behind the existing EntropyCoder interface would move text
  compression well beyond the current honest-but-modest ratios.
